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
const btnSend = document.getElementById('btnSend');
const statusDot = document.getElementById('statusDot');
const statusText = document.getElementById('statusText');
const navButtons = document.querySelectorAll('.nav-item');
const peersListEl = document.getElementById('peersList');
const peersView = document.getElementById('view-peers');
const messagesView = document.getElementById('view-messages');
const settingsView = document.getElementById('view-settings');
const chatPeerLabel = document.getElementById('chatPeerLabel');
const settingSounds = document.getElementById('settingSounds');
const settingBrightness = document.getElementById('settingBrightness');

//format for peers: "name", where name is peer's MAC, "messages", where message is an array of message strings from that peer
/*example structure:
peers = [
  {
    name: "ab:cd:ef:12:34:56",
    "messages": [
      {content: "hello", sent: false},
      {content: "hi there", sent: true}
    ],
  },
*/
let peers = [];

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
}

/** Populate the peers list in the Peers view */
function renderPeers() {
  peersListEl.innerHTML = '';
  peers.forEach(peer => {
    const li = document.createElement('li');
    const btn = document.createElement('button');
    btn.textContent = peer.name;
    btn.addEventListener('click', () => selectPeer(peer.name));
    console.log('Added peer to list:', peer.name);
    li.appendChild(btn);
    peersListEl.appendChild(li);
  });
}

/** Handle peer selection and open its chat window */
function selectPeer(peerId) {
  currentPeerId = peerId;
  const peer = peers.find(p => p.name === peerId);
  chatPeerLabel.textContent = peer ? `Chat with ${peer.name}` : 'Chat';
  // Highlight active peer
  const buttons = peersListEl.querySelectorAll('button');
  buttons.forEach(btn => {
    btn.classList.toggle('active', btn.textContent === (peer.name));
  });
  messagesEl.replaceChildren(); // clear messages when switching peers
  console.log(peer.messages);
  peer.messages.forEach(message => {
    appendMessage(message.content, message.sent);
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
  input.disabled = !connected;
  btnSend.disabled = !connected;
  compose.classList.toggle('disabled', !connected);  // shows/hides Send button
}

/** Add a message bubble (sent or received) and scroll to bottom */
function appendMessage(text, sent) {
  emptyState.classList.add('hidden');
  const div = document.createElement('div');
  div.className = 'message ' + (sent ? 'sent' : 'received');
  const time = new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
  div.innerHTML = `<span class="content">${escapeHtml(text)}</span><div class="time">${time}</div>`;
  messagesEl.appendChild(div);
  messagesEl.scrollTop = messagesEl.scrollHeight;  // keep latest message in view
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

// handle incoming message from device
function HandleMessageFromDevice(message) {
  const targetPeerId = message.slice(0, 17);
  message = message.slice(17); //remove the target peer ID from the message
  console.log("Extracted target peer ID:", targetPeerId);
  console.log("message after slicing:", message);
  const lines = message.split(/\r?\n/);
  console.log("Split message into lines:", lines);
  message = '';
  lines.forEach(line => {
    console.log("in line for loop")
    console.log("Processing line from device:", line);
    const trimmed = line.trim();
    console.log("Processing line:", trimmed);
    if (trimmed && isDisplayableText(trimmed)){
      let peer = peers.find(p => p.name === targetPeerId);
      let sentStatus = false;
      peer.messages.push({content: trimmed, sent: sentStatus});
      console.log("peer messages", peer.messages);
      if (peer.name === currentPeerId) {
        appendMessage(trimmed, sentStatus);
      }
    }
  });
}

// function for processing peer list from device
function HandlePeerListFromDevice(peerListStr) {
  console.log('Received peer list data from device:', peerListStr);
  const newPeers = JSON.parse(peerListStr);
  for (const peer of newPeers) {
    console.log("Processing peer from device:", peer);
    if (!peers.some(p => p.name === peer)) {
      console.log("Adding new peer to list:", peer);
      peers.push({name: peer, messages: []});
    }
  }
  console.log("Updated peers list:", peers);
  renderPeers(); // update UI
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
  //first character of rxBuffer is the characteristic value type, 'l' for peer list, 'm' for message
  if (rxBuffer[0] == 'm'){
    console.log('Received message data from device:', rxBuffer);
    rxBuffer = rxBuffer.slice(1); //remove the type character
    let messageStr = rxBuffer;
    rxBuffer = '';
    HandleMessageFromDevice(messageStr);
  } else if (rxBuffer[0] == 'l'){
    rxBuffer = rxBuffer.slice(1); //remove the type character
    let peerListStr = rxBuffer; 
    rxBuffer = ''; 
    HandlePeerListFromDevice(peerListStr);
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
    }
  }
}

/** Send text to ESP32 via NUS RX characteristic (chunked for BLE MTU) */
async function sendMessage(text) {
  const trimmed = text.trim();
  if (!rxChar || !trimmed || sending) return;
  sending = true;
  btnSend.disabled = true;
  try {
    let peer = peers.find(p => p.name === currentPeerId);
    const encoder = new TextEncoder();
    //encodes with message type 'm' for message, followed by the text and a newline as delimiter
    const data = encoder.encode('m'+ peer.name + trimmed + '\n' + String.fromCharCode(3)); 
    const chunk = 20;
    for (let i = 0; i < data.length; i += chunk) {
      await rxChar.writeValue(data.slice(i, i + chunk));
    }
    let sentStatus = true;
    peer.messages.push({content: trimmed, sent: sentStatus});
    appendMessage(trimmed, sentStatus);
  } finally {
    sending = false;
    btnSend.disabled = !device || !server || !server.connected;
  }
}

/** Ask ESP32 for the current list of connected peers */
async function requestPeers() {
  if (!rxChar) return;
  const encoder = new TextEncoder();
  //encodes with message type 'l' for peer list request
  await rxChar.writeValue(encoder.encode('l'));  // command to trigger peer list response
  console.log('Requested peer list from device...');
}

function InitialSetup() {
  // --- Event listeners ---
  btnConnect.addEventListener('click', () => connect());
  btnPeerList.addEventListener('click', () => requestPeers());

  form.addEventListener('submit', (e) => {
    e.preventDefault();
    const t = input.value;
    if (!t.trim() || sending) return;
    sendMessage(t);
    input.value = '';
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
  setSection('peers');
}

InitialSetup();

//=========================================================================================================
// --- Crypto Functions ---
//=========================================================================================================

import { ed25519, x25519 } from "https://esm.sh/@noble/curves@1.4.0/ed25519";
import { edwardsToMontgomeryPriv, edwardsToMontgomeryPub } from "https://esm.sh/@noble/curves@1.4.0/ed25519";

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

/* ── encrypt ── */
async function encrypt(aesKey, plaintext) {
  const iv   = crypto.getRandomValues(new Uint8Array(12));
  const data = new TextEncoder().encode(plaintext);
  const ct   = await crypto.subtle.encrypt({ name: "AES-GCM", iv }, aesKey, data);
  return { iv: toHex(iv), ct: toHex(new Uint8Array(ct)) };
}

/* ── decrypt ── */
async function decrypt(aesKey, { iv, ct }) {
  const plain = await crypto.subtle.decrypt(
    { name: "AES-GCM", iv: fromHex(iv) },
    aesKey,
    fromHex(ct)
  );
  return new TextDecoder().decode(plain);
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