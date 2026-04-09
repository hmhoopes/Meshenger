## Mesh Node
Directory for ESP 32 Nodes in Mesh

### Files
`node.ino`:
- arduino file for the board
- configure `Sender` to 0 for it to be a receiver or 1 for it to be a sender

`Helpers.h`
- header containing useful abstractions for ESP Now

### Current Setup
1. Configure one board as Sender
2. Unplug sender board from serial and provide separate power supply to sender board
3. Plug in another board and configure as Receiver

Should see the messages on serial now.