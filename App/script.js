/*
Project: Meshenger
Module Name: script.js
Description:
    Browser-side Web Bluetooth client for the Meshenger mesh messaging system. Connects to the
    Meshenger-Pager ESP32 via Web Bluetooth (Nordic UART Service), enabling users to send and
    receive encrypted messages across the mesh. Handles user authentication (sign-in/registration),
    peer list management, user roster updates, and AES-GCM encryption/decryption using Ed25519 keys
    derived from passwords.
Inputs:
    - Web Bluetooth notifications from NUS TX characteristic (message type prefix + sender + recipient + encrypted content)
    - User interactions: peer selection, message composition, sign-in/registration, peer list refresh
    - Server responses: user list entries, peer activity status, sign-in/registration confirmations
Outputs:
    - Serialized messages via Web Bluetooth NUS RX characteristic (destination MAC + indicator + sender name + target name + encrypted payload)
    - Chat UI updates: message bubbles grouped by peer, online/offline status indicators, user roster
Message Format (BLE → App):
    - 'm' = encrypted message: sourceMac (17 chars) + indicator ('m') + senderName (12 chars padded) + targetName (12 chars padded) + encrypted payload
    - 'l' = peer list response (deprecated)
    - 'h' = heartbeat (pinger handshake)
    - 's' = sign-in response: '1' (success) or '0<error msg>'
    - 'r' = registration response: '1' (success) or '0<error msg>'
    - 'u' = user roster entry: JSON {name, pubkey, mac, active}
Message Format (App → BLE):
    - 'm' + destinationMac (17 chars) + indicator + senderName (12 chars padded) + targetName (12 chars padded) + plaintext (chunked)
Encryption:
    - Uses PBKDF2 (600k iterations) to derive Ed25519 keypair from password
    - Converts Ed25519 → X25519 for ECDH
    - Computes shared AES-GCM key between peers
    - Encrypts messages as "iv.ciphertext" (base64url-encoded)
External Sources:
    - Web Bluetooth API (NUS service UUIDs)
    - @noble/curves (Ed25519, X25519, PBKDF2)
    - Web Crypto API (AES-GCM, PBKDF2)
Author: Team 2
Creation Date: 04/12/2026
*/

console.log("Script loaded, initializing app...");
// --- Nordic UART Service (NUS) ---
// UUIDs must match LocalESP ble_serial.ino so the browser can find and use the service
const NUS_SERVICE = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const NUS_RX = '6e400002-b5a3-f393-e0a9-e50e24dcca9e';  // we write here (app → ESP32)
const NUS_TX = '6e400003-b5a3-f393-e0a9-e50e24dcca9e';  // we subscribe here (ESP32 → app)

// --- DOM references ---
const messagesEl = document.getElementById('messages');
const emptyState = document.getElementById('emptyState');
const compose = document.getElementById('compose');
const form = document.getElementById('form');
const input = document.getElementById('input');
const btnConnect = document.getElementById('btnConnect');
const btnPeerList = document.getElementById('btnPeerList');
const btnSetName = document.getElementById('btnSetName');
const btnGetMessages = document.getElementById('btnGetMessages');
const btnCreateGroup = document.getElementById('btnCreateGroup');
const btnSend = document.getElementById('btnSend');
const statusDot = document.getElementById('statusDot');
const peerStatusDot = document.getElementById('peerStatusDot');
const peerStatusText = document.getElementById('peerStatusText');
const statusText = document.getElementById('statusText');
const macText = document.getElementById('macText');
const usernameText = document.getElementById('usernameText');
const navButtons = document.querySelectorAll('.nav-item');
const peersListEl = document.getElementById('peersList');
const peersView = document.getElementById('view-peers');
const groupsView = document.getElementById('view-groups');
const groupsListEl = document.getElementById('groupsList');
const messagesView = document.getElementById('view-messages');
const settingsView = document.getElementById('view-settings');
const chatPeerLabel = document.getElementById('chatPeerLabel');
const settingSounds = document.getElementById('settingSounds');
const settingBrightness = document.getElementById('settingBrightness');

//format for peers: "name", where name is peer's MAC, "messages", where message is an table array of message information from that peer
//message has "content", which is text content, "sender", a boolean indicating if the message was sent by this user or received from the peer
/*example structure:
peers = [
  {
    name: "evan",
    activity: true, // or false if offline, can be used to show online status in UI
    "messages": [
      {content: "hello", sender: false, route: "direct"},
      {content: "hi there", sender: true, route: "server"}
    ],
  },
*/
let PeerList = [];

// GroupList: array of group chat objects the current user belongs to.
// Populated on login by querying the server for group membership.
// Structure:
// GroupList = [
//   {
//     name: "Team Alpha",          // unique group name (matches server-side registration)
//     members: ["evan", "alice"],  // list of member usernames in this group
//     messages: [                  // chronological list of all messages in this group
//       {
//         content: "hello everyone", // decrypted message text
//         sender: "evan",            // username of who sent it (UserName = you sent it)
//         route: "direct"            // "direct" or "server" — how the message was delivered
//       }
//     ]
//   }
// ]
let GroupList = [];

// MAX message length, used in sending message to ensure sending in esp-now 1 message sized chunks
const MAX_ESP_PAYLOAD_LENGTH = 229;

// message sending fields:
// 1 byte for message indicator + 12 bytes for sender name + 12 bytes for target name
const MESSAGE_FIELDS_OVERHEAD = 1 + 12 + 12;
const MESSAGE_UUID_OVERHEAD = 8; // 8 bytes for uuid when acking
//TODO: update this and corresponding code to allow gcs
const MAX_MESSAGE_LENGTH = MAX_ESP_PAYLOAD_LENGTH - MESSAGE_FIELDS_OVERHEAD;
const MAX_UMESSAGE_LENGTH = MAX_MESSAGE_LENGTH - MESSAGE_UUID_OVERHEAD;
// \x02 prefix + 12 char group name prepended to group message plaintext before encryption
const GROUP_NAME_OVERHEAD = 1 + 12; // \x02 + group name
const MAX_GMESSAGE_LENGTH = MAX_UMESSAGE_LENGTH - GROUP_NAME_OVERHEAD;

let ConnectedDeviceMac = "Unknown";

//Server Name
let ServerName = "ServerPi";
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Will need to update this depending on the device selected as server
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
let ServerMAC = "b0:cb:d8:cc:bf:b0";

//signing in / registering fields
let UserName = "Unknown";
let Keys = null;
let signingIn = false; 
let registering = false; 

// user list from server, with name, public key, and current mac address (if online)
// example structure:
/*UserList = [
  {
    name: "evan",
    publicKey: "abc123",
    mac: "00:11:22:33:44:55", // undefined what this would be if offline
    activity: true, // or false if offline, can be used to show online status in UI
    awaitingUpdate: false,
  },
]*/
let UserList = [];
// stores messages being sent that are awaiting ack
const pendingAcks = new Map();

//increasing delays for ack attempts
const RETRY_DELAYS = [500, 1000, 2000, 4000];

// BLE connection state
let device = null;
let server = null;
let rxChar = null;
let txChar = null;
let rxBuffer = '';  // buffer incomplete lines from TX notifications (split on \n)
let sending = false;  // prevent double-send and duplicate UI messages

// UI state
let currentSection = 'peers';
let currentPeerId = null;

/** Switch between Peers, Messages, and Settings sections */
function setSection(section) {
  currentSection = section;
  navButtons.forEach(btn => {
    btn.classList.toggle('nav-item-active', btn.dataset.section === section);
  });
  peersView.classList.toggle('view-active', section === 'peers');
  messagesView.classList.toggle('view-active', section === 'messages');
  settingsView.classList.toggle('view-active', section === 'settings');
  groupsView.classList.toggle('view-active', section === 'groups');
}

/** Populate the peers list in the Peers view */
function renderPeers() {
  peersListEl.innerHTML = '';
  PeerList.forEach(peer => {
    const li = document.createElement('li');
    const btn = document.createElement('button');
    const div = document.createElement('div');
    const text = document.createElement('span');
    const indicator = document.createElement('span');

    if (peer.name == currentPeerId) {

      peerStatusDot.classList.toggle('connected', peer && peer.activity);
      peerStatusText.textContent = peer && peer.activity ? 'Online' : 'Offline';
    }
    
    indicator.classList.add('status-dot');
    indicator.classList.toggle('connected', peer.activity);

    text.textContent = peer.name;

    div.style.display = 'flex';
    div.style.alignItems = 'center';
    div.style.justifyContent = 'space-between';
    div.style.width = '100%';

    div.appendChild(text);
    div.appendChild(indicator);

    btn.addEventListener('click', () => selectPeer(peer.name));
    console.log('Added peer to list:', peer.name);

    btn.appendChild(div);
    li.appendChild(btn);
    peersListEl.appendChild(li);
  });
}

/** Handle peer selection and open its chat window */
function selectPeer(peerId) {
  if (peerId === UserName) {
    alert('Cannot select yourself as a peer');
    return;
  }

  currentPeerId = peerId;
  const peer = PeerList.find(p => p.name === peerId);
  chatPeerLabel.textContent = peer ? `Chat with ${peer.name}` : 'Chat';
  peerStatusDot.classList.toggle('connected', peer && peer.activity);
  peerStatusText.textContent = peer && peer.activity ? 'Online' : 'Offline';
  // Highlight active peer
  const buttons = peersListEl.querySelectorAll('button');
  buttons.forEach(btn => {
    btn.classList.toggle('active', btn.textContent === (peer.name));
  });
  messagesEl.replaceChildren(); // clear messages when switching peers
  console.log(peer.messages);
  peer.messages.forEach(message => {
    appendMessage(message.content, message.sender, message.route ?? null);
  });
  setSection('messages');
}

// --- Connection & UI helpers ---
/** Update UI and input state when BLE connection changes */
function setConnected(connected) {
  statusDot.classList.toggle('connected', connected);

  statusText.textContent = connected ? 'Connected' : 'Disconnected';

  btnConnect.textContent = connected ? 'Disconnect' : 'Connect to device';
  btnConnect.classList.toggle('connected', connected);

  btnPeerList.classList.toggle('connected', connected);
  btnSetName.classList.toggle('connected', connected);
  btnGetMessages.classList.toggle('connected', connected && UserName !== "Unknown");
  btnCreateGroup.classList.toggle('connected', connected && UserName !== "Unknown");
  
  if (!connected) {
    ConnectedDeviceMac = "Unknown";
    UserName = "Unknown";
    macText.textContent = `Device MAC: ${ConnectedDeviceMac}`;
    usernameText.textContent = `Username: ${UserName}`;
  }

  input.disabled = !connected;
  btnSend.disabled = !connected;

  compose.classList.toggle('disabled', !connected);  // shows/hides Send button
}

/** Add a message bubble (sent or received) and scroll to bottom */
function appendMessage(text, sender, route = null) {
  emptyState.classList.add('hidden');
  const div = document.createElement('div');
  div.className = 'message ' + (sender ? 'sent' : 'received');
  if (route === 'server') div.classList.add('via-server');
  const time = new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
  const routeLabel = `<div class="route">${route === 'server' ? '⚡ via server' : '✓ direct'}</div>`;
  div.innerHTML = `<span class="content">${escapeHtml(text)}</span><div class="time">${time}</div>${routeLabel}`;
  messagesEl.appendChild(div);
  messagesEl.scrollTop = messagesEl.scrollHeight;
}

/** Populate the groups list in the Groups view */
function renderGroups() {
  groupsListEl.innerHTML = '';
  if (GroupList.length === 0) {
    const empty = document.createElement('li');
    empty.style.color = 'var(--text-muted)';
    empty.style.fontSize = '0.85rem';
    empty.style.padding = '0.5rem 0';
    empty.textContent = 'No groups yet.';
    groupsListEl.appendChild(empty);
    return;
  }
  GroupList.forEach(group => {
    const li = document.createElement('li');
    const btn = document.createElement('button');
    const div = document.createElement('div');
    const name = document.createElement('span');
    const memberCount = document.createElement('span');

    name.textContent = group.name;
    memberCount.textContent = `${group.members.length} members`;
    memberCount.style.fontSize = '0.75rem';
    memberCount.style.color = 'var(--text-muted)';

    div.style.display = 'flex';
    div.style.alignItems = 'center';
    div.style.justifyContent = 'space-between';
    div.style.width = '100%';

    div.appendChild(name);
    div.appendChild(memberCount);
    btn.addEventListener('click', () => selectGroup(group.name));
    btn.appendChild(div);
    li.appendChild(btn);
    groupsListEl.appendChild(li);
  });
}

/** Handle group selection and open its chat window */
function selectGroup(groupName) {
  const group = GroupList.find(g => g.name === groupName);
  if (!group) return;

  currentPeerId = groupName;
  chatPeerLabel.textContent = `Group: ${group.name} (${group.members.join(', ')})`;
  peerStatusDot.classList.remove('connected'); // groups don't have a single online status
  peerStatusText.textContent = `${group.members.length} members`;

  messagesEl.replaceChildren();
  group.messages.forEach(message => {
    appendGroupMessage(message.content, message.sender, message.route ?? null);
  });
  setSection('messages');
}

/** Like appendMessage but includes a sender label for group context */
function appendGroupMessage(text, sender, route = null) {
  emptyState.classList.add('hidden');
  const div = document.createElement('div');
  const isMine = sender === UserName;
  div.className = 'message ' + (isMine ? 'sent' : 'received');
  if (route === 'server') div.classList.add('via-server');
  const time = new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
  const routeLabel = `<div class="route">${route === 'server' ? '⚡ via server' : '✓ direct'}</div>`;
  const senderLabel = !isMine ? `<div class="group-sender">${escapeHtml(sender)}</div>` : '';
  div.innerHTML = `${senderLabel}<span class="content">${escapeHtml(text)}</span><div class="time">${time}</div>${routeLabel}`;
  messagesEl.appendChild(div);
  messagesEl.scrollTop = messagesEl.scrollHeight;
}

/** Escape text so it's safe to put in HTML (avoids XSS if ESP32 sends script) */
function escapeHtml(s) {
  const div = document.createElement('div');
  div.textContent = s;
  return div.innerHTML;
}

/** Return true if string looks like displayable text (not garbage/binary from BLE) */
function isDisplayableText(s) {
  if (!s || s.length > 1024) return false;
  let printable = 0;
  for (let i = 0; i < s.length; i++) {
    const c = s.charCodeAt(i);
    if (c === 0xFFFD) return false;
    if ((c >= 0x20 && c <= 0x7E) || c === 9 || c === 10 || c === 13) printable++;
  }
  return printable >= Math.min(s.length, 1);
}

async function EnsureHBResponse() {
  let uuid = getMessageId();
  let delivered = false;
  // attempt direct send 4 times with doubling delay
  for (let attempt = 0; attempt < 4 && !delivered; attempt++) {
    console.log(`Attempt ${attempt + 1} to send chunk ...`);
    try {
      await sendMessageWithAck('h', uuid, "test", ServerName.padEnd(12, '\x01').slice(0, 12), ServerMAC, RETRY_DELAYS[attempt]);
      delivered = true;
    } catch (err) {
      console.warn(`Direct send attempt ${attempt + 1} failed: ${err.message}`);
    }
  }

  if (!delivered){
    console.warn(`HB response was unable to be delivered :(`);
  }
}

// handle incoming message from device
function HandleMessageFromDevice(message) {
  const sourcePeerId = message.slice(0, 17);
  message = message.slice(17); //remove the target peer ID from the message
  console.log("Extracted source peer ID:", sourcePeerId);
  console.log("message after slicing:", message);

  const indicator = message.slice(0, 1);
  message = message.slice(1); // remove the indicator
  console.log("Extracted message indicator:", indicator);
  console.log("message after slicing indicator:", message);

  let senderPeerName = message.slice(0, 12);
  senderPeerName = senderPeerName.replace(/\x01/g, '').trim(); //remove padding and trim
  message = message.slice(12); //remove the sender name from the message
  console.log("Extracted sender name:", senderPeerName);
  console.log("message after slicing sender name:", message);

  let targetPeerName = message.slice(0, 12);
  targetPeerName = targetPeerName.replace(/\x01/g, '').trim(); //remove padding and trim
  message = message.slice(12); //remove the sender name from the message
  console.log("Extracted target name:", targetPeerName);
  console.log("message after slicing target name:", message);

  const lines = message.split(/\r?\n/);
  message = lines[0].trim();
  console.log("Obtained message", message, "ignoring extra lines:", lines.slice(1));

  if (indicator === 'm') {
    let uuid = ''
    if (indicator === 'm'){
      uuid = message.slice(0, 8);
      message = message.slice(8);
      console.log("extracted message uuid: ", uuid);
      console.log("message after slicing indicator:", message);
    }

    if (message && isDisplayableText(message)){
      let userEntry = UserList.find(u => u.name === senderPeerName);
      if (!userEntry){
        alert("Cannot parse message from user ", senderPeerName);
        return;
      } 
      if (uuid === ''){
        alert("received message w/o uuid, cannot parse");
        return;
      }
      
      // reply with ack — send uuid back to sender so they can resolve their pendingAck
      sendMessage('a', uuid, senderPeerName.padEnd(12, '\x01').slice(0, 12), sourcePeerId);

      let deliveredDirect = sourcePeerId !== ServerMAC;
      const route = !deliveredDirect ? 'server' : 'direct';

      //decrypt message
      sharedAesKey(Keys.xPriv, userEntry.publicKey)
        .then(secret => {
          // check for group prefix before decrypting
          if (message.charCodeAt(0) === 0x02) {
            console.log("group message, has gname so removing that and indicator")
            const groupName = message.slice(1, 13).replace(/\x01/g, '').trim();
            const encryptedBody = message.slice(13);
            console.log("encrypted body: ", encryptedBody);
            return decrypt(secret, encryptedBody).then(decrypted => ({ isGroup: true, groupName, decrypted }));
          }
          console.log("nongroup-message, so just decrypting it ", message)
          return decrypt(secret, message).then(decrypted => ({ isGroup: false, decrypted }));
        })
        .then(({ isGroup, groupName, decrypted }) => {
          if (isGroup) {
            const group = GroupList.find(g => g.name === groupName);
            if (group) {
              for (const msg of group.messages) {
                if (msg.uuid === uuid) {
                  console.log("dropping duplicate group message:", decrypted);
                  return;
                }
              }
              group.messages.push({ content: decrypted, sender: senderPeerName, route, uuid });
              if (currentPeerId === groupName) {
                appendGroupMessage(decrypted, senderPeerName, route);
              }
            } else {
              console.warn(`Received group message for unknown group '${groupName}', ignoring`);
            }
            return;
          }

          // normal direct message
          let peer = PeerList.find(p => p.name === senderPeerName);
          if (peer) {
            for (const msg of peer.messages) {
              if (msg.uuid === uuid) {
                console.log("dropping duplicate message:", msg.content);
                return;
              }
            }
            peer.messages.push({ content: decrypted, sender: false, uuid, route });
            if (peer.name === currentPeerId) {
              appendMessage(decrypted, false, route);
            }
          }
        });
    }
  } else if (indicator === 'h') {
    console.log("Received heartbeat, replying ...");
    if (targetPeerName == UserName){
      EnsureHBResponse();
    }
  } else if (indicator === 's') {
    if (!signingIn) {
      console.warn('Received unexpected sign-in message, ignoring');
      return;
    }
    signingIn = false;
    const success = message.slice(0, 1) === '1'; // first char indicates success
    if (success) {
      alert('Sign-in successful!');
      usernameText.textContent = `Username: ${UserName}`;
      btnGetMessages.classList.add('connected');
      btnCreateGroup.classList.add('connected');
    } else {
      alert('Sign-in failed: ' + message.slice(1));
      UserName = "Unknown";
      Keys = null;
    }
  } else if (indicator === 'r') {
    if (!registering) {
      console.warn('Received unexpected registering message, ignoring');
      return;
    }
    registering = false;
    const success = message.slice(0, 1) === '1'; // first char indicates success
    if (success) {
      alert('Registration successful!');
      usernameText.textContent = `Username: ${UserName}`;
      btnGetMessages.classList.add('connected');
      btnCreateGroup.classList.add('connected');
    } else {
      alert('Registration failed: ' + message.slice(1));
      UserName = "Unknown";
      Keys = null;
    }
  } else if (indicator === 'l') {
    console.log("Received user list response:", message);
    let entry = JSON.parse(message);
    if (!entry || Object.keys(entry).length === 0) {
      console.warn('Empty or invalid entry, ignoring');
      return;
    }
    console.log("Parsed user list entry:", entry);
    // convert activity to boolean
    entry.activity = Boolean(entry.active);

    // will use stringToPubKey
    let pubXKey = stringToPubKey(entry.pubkey);
    let existingIndex = UserList.findIndex(u => u.name === entry.name);
    if (existingIndex !== -1) {
      UserList[existingIndex] = {name: entry.name, publicKey: pubXKey, mac: entry.mac, active: entry.activity, awaiting: false};
      let peer = PeerList.find(p => p.name === entry.name);
      if (peer) {
        console.log(`Updating peer ${peer} to have activity ${entry.activity}`);
        peer.activity = entry.activity;
      } else {
        PeerList.push({name: entry.name, activity: entry.activity, messages: []});
      }
    } else {
      UserList.push({name: entry.name, publicKey: pubXKey, mac: entry.mac, active: entry.activity, awaiting: false});
      PeerList.push({name: entry.name, activity: entry.activity, messages: []});
      console.log("updated peers list:", PeerList);
    }
    renderPeers(); // update UI
    console.log("Updated user list:", UserList);
  } else if (indicator === 'u') {
    console.log("Received user list response:", message);
    let entry = JSON.parse(message);
    if (!entry || Object.keys(entry).length === 0) {
      console.warn('Empty or invalid entry, ignoring');
      return;
    }
    console.log("Parsed user list entry:", entry);
    // convert activity to boolean
    entry.activity = Boolean(entry.active);

    // will use stringToPubKey
    let pubXKey = stringToPubKey(entry.pubkey);
    let existingIndex = UserList.findIndex(u => u.name === entry.name);
    if (existingIndex !== -1) {
      UserList[existingIndex] = {name: entry.name, publicKey: pubXKey, mac: entry.mac, active: entry.activity, awaiting: false};
      let peer = PeerList.find(p => p.name === entry.name);
      if (peer) {
        console.log(`Updating peer ${peer} to have activity ${entry.activity}`);
        peer.activity = entry.activity;
      } else {
        PeerList.push({name: entry.name, activity: entry.activity, messages: []});
      }
    } else {
      UserList.push({name: entry.name, publicKey: pubXKey, mac: entry.mac, active: entry.activity, awaiting: false});
      PeerList.push({name: entry.name, activity: entry.activity, messages: []});
      console.log("updated peers list:", PeerList);
    }
    renderPeers(); // update UI
    console.log("Updated user list:", UserList);
  } else if (indicator === 'g') {
    //not sure what to do here, shouldn't be receiving messages with this indicator
  } else if (indicator === 'a') {
    // message contains the ack'd message ID
    const ackId = message.trim();
    if (pendingAcks.has(ackId)) {
      pendingAcks.get(ackId)(); // resolve the waiting promise
      pendingAcks.delete(ackId);
    }
  } else if (indicator === 'w') {
    const success = message.slice(0, 1) === '1';
    if (success) {
        alert('Group created successfully!');
    } else {
        alert('Group creation failed: ' + message.slice(1));
    }
  } else if (indicator === 'p') {
    console.log("Received group list entry:", message);
    let entry;
    try {
        entry = JSON.parse(message);
    } catch (e) {
        console.warn('Failed to parse group entry, ignoring:', message);
        return;
    }
    if (!entry || !entry.name) {
        console.warn('Empty or invalid group entry, ignoring');
        return;
    }

    const existingIndex = GroupList.findIndex(g => g.name === entry.name);
    if (existingIndex !== -1) {
        // update members in case they changed, preserve existing messages
        GroupList[existingIndex].members = entry.members;
        GroupList[existingIndex].creator = entry.creator;
    } else {
        GroupList.push({
            name: entry.name,
            creator: entry.creator,
            members: entry.members,
            messages: []  // messages populated as they arrive
        });
    }
    renderGroups();
    console.log("Updated group list:", GroupList);
  }
}

// function for processing peer list from device
function HandlePeerListFromDevice(peerListStr) {
  console.log("function deprecated");
}

//function for handling incoming BLE notifications
function CharacteristicValueChanged(event) {
  console.log("characteristic value changed");
  const value = event.target.value;

  //early exit
  if (!value || value.byteLength === 0) return;

  //decode value as UTF-8, ignoring errors (e.g. from binary data or incomplete multi-byte sequences)
  let decoded;
  console.log("attempting decode");
  try {
    decoded = new TextDecoder('utf-8', { fatal: false }).decode(value);
  } catch (_) { return; }
  console.log("decoded value:", decoded);
  rxBuffer += decoded;
  console.log("updated rxBuffer:", rxBuffer);

  // process complete rxBuffer
  if (rxBuffer[0] == 'm'){
    console.log('Received message data from device:', rxBuffer);
    rxBuffer = rxBuffer.slice(1); //remove the type character
    let messageStr = rxBuffer;
    rxBuffer = '';
    HandleMessageFromDevice(messageStr);
  } else if (rxBuffer[0] == 'l'){
    console.log('peer list from device deprecated')
  }
}

/** Connect to LocalESP over BLE (or disconnect if already connected) */
async function connect() {
  // If already connected, disconnect and bail
  if (device && server && server.connected) {
    try {
      server.disconnect();
    } catch (_) {}
    setConnected(false);
    device = server = rxChar = txChar = null;
    return;
  }

  try {
    // 1) Show browser's BLE device picker.
    //    Filter by NUS service; fallback to name prefix so "Meshenger-Pager" / "Meshenger-PagerTest" shows.
    device = await navigator.bluetooth.requestDevice({
      filters: [
        { services: [NUS_SERVICE] },
        { namePrefix: 'Meshenger' }
      ],
      optionalServices: [NUS_SERVICE]
    });

    // Store the connected device's MAC address
    ConnectedDeviceMac = device.name;
    ConnectedDeviceMac =ConnectedDeviceMac.slice(15); //remove "Meshenger-Pager" prefix to get the MAC
    macText.textContent = `Device MAC: ${ConnectedDeviceMac}`;
    

    // 2) Connect to GATT server on the ESP32
    const gatt = await device.gatt.connect();
    server = gatt;

    // 3) Get NUS service and the two characteristics
    const service = await gatt.getPrimaryService(NUS_SERVICE);
    txChar = await service.getCharacteristic(NUS_TX);
    rxChar = await service.getCharacteristic(NUS_RX);

    // 4) Subscribe to TX – we get notified when ESP32 sends data
    rxBuffer = '';
    await txChar.startNotifications();
    txChar.addEventListener('characteristicvaluechanged', CharacteristicValueChanged);

    // If ESP32 disconnects (e.g. power off), update UI
    device.addEventListener('gattserverdisconnected', () => setConnected(false));
    setConnected(true);
  } catch (err) {
    // User cancelled the device picker – don't show an error
    if (err.name !== 'NotFoundError') {
      console.error(err);
      alert('Connection failed: ' + (err.message || err));
      ConnectedDeviceMac = "Unknown";
      macText.textContent = `Device MAC: ${ConnectedDeviceMac}`;
    }
  }
}

/** Send text to ESP32 via NUS RX characteristic (chunked for BLE MTU) */
// indicator is the message indicator ('m' for message, 'h' for heartbeat, 's' for sign in, 'r' for register, 'l' for user list, 'g' for get messages)
async function sendMessage(indicator, text, targetName, targetMac) {
  const trimmed = text.trim();
  if (!rxChar || !trimmed || sending) return;
  sending = true;
  btnSend.disabled = true;
  try {
    const encoder = new TextEncoder();
    const chunkText = trimmed.slice(0, MAX_MESSAGE_LENGTH);
    //encodes with message type 'm' for message, followed by the text and end-of-stream as delimiter
    const usernamePadded = UserName.padEnd(12, '\x01').slice(0, 12); // ensure username is exactly 12 chars
    const targetPeerPadded = targetName.padEnd(12, '\x01').slice(0, 12); // ensure target peer ID is exactly 12 chars
    indicator = indicator.slice(0, 1); // ensure indicator is 1 char
    const data = encoder.encode('m'+ targetMac + indicator + usernamePadded + targetPeerPadded+ chunkText + String.fromCharCode(3)); 
    const chunk = 20;
    for (let i = 0; i < data.length; i += chunk) {
      await rxChar.writeValue(data.slice(i, i + chunk));
    }
    
  } finally {
    sending = false;
    btnSend.disabled = !device || !server || !server.connected;
  }
}

function getMessageId() {
 return Array.from(crypto.getRandomValues(new Uint8Array(4))).map(b => b.toString(16).padStart(2, '0')).join(''); // e.g. "a3f1c820"
}

async function sendMessageWithAck(indicator, uuid, text, targetName, targetMac, timeoutMs = 5000) {
  const textWithId = uuid + text; // prepend ID so receiver can ACK it
  console.log("message with uuid: ", textWithId)

  const ackPromise = new Promise((resolve, reject) => {
    pendingAcks.set(uuid, resolve);
    setTimeout(() => {
      if (pendingAcks.has(uuid)) {
        pendingAcks.delete(uuid);
        reject(new Error('ACK timeout'));
      }
    }, timeoutMs);
  });

  await sendMessage(indicator, textWithId, targetName, targetMac);
  await ackPromise; // throws on timeout
}

/** Ask ESP32 for the current list of connected peers */
async function requestPeers() {
  if (!rxChar) return;

  console.log('Requesting peer list from device...');
  let indicator = 'l';
  await sendMessage(indicator, "test", ServerName.padEnd(12, '\x01').slice(0, 12), ServerMAC);  // command to trigger name setting
  console.log('Requested peer list from device, waiting for response...');

  // also request group list if signed in
  if (UserName !== "Unknown") {
    console.log('Requesting group list from server...');
    await sendMessage('p', "test", ServerName.padEnd(12, '\x01').slice(0, 12), ServerMAC);
    console.log('Requested group list from server, waiting for response...');
  }
}

// handler function
async function requestMessages() {
  if (!rxChar || UserName === "Unknown") {
    alert("Cannot get messages if not signed in!");
    return;
  }
  console.log('Requesting messages from server...');
  await sendMessage('g', "test", ServerName.padEnd(12, '\x01').slice(0, 12), ServerMAC);
  console.log('Requested messages from server, waiting for response...');
}

/** Send a group creation request to the server
 *  Format: indicator 'w' + group name (12 chars padded \x01) + participant count (2 chars, e.g. "03") + participant names (12 chars each padded \x01)
 *  Server will register the group and its members
 */
async function createGroup() {
  if (UserName === "Unknown") {
    alert('Please sign in before creating a group');
    return;
  }

  const groupName = prompt('Enter group name (max 12 chars):');
  if (!groupName || !groupName.trim()) return;

  const countStr = prompt('Enter number of other participants (1-9):');
  const count = parseInt(countStr);
  if (isNaN(count) || count < 1 || count > 9) {
    alert('Participant count must be between 1 and 9');
    return;
  }

  const participants = [];
  for (let i = 0; i < count; i++) {
    const name = prompt(`Enter participant ${i + 1} name:`);
    if (!name || !name.trim()) {
      alert('Participant name cannot be empty');
      return;
    }
    participants.push(name.trim().slice(0, 12));
  }

  // Build message body:
  // 12 chars: group name padded with \x01
  // 2 chars: participant count zero-padded (e.g. "03")
  // 12 chars each: participant names padded with \x01
  const groupNamePadded = groupName.trim().slice(0, 12).padEnd(12, '\x01');
  const countPadded = String(count).padStart(2, '0');
  const participantsPadded = participants.map(p => p.padEnd(12, '\x01')).join('');

  const messageBody = groupNamePadded + countPadded + participantsPadded;

  console.log('Sending group creation request:', messageBody);
  await sendMessage('w', messageBody, ServerName.padEnd(12, '\x01').slice(0, 12), ServerMAC);
  console.log('Group creation request sent');
}

async function AttemptLogin(username, password, register=false) {
  if (!rxChar) return;
  const encoder = new TextEncoder();

  // Ensure username is at most 12 characters
  if (username.length > 12) {
    console.warn('Username too long, truncating to 12 characters');
    username = username.slice(0, 12);
  }
  UserName = username;

  PeerList.forEach(peer => {
    peer.messages = [];
  });

  //Set global Keys variable for use in encryption/decryption functions
  const tempKeys = await deriveKeyPair(password, salt);
  Keys = toX25519(tempKeys.edPriv, tempKeys.edPub); 
  let keyStr = pubKeyToString(Keys.xPub); //test conversion functions, remove after testing
  let messageText = keyStr.length + '|' + keyStr;

  //encodes with message type 's' for setting name
  let indicator = register ? 'r' : 's'; // 'r' for register, 's' for sign in
  if (register) {
    registering = true;
  } else {
    signingIn = true;
  }
  await sendMessage(indicator, messageText, ServerName.padEnd(12, '\x01').slice(0, 12), ServerMAC);  // command to trigger name setting
  console.log('Requested to set name on device...');
}

async function requestUserEntry(username) {
  if (!rxChar) return;

  console.log('Requesting user entry from device...');
  let indicator = 'u';
  await sendMessage(indicator, username, ServerName.padEnd(12, '\x01').slice(0, 12), ServerMAC);
  console.log('Requested user entry from device, waiting for response...');
}

async function AttemptSendDirectMessage(t) {
  if (currentPeerId == UserName) {
    alert('Cannot send message to yourself');
    return;
  }
  let userEntry = UserList.find(u => u.name === currentPeerId);
  if (!userEntry) {
    alert('Selected peer not found in user list');
    return;
  }

  userEntry.awaiting = true;
  requestUserEntry(currentPeerId);
  const ms = 5000;
  const interval = 50;
  let elapsed = 0;
  let success = false;
  while (elapsed < ms) {
    userEntry = UserList.find(u => u.name === currentPeerId);
    if (!userEntry.awaiting) { success = true; break; }
    await new Promise(r => setTimeout(r, interval));
    elapsed += interval;
  }
  if (!success) {
    alert("Couldn't send message, server didn't verify mac");
    return;
  }

  const secret = await sharedAesKey(Keys.xPriv, userEntry.publicKey);
  let peer = PeerList.find(p => p.name === currentPeerId);
  for (let i = 0; i < t.length; i += maxPlaintextLength(MAX_UMESSAGE_LENGTH)) {
    const chunk = t.slice(i, i + maxPlaintextLength(MAX_UMESSAGE_LENGTH));
    const encrypted = await encrypt(secret, chunk);

    let delivered = false;
    let deliveredDirect = false;
    let uuid = getMessageId();

    if (userEntry.active !== false) {
      for (let attempt = 0; attempt < 4 && !delivered; attempt++) {
        try {
          await sendMessageWithAck('m', uuid, encrypted, userEntry.name, userEntry.mac, RETRY_DELAYS[attempt]);
          delivered = true;
          deliveredDirect = true;
        } catch (err) {
          console.warn(`Direct send attempt ${attempt + 1} failed: ${err.message}`);
        }
      }
    }

    uuid = getMessageId();
    if (!delivered) {
      for (let attempt = 0; attempt < 4 && !delivered; attempt++) {
        try {
          await sendMessageWithAck('m', uuid, encrypted, userEntry.name, ServerMAC, RETRY_DELAYS[attempt]);
          delivered = true;
        } catch (err) {
          console.warn(`Server relay attempt ${attempt + 1} failed: ${err.message}`);
        }
      }
    }

    if (!delivered) {
      alert(`Message delivery failed after all retries.`);
      break;
    }

    if (peer) {
      const route = delivered && !deliveredDirect ? 'server' : 'direct';
      peer.messages.push({ content: chunk, sender: true, route });
      appendMessage(chunk, true, route);
    }
  }
  input.value = '';
}

async function AttemptSendGroupMessage(t, group) {
  // build the group prefix: \x02 + group name padded to 12 chars
  const groupPrefix = '\x02' + group.name.padEnd(12, '\x01').slice(0, 12);

  // get members excluding ourselves
  const otherMembers = group.members.filter(m => m !== UserName);

  for (let i = 0; i < t.length; i += maxPlaintextLength(MAX_GMESSAGE_LENGTH)) {
    const chunk = t.slice(i, i + maxPlaintextLength(MAX_GMESSAGE_LENGTH));

    let anyDelivered = false;

    for (const memberName of otherMembers) {
      let userEntry = UserList.find(u => u.name === memberName);
      if (!userEntry) {
        alert(`User entry not found for group member ${memberName}, skipping`);
        continue;
      }

      // refresh mac for this member
      userEntry.awaiting = true;
      requestUserEntry(memberName);
      const ms = 5000;
      const interval = 50;
      let elapsed = 0;
      while (elapsed < ms) {
        userEntry = UserList.find(u => u.name === memberName);
        if (!userEntry.awaiting) break;
        await new Promise(r => setTimeout(r, interval));
        elapsed += interval;
      }
      if (userEntry.awaiting) {
        alert(`Couldn't refresh mac for ${memberName}, skipping`);
        continue;
      }

      const secret = await sharedAesKey(Keys.xPriv, userEntry.publicKey);
      const encrypted = await encrypt(secret, chunk);
      const gmessage = groupPrefix + encrypted;

      let delivered = false;
      let deliveredDirect = false;
      let uuid = getMessageId();

      // attempt direct
      if (userEntry.active !== false) {
        for (let attempt = 0; attempt < 4 && !delivered; attempt++) {
          try {
            await sendMessageWithAck('m', uuid, gmessage, userEntry.name, userEntry.mac, RETRY_DELAYS[attempt]);
            delivered = true;
            deliveredDirect = true;
          } catch (err) {
            console.warn(`Direct send attempt ${attempt + 1} to ${memberName} failed: ${err.message}`);
          }
        }
      }

      // fallback to server
      uuid = getMessageId();
      if (!delivered) {
        for (let attempt = 0; attempt < 4 && !delivered; attempt++) {
          try {
            await sendMessageWithAck('m', uuid, gmessage, userEntry.name, ServerMAC, RETRY_DELAYS[attempt]);
            delivered = true;
          } catch (err) {
            console.warn(`Server relay attempt ${attempt + 1} to ${memberName} failed: ${err.message}`);
          }
        }
      }

      if (delivered) {
        anyDelivered = true;
        console.log(`Chunk delivered to ${memberName}`);
      } else {
        alert(`Failed to deliver chunk to ${memberName} after all retries`);
      }
    }

    if (anyDelivered) {
      const route = 'direct'; // best effort across members, could refine
      group.messages.push({ content: chunk, sender: UserName, route });
      appendGroupMessage(chunk, UserName, route);
    } else {
      alert('Failed to deliver message to any group member');
      break;
    }
  }
  input.value = '';
}

async function AttemptSendMessage(event){
  event.preventDefault();
  if (Keys == null) {
    alert('Please sign in or register before sending messages');
    return;
  }
  const t = input.value;
  if (!t.trim() || sending) return;

  let userEntry = UserList.find(u => u.name === currentPeerId);
  const currentGroup = GroupList.find(g => g.name === currentPeerId);

  if (userEntry) {
    AttemptSendDirectMessage(t);
  } else if (currentGroup){
    AttemptSendGroupMessage(t, currentGroup);
  } else {
    alert('Selected peer or group not found in lists');
  }
}

function InitialSetup() {
  // --- Event listeners ---
  btnConnect.addEventListener('click', () => connect());
  btnPeerList.addEventListener('click', () => requestPeers());
  btnSetName.addEventListener('click', () => {
    const registering = confirm('Click OK to add new account, or cancel to sign in');
    const username = prompt('Enter your device name:');
    const password = prompt('Enter your password:');
    if (username) {
      AttemptLogin(username, password, registering);
    }
  });
  btnGetMessages.addEventListener('click', () => requestMessages());
  btnCreateGroup.addEventListener('click', () => createGroup());

  form.addEventListener('submit', (e) => {
    AttemptSendMessage(e);
  });

  // Nav buttons: switch between Peers / Messages / Settings
  navButtons.forEach(btn => {
    btn.addEventListener('click', () => {
      const section = btn.dataset.section;
      setSection(section);
    });
  });

  // Initial render
  renderPeers();
  renderGroups();
  setSection('peers');
}

InitialSetup();

//=========================================================================================================
// --- Crypto Functions ---
//=========================================================================================================

import { ed25519, x25519 } from "https://esm.sh/@noble/curves@1.4.0/ed25519";
import { edwardsToMontgomeryPriv, edwardsToMontgomeryPub } from "https://esm.sh/@noble/curves@1.4.0/ed25519";

const salt = "my-app-salt-v1"

/* ── utils ── */
const toHex   = (b) => Array.from(b).map(x => x.toString(16).padStart(2,"0")).join("");
const fromHex = (h) => new Uint8Array(h.match(/.{2}/g).map(b => parseInt(b,16)));

/* ── derive Ed25519 keypair from password ── */
async function deriveKeyPair(password, salt) {
  const keyMaterial = await crypto.subtle.importKey(
    "raw", new TextEncoder().encode(password),
    "PBKDF2", false, ["deriveBits"]
  );
  const seedBuffer = await crypto.subtle.deriveBits(
    { name: "PBKDF2", salt: new TextEncoder().encode(salt), iterations: 600_000, hash: "SHA-256" },
    keyMaterial, 256
  );
  const seed = new Uint8Array(seedBuffer);
  return {
    edPriv: seed,
    edPub:  ed25519.getPublicKey(seed),
  };
}

/* ── convert Ed25519 → X25519 ── */
function toX25519(edPriv, edPub) {
  return {
    xPriv: edwardsToMontgomeryPriv(edPriv),
    xPub:  edwardsToMontgomeryPub(edPub),
  };
}

/* ── ECDH shared secret → AES-GCM key ── */
async function sharedAesKey(myXPriv, theirXPub) {
  const raw = x25519.getSharedSecret(myXPriv, theirXPub);
  return crypto.subtle.importKey("raw", raw, { name: "AES-GCM" }, false, ["encrypt", "decrypt"]);
}

/* ── utils ── */
const toB64   = (b) => btoa(String.fromCharCode(...b)).replace(/\+/g,'-').replace(/\//g,'_').replace(/=/g,'');
const fromB64 = (s) => Uint8Array.from(atob(s.replace(/-/g,'+').replace(/_/g,'/')), c => c.charCodeAt(0));

/* ── encrypt → compact "iv.ct" string ── */
async function encrypt(aesKey, plaintext) {
  const iv  = crypto.getRandomValues(new Uint8Array(12));
  const ct  = await crypto.subtle.encrypt({ name: "AES-GCM", iv }, aesKey, new TextEncoder().encode(plaintext));
  return toB64(iv) + '.' + toB64(new Uint8Array(ct));   // ~16 + 1 + ~ceil(len*4/3) chars
}

/* ── decrypt from compact string ── */
async function decrypt(aesKey, packed) {
  const [ivB64, ctB64] = packed.split('.');
  const plain = await crypto.subtle.decrypt(
    { name: "AES-GCM", iv: fromB64(ivB64) },
    aesKey,
    fromB64(ctB64)
  );
  return new TextDecoder().decode(plain);
}

function pubKeyToString(xPub) {
  return toB64(xPub);  // reuse your existing toHex util
}

function stringToPubKey(str) {
  return fromB64(str);  // reuse your existing fromHex util
}

function encryptedLength(plaintextLength) {
  const ciphertextBytes = plaintextLength + 16; // AES-GCM adds 16-byte tag
  //console.log("cipher text bytes: ", ciphertextBytes);
  const ivB64  = Math.ceil(12 * 4 / 3);         // 12 IV bytes → 16 Base64url chars
  //console.log("ivB64: ", ivB64);
  const ctB64  = Math.ceil(ciphertextBytes * 4 / 3); // round up to nearest 4
  //console.log("ctB64:", ctB64);
  return ivB64 + 1 + ctB64;                     // +1 for the '.' separator
}

function maxPlaintextLength(maxEncryptedLength) {
  // inverse of encryptedLength:
  // maxEncryptedLength = 16 + 1 + ceil((pl + 16) * 4/3)
  // so: pl = floor((maxEncryptedLength - 17) * 3/4) - 16
  return Math.floor((maxEncryptedLength - 17) * 3 / 4) - 16;
}

//----------------------------------------------------------------------------------------------------
// Example usage:
//----------------------------------------------------------------------------------------------------
/****************************************************************************************************/
//this is example of enc/dec functions
/* const salt = "my-app-salt-v1";

// Alice derives her keypair
const alice = await deriveKeyPair("alice-password", salt);
const aliceX = toX25519(alice.edPriv, alice.edPub);

// Bob derives his keypair
const bob = await deriveKeyPair("bob-password", salt);
const bobX = toX25519(bob.edPriv, bob.edPub);

// Each side computes the shared secret independently
const aliceAes = await sharedAesKey(aliceX.xPriv, bobX.xPub);
const bobAes   = await sharedAesKey(bobX.xPriv, aliceX.xPub);

// Alice encrypts
const encrypted = await encrypt(aliceAes, "hello bob!");
console.log("encrypted:", encrypted);

// Bob decrypts
const decrypted = await decrypt(bobAes, encrypted);
console.log("decrypted:", decrypted); // "hello bob!" */

/****************************************************************************************************/
//this is example of verification
/*const { privateKey, publicKey } = await deriveKeyPair("password", "salt");

const message = new TextEncoder().encode("hello world");

// Sign
const privBytes = bytesFromHex(privateKey);
const signature = ed25519.sign(message, privBytes);

// Verify
const pubBytes = bytesFromHex(publicKey);
const valid = ed25519.verify(signature, message, pubBytes);
console.log(valid); // true*/