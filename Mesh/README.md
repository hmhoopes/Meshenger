# Mesh Module

## Overview

The Mesh module provides the core functionality for ESP-NOW-based mesh networking in the Meshenger project. It includes utilities for MAC address handling, message structures and transmission, peer management, and helper functions for initialization and communication. This enables secure, decentralized messaging across ESP32 devices.

## Files

### MAC.hpp

This file defines the `MAC` class, a lightweight wrapper for 6-byte MAC addresses. It provides methods for formatting MAC addresses as strings (both Arduino String and std::string), accessing raw byte arrays, and comparison operators. Helper functions include `GetMACAddress()` to retrieve the device's station MAC, and `GetSenderMAC()`/`GetDestinationMAC()` to extract MACs from ESP-NOW receive info.

Key functionalities:
- MAC address storage and manipulation.
- String formatting for logging and BLE naming.
- Retrieval of local and packet MAC addresses.

### Message.hpp

This file defines the `Message` struct, which encapsulates mesh messages with a type, ID, and fixed-size payload buffer. It includes the `MessageType` enum for categorizing messages (e.g., Discovery, Text, ACK). Send functions are provided: `SendMessage()` for basic transmission and `SendMessageWithRetry()` for reliable delivery with ACK/retry logic using timeouts and retry limits.

Key functionalities:
- Message structure and formatting helpers.
- ESP-NOW message sending with and without retry/ACK handling.
- Global state for tracking ACKs.

### Peer.hpp

This file defines the `Peer` class, which abstracts ESP-NOW peer information. It stores a MAC address and manages the `esp_now_peer_info_t` structure for adding peers to the ESP-NOW network. Methods include adding peers, checking addition status, and accessing the MAC.

Key functionalities:
- Peer representation and ESP-NOW peer management.
- Addition of peers to the network with error handling.

### Helpers.hpp

This file contains utility functions for ESP-NOW initialization, message handling, peer discovery, and pager mode operations. It includes setup routines like `InitializeESPNow()` and `RegisterListen()`, handlers for different message types (e.g., `HandleDiscovery()`, `HandleText()`), and functions for sending text messages and managing the peer list. It also supports pager mode for forwarding messages to BLE.

Key functionalities:
- ESP-NOW setup and peer discovery.
- Message reception and dispatching.
- Text message transmission with chunking.
- Peer list management and JSON serialization for BLE.
- Integration with pager mode for BLE bridging.

### Connection & Message

#### Connection tracking
- Each pager tracks vector of connections
- Each connection tracks:
    - nickname of pager?
    - MAC of connected pager
    - DH crypto info
    - last acked message
    - last sent message
    - functions for checking connection:
        - get nickname ?
        - bool to check if still valid (if under message limit)
        - get remaining allowed messages
- add global funcs to check if target is a connection, get idx of target in connection vec 

- Message tracks:
    - MAC of target
    - type
    - id for tracking message
    - split id for tracking split messages

- Process for sending messages
1. call `bool EstablishConnection(target)`
    - authenticates target and Sets up crypto keys
    - adds target to the connection vector
    - overwrites connection if it is in vector already
    - returns true if successful, false if fails
2. call `bool SendToConnection(target)`
    - encrypts message
    - sets id
    - calls `SendText...` to send to target 
    - returns true if successful, false if not
      - fails if target not a connection / hit message limit