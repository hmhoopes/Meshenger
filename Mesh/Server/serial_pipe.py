"""
Project: Meshenger
Module Name: serial_pipe.py
Description:
    controls serial communication in and out of the server rasp_pi, f
Inputs:
    - serial info from ESP-32 over tty 
Outputs:
    - updates internal message store 
External Sources:
    - why does this matter idk 
Author: Omar Mohammed
Creation Date: 03/28/2026
"""

#self explanatory 
import serial
from message_store import store_message

#defines static serial port and baud rate 
#flag: serial port may not be assigned by server as static -- likely requires either manually changing this definition or forcing tty definition on server
SERIAL_PORT = '/dev/ttyUSB0'
BAUD_RATE = 115200

#this naturally assumes that messages are formatted in MAC:mess, parses assuming this, stores
def parse_and_store(line: str):
    if ':' in line:
        mac, message = line.split(':', 1)
        store_message(mac.strip(), message.strip())
        #test clarity line, flag:removal after fixing
        print(f"Stored from {mac.strip()}: {message.strip()}")
    else:
        #test clarity line, flag: removal after fixing 
        print(f"Unrecognized format, discarding: {line}")

#sends back a message over serial 
def send_message(ser: serial.Serial, mac_addr: str, message: str):
    line = f"{mac_addr}:{message}\n"
    ser.write(line.encode('utf-8'))
    print(f"Sent to {mac_addr}: {message}")
    
#reads from serial 
def read_serial():
    with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1) as ser:
        print(f"Listening on {SERIAL_PORT}...")
        while True:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                #test clarity line, flag:removal after fixing
                print(f"Raw received: {line}")
                parse_and_store(line)

def send_message(ser: serial.Serial, mac_addr: str, message: str):
    line = f"{mac_addr}:{message}\n"
    ser.write(line.encode('utf-8'))
    print(f"Sent to {mac_addr}: {message}")


#this naturally assumes that messages are formatted in MAC:mess, parses assuming this, stores
def parse_and_store(line: str):
    if ':' in line:
        mac, message = line.split(':', 1)
        store_message(mac.strip(), message.strip())
        #test clarity line, flag:removal after fixing
        print(f"Stored from {mac.strip()}: {message.strip()}")
    else:
        #test clarity line, flag: removal after fixing 
        print(f"Unrecognized format, discarding: {line}")