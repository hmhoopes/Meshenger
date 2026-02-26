#### Diagram Summary
- We would only do encryption / auth between "pagers" and the pi
- The nodes would do the basic broadcast we talked about (drop if seen before, otherwise send to all peers) and then the "pagers" would interact with the server to get list of all users, get list of active users, send messages to server, receive offline messages
- Server would track if messages were sent to offline users / went unacked for too long and would add them to offline queue 
- The nodes wouldn't need to be authed since they only repeat messages / messages they see would be encrypted