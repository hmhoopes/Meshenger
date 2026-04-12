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

from collections import defaultdict
import message_store, time 
import json

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

#Maps each sender name to tuple of sender' pub key, flag indicator of activity, and connected MAC (only if active)
user_tracking: dict[str, tuple[str, bool, str]] = defaultdict(tuple)

#registers a user (if name isn't used, pub_key isn't used)
# returns tuple of (bool, str) where bool is true if registered, false if not and str is an error message if registration fails
def register_user(name: str, pub_key: str, mac_addr: str):
    if name in user_tracking:
        print(f"Name {name} already registered")
        return (False, f"Name {name} already registered")
    for user_info in user_tracking.values():
        if user_info and user_info[0] == pub_key:
            print(f"Public key {pub_key} already registered")
            return (False, f"Public key {pub_key} already registered")

    #replace any entries' mac address in user_tracking 
    broadcast_mac = "ff:ff:ff:ff:ff:ff"
    for user, (key, active, addr) in user_tracking.items():
        if addr == mac_addr:
            user_tracking[user] = (key, False, broadcast_mac)

    user_tracking[name] = (pub_key, True, mac_addr)
    print(f"User {name} registered successfully with public key {pub_key} and MAC {mac_addr}")

    return (True, f"User {name} registered successfully")

#signs in a user (if name is registered, pub_key matches, and not already signed in)
# returns tuple of (bool, str) where bool is true if signed in, false if not and str is an error message if sign-in fails
def sign_in_user(name: str, pub_key: str, mac_addr: str):
    if name not in user_tracking:
        print(f"Name {name} not registered")
        return (False, f"Name {name} not registered")
    user_info = user_tracking[name]
    if user_info[0] != pub_key:
        print(f"Public key {pub_key} does not match registered key for {name}")
        return (False, f"Public key {pub_key} does not match registered key for {name}")
    if user_info[1]:
        print(f"User {name} already signed in")
        return (False, f"User {name} already signed in")

    #replace any entries' mac address in user_tracking 
    broadcast_mac = "ff:ff:ff:ff:ff:ff"
    for user, (key, active, addr) in user_tracking.items():
        if addr == mac_addr:
            user_tracking[user] = (key, False, broadcast_mac)

    user_tracking[name] = (pub_key, True, mac_addr)
    print(f"User {name} signed in successfully with MAC {mac_addr}")
    return (True, f"User {name} signed in successfully")

def get_user_json(name: str) -> str:
    if name not in user_tracking:
        return None
    pubkey, active, mac = user_tracking[name]
    return json.dumps({"name": name, "pubkey": pubkey, "active": active, "mac": mac})