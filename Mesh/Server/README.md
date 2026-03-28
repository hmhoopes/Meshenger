## Mesh Server
Directory for Raspberry Pi server in Mesh


message store is a simple storage mechanism for messages received over serial

serial_pipe is a pipe that receives serial input over the tty from the esp connected to the pi, that then pipes this message into the storage defined in message store. serial pipe also contains code that pipes messages back into the esp over serial. 

right now serial_pipe assumes input as mac_addr:mess over the serial input line between the server and the esp32 -- it currently outputs 