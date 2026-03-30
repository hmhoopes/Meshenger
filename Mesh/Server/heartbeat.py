"""
Project: Meshenger
Module Name: heartbeat.py
Description:
    sends out a request for presence to known ESP's on the network, processes heartbeat responses 
    flag: requires ESP 32 rewrite to message handle heartbeat messages and respond appropriately
Inputs:
    - serial input 
Outputs:
    - 
External Sources:
    - why does this matter idk 
Author: Omar Mohammed
Creation Date: 03/29/2026
"""

import serial_pipe, message_store, time 

#two minutes from now 
timeout = time.time() + 60*2

#receives a mac_addr, if this mac_addr is not resent in a certain pre-defined time frame then does not 
#honestly this definitely needs to be rewritten for ping timing, speed, resource and quite a bit but the gist is there 
def heart_ping(mac_addr):
    while time.time() < timeout:
        ping = read_serial_mac() #rewrite this based on the way things are handled on input to return the mac_addr
        if ping == mac_addr:
            print(f"{mac_addr} alive!")
            break



