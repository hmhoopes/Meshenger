"""
Project: Meshenger
Module Name: message_store.py
Description:
    Maintains a store of received text messages keyed by sender MAC address.
    Provides store and retrieval helpers used by the mesh receive handlers.
Inputs:
    - MAC addresses, message strings
Outputs:
    - None, just updates the internal message store
External Sources:
    - Python documentation, geeksforgeeks
Author: Team 2
Creation Date: 03/15/2026
"""

from collections import defaultdict

#Maps each sender MAC address string to the ordered list of messages received from that peer.
message_store: dict[str, list[str]] = defaultdict(list)

#Record incoming message and store under sender MAC address. Make a new entry if message is from a new peer.
def store_message(mac_addr: str, message: str):
    message_store[mac_addr].append(message)

#Return all messages from a given MAC address. Return None if no messages exist from that peer.
def get_messages(mac_addr: str):
    if mac_addr in message_store:
        return message_store[mac_addr]
    return None

#Return the full message store, with all MAC addresses and their attached message lists
def get_all_messages():
    return dict(message_store)
