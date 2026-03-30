#!/usr/bin/env python3
"""
Meshenger serial monitor TUI
Split-screen: serial output (top) + message input (bottom).
Serial output never interrupts typing.

Usage:  python3 monitor.py [PORT [BAUD]]
        python3 monitor.py /dev/ttyUSB0 115200   (defaults)

Keys:
  Enter       Send typed message
  PgUp/PgDn  Scroll output
  Up/Down     Scroll one line
  End         Jump to latest output
  Ctrl-C      Quit
"""

import curses
import os
import sys
import termios
import threading
import time


BAUD_MAP = {
    9600:   termios.B9600,
    19200:  termios.B19200,
    57600:  termios.B57600,
    115200: termios.B115200,
}


def open_serial(port, baud):
    """Open and configure a serial port for raw 8N1 I/O."""
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY)
    attrs = termios.tcgetattr(fd)

    rate = BAUD_MAP.get(baud, termios.B115200)
    attrs[4] = rate  # ispeed
    attrs[5] = rate  # ospeed

    # Input: disable all special processing
    attrs[0] &= ~(termios.IGNBRK | termios.BRKINT | termios.PARMRK |
                  termios.ISTRIP | termios.INLCR | termios.IGNCR |
                  termios.ICRNL | termios.IXON)
    # Output: raw
    attrs[1] &= ~termios.OPOST
    # Control: 8N1, receiver on, ignore modem control
    attrs[2] &= ~(termios.CSIZE | termios.PARENB | termios.CSTOPB)
    attrs[2] |= termios.CS8 | termios.CREAD | termios.CLOCAL
    # Local: fully raw (no echo, canonical, signal chars)
    attrs[3] &= ~(termios.ECHO | termios.ECHONL | termios.ICANON |
                  termios.ISIG | termios.IEXTEN)

    attrs[6][termios.VMIN] = 1   # block until at least 1 byte
    attrs[6][termios.VTIME] = 0

    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    return fd


def main(stdscr, port, baud):
    try:
        fd = open_serial(port, baud)
    except Exception as e:
        curses.endwin()
        print(f"Cannot open {port}: {e}", file=sys.stderr)
        sys.exit(1)

    # lines: list of (text, style)  style ∈ 'rx' | 'tx' | 'sys'
    lines = []
    lock = threading.Lock()
    stop = threading.Event()
    MAX_LINES = 5000

    def add_line(text, style="rx"):
        with lock:
            lines.append((text, style))
            if len(lines) > MAX_LINES:
                lines.pop(0)

    def reader():
        buf = b""
        while not stop.is_set():
            try:
                chunk = os.read(fd, 256)
                if not chunk:
                    continue
                buf += chunk
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    add_line(line.decode("utf-8", errors="replace").rstrip("\r").replace("\x00", ""))
            except OSError:
                break

    threading.Thread(target=reader, daemon=True).start()

    # curses setup
    curses.curs_set(1)
    curses.noecho()
    stdscr.nodelay(True)
    stdscr.keypad(True)

    if curses.has_colors():
        curses.start_color()
        curses.use_default_colors()
        curses.init_pair(1, curses.COLOR_GREEN, -1)   # tx (sent by us)
        curses.init_pair(2, curses.COLOR_YELLOW, -1)  # sys
        curses.init_pair(3, curses.COLOR_CYAN, -1)    # rx (from device)

    input_buf = ""
    scroll = 0        # lines scrolled up from bottom; 0 = latest
    auto_scroll = True
    prev_total = 0

    while True:
        h, w = stdscr.getmaxyx()
        # Layout:  row 0 = title, rows 1..h-3 = output, row h-2 = divider, row h-1 = input
        out_rows = h - 3
        divider_row = h - 2
        input_row = h - 1

        stdscr.erase()

        # ── Title bar ─────────────────────────────────────────────────────────
        scroll_tag = f"  [↑{scroll} lines]" if scroll > 0 else ""
        title = f" Meshenger | {port} @ {baud}{scroll_tag} | PgUp/Dn · End=latest · Ctrl-C=quit "
        stdscr.attron(curses.A_REVERSE)
        try:
            stdscr.addstr(0, 0, title[:w - 1].ljust(w - 1))
        except curses.error:
            pass
        stdscr.attroff(curses.A_REVERSE)

        # ── Output pane ───────────────────────────────────────────────────────
        with lock:
            snapshot = list(lines)

        total = len(snapshot)
        if auto_scroll and total != prev_total:
            scroll = 0
        prev_total = total

        end = total - scroll
        start = max(0, end - out_rows)
        visible = snapshot[start:end]

        for i, (text, style) in enumerate(visible):
            row = 1 + i
            if row >= divider_row:
                break
            attr = curses.A_NORMAL
            if curses.has_colors():
                attr = curses.color_pair({"tx": 1, "sys": 2, "rx": 3}.get(style, 0))
            try:
                stdscr.addstr(row, 0, text[: w - 1], attr)
            except curses.error:
                pass

        # ── Divider ───────────────────────────────────────────────────────────
        stdscr.attron(curses.A_REVERSE)
        try:
            stdscr.addstr(divider_row, 0, " " * (w - 1))
        except curses.error:
            pass
        stdscr.attroff(curses.A_REVERSE)

        # ── Input line ────────────────────────────────────────────────────────
        prompt = "> "
        max_disp = w - len(prompt) - 1
        display = input_buf[-max_disp:] if len(input_buf) > max_disp else input_buf
        try:
            stdscr.addstr(input_row, 0, prompt + display)
            stdscr.move(input_row, len(prompt) + len(display))
        except curses.error:
            pass

        stdscr.refresh()

        # ── Key handling ──────────────────────────────────────────────────────
        try:
            ch = stdscr.getch()
        except curses.error:
            ch = -1

        if ch == -1:
            time.sleep(0.05)
            continue

        if ch in (curses.KEY_ENTER, 10, 13):
            msg = input_buf.strip()
            if msg:
                try:
                    os.write(fd, (msg + "\n").encode())
                    add_line(f"> {msg}", "tx")
                except OSError as e:
                    add_line(f"[write error: {e}]", "sys")
            input_buf = ""
            scroll = 0
            auto_scroll = True

        elif ch in (curses.KEY_BACKSPACE, 127, 8):
            input_buf = input_buf[:-1]

        elif ch == curses.KEY_PPAGE:  # Page Up
            auto_scroll = False
            scroll = min(scroll + max(1, out_rows // 2), max(0, total - out_rows))

        elif ch == curses.KEY_NPAGE:  # Page Down
            scroll = max(0, scroll - max(1, out_rows // 2))
            if scroll == 0:
                auto_scroll = True

        elif ch == curses.KEY_UP:
            auto_scroll = False
            scroll = min(scroll + 1, max(0, total - out_rows))

        elif ch == curses.KEY_DOWN:
            scroll = max(0, scroll - 1)
            if scroll == 0:
                auto_scroll = True

        elif ch == curses.KEY_END:
            scroll = 0
            auto_scroll = True

        elif ch == curses.KEY_RESIZE:
            pass  # getmaxyx() picks up the new size next iteration

        elif 32 <= ch <= 126:
            input_buf += chr(ch)

    stop.set()
    try:
        os.close(fd)
    except OSError:
        pass


if __name__ == "__main__":
    _port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyUSB0"
    _baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200
    try:
        curses.wrapper(lambda s: main(s, _port, _baud))
    except KeyboardInterrupt:
        pass
