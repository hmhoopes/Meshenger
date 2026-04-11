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
import json
import serial
from message_store import store_message
import user_tracking

#defines static serial port and baud rate 
#flag: serial port may not be assigned by server as static -- likely requires either manually changing this definition or forcing tty definition on server
SERIAL_PORT = '/dev/ttyUSB0'
BAUD_RATE = 115200

MAX_ESP_PAYLOAD_LENGTH = 229
# 1 byte for message indicator + 12 bytes for sender name + 12 bytes for target name
MESSAGE_OVERHEAD = 1 + 12 + 12
MAX_MESSAGE_LENGTH = MAX_ESP_PAYLOAD_LENGTH - MESSAGE_OVERHEAD

server_name = "ServerPi"

#sends back a message over serial 
#format is MSG:target-mac|content, where content's format is 1 char for indicator, 12 chars for sender name, 12 chars for recipient name, message body
def send_message(ser: serial.Serial, mac_addr: str, indicator: str, target_name: str, message: str):
    if len(message) > MAX_MESSAGE_LENGTH:
        print(f"Message too long to send, truncating to {MAX_MESSAGE_LENGTH} chars")
        message = message[:MAX_MESSAGE_LENGTH]
    if len(indicator) != 1:
        print("Indicator must be exactly 1 char, truncating")
        indicator = indicator[:1]
    if len(target_name) > 12:
        print("Target name too long, truncating to 12 chars")
        target_name = target_name[:12]
    target_name_padded = target_name.ljust(12, '\x01')[:12] # pad with \x01 and truncate to 12 chars
    server_name_padded = server_name.ljust(12, '\x01')[:12] # pad with \x01 and truncate to 12 chars

    line = f"MSG:{mac_addr}{indicator+server_name_padded+target_name_padded+message}\x1E" # \x1E is the end-of-stream delimiter
    ser.write(line.encode('utf-8'))
    print(f"Sent the following line: |{line}|")

#reads till we hit stop symbol '\x1E'
def read_till_stop(serial: serial.Serial):
    buffer = b''
    while True:
        byte = serial.read(1)
        if byte == b'\x1E':
            break
        buffer += byte
    serial.reset_input_buffer()
    return buffer.decode('utf-8', errors='ignore').strip()
    
#reads from serial 
def read_serial():
    with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1) as ser:
        print(f"Listening on {SERIAL_PORT}...")
        while True:
            line = read_till_stop(ser)
            if line:
                #test clarity line, flag:removal after fixing
                print(f"Raw received: {line}")
                parse_and_store(ser, line)

#this naturally assumes that messages are formatted in MAC:mess, parses assuming this, stores
''' input formats:
    - Message: MSG:dst_mac|src_mac|type|id|content
        - content format: 1 char for indicator, 12 chars for sender name, 12 chars for target name, message body
'''
def parse_and_store(ser: serial.Serial, line: str):
    if ':' in line:
        type, message = line.split(':', 1)
        type = type.strip()
        message = message.strip()
        if type == "MSG":
            interpret_message(ser,message)
        else:
            #test clarity line, flag:removal after fixing
            print(f"Unrecognized type '{type}', discarding: {line}")
    else:
        #test clarity line, flag: removal after fixing 
        print(f"Unrecognized format, discarding: {line}")

def interpret_message(ser: serial.Serial, message: str):
    parts = message.split('|', 4)
    if len(parts) != 5:
        #test clarity line, flag:removal after fixing
        print(f"Invalid message format, discarding: {message}")
        return
    dst_mac, src_mac, msg_type, msg_id, content = parts
    dst_mac = dst_mac.strip()
    src_mac = src_mac.strip()
    msg_type = msg_type.strip()
    msg_id = msg_id.strip()
    content = content.strip()
    print(f"Parsed message - DST MAC: {dst_mac}, SRC MAC: {src_mac}, Type: {msg_type}, ID: {msg_id}, Content: {content}")
    #split content into indicator, sender name, recipient name, and message body
    if len(content) < 25: # 1 char for indicator + 12 chars for sender name + 12 chars for recipient name
        #test clarity line, flag:removal after fixing
        print(f"Content too short, discarding: {message}")
        return
    indicator = content[0]
    sender_name = content[1:13].replace('\x01', '').strip() # remove padding and trim
    recipient_name = content[13:25].replace('\x01', '').strip() # remove padding and trim
    message_body = content[25:].replace('\x01', '').strip()

    if indicator == 'm':  # message indicator
        print(f"Parsed message - Sender: {sender_name}, Recipient: {recipient_name}, Body: {message_body}")
        store_message(sender_name, message_body)
        send_message(ser, src_mac, 'm', sender_name, message_body)  # forward message to recipient
    elif indicator == 'h':  # heartbeat indicator
        #TODO
        print(f"TODO: add heartbeat handling")
    elif indicator == 's':  # sign in indicator
        key_parts = message_body.split('|', 1)
        if len(key_parts) != 2:
            print(f"Invalid Sign In message format, discarding: {message}")
            return
        length_str, key_str = key_parts
        length = int(length_str.strip())
        key_str = key_str[:length].strip()
        print(f"Parsed Sign In - Sender: {sender_name}, Key: {key_str}")

        print(f"Attempting to sign in user {sender_name} with public key {key_str} and MAC {src_mac} ...")
        success, msg = user_tracking.sign_in_user(sender_name, key_str, src_mac)
        print(f"Sign In result for {sender_name}: {'Success' if success else 'Failure'}, Message: {msg}")
        msg = ('1' if success else '0') + msg
        send_message(ser, src_mac, 's', sender_name, msg)  # send back sign-in result
    elif indicator == 'r':  # register indicator
        key_parts = message_body.split('|', 1)
        if len(key_parts) != 2:
            print(f"Invalid registration message format, discarding: {message}")
            return
        length_str, key_str = key_parts
        length = int(length_str.strip())
        key_str = key_str[:length].strip()
        print(f"Parsed registration - Sender: {sender_name}, Key: {key_str}")

        print(f"Attempting to register user {sender_name} with public key {key_str} and MAC {src_mac} ...")
        success, msg = user_tracking.register_user(sender_name, key_str, src_mac)
        print(f"Registration result for {sender_name}: {'Success' if success else 'Failure'}, Message: {msg}")
        msg = ('1' if success else '0') + msg
        send_message(ser, src_mac, 'r', sender_name, msg)  # send back registration result
    elif indicator == 'l':  # user list request indicator
        print(f"Received user list request from {sender_name}, sending user list...")
        for user in user_tracking.user_tracking:
            print(f"\tUser: {user}, Info: {user_tracking.user_tracking[user]}")
            user_entry_str = user_tracking.get_user_json(user)
            print(f"\tUser entry string: {user_entry_str}, length: {len(user_entry_str)}")
            send_message(ser, src_mac, 'l', sender_name, user_entry_str)

    elif indicator == 'g':  # get messages request indicator
        #TODO
        print(f"TODO: add get messages handling")
    else:
        #test clarity line, flag:removal after fixing
        print(f"Unrecognized message type '{indicator}', discarding: {message}")
        return

read_serial()