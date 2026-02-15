#!/usr/bin/env node
// http2-ws-client.js
// Copyright 2026 Qore Technologies, s.r.o.
// Minimal RFC 8441 WebSocket-over-HTTP/2 client for server verification.

'use strict';

const http2 = require('http2');
const crypto = require('crypto');

const MAGIC = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';

function usage() {
  console.error('Usage: http2-ws-client.js <host> <port> <path>');
  process.exit(2);
}

if (process.argv.length < 5) {
  usage();
}

const host = process.argv[2];
const port = Number(process.argv[3]);
const path = process.argv[4] || '/';

if (!host || !port || !path) {
  usage();
}

function acceptKey(keyValue) {
  return crypto.createHash('sha1').update(keyValue + MAGIC).digest('base64');
}

function encodeClientFrame(opcode, payload, fin = true) {
  const firstByte = (fin ? 0x80 : 0x00) | (opcode & 0x0f);
  const maskKey = crypto.randomBytes(4);
  let length = payload.length;
  let header = [firstByte];

  if (length < 126) {
    header.push(0x80 | length);
  } else if (length < 65536) {
    header.push(0x80 | 126);
    header.push((length >> 8) & 0xff, length & 0xff);
  } else {
    header.push(0x80 | 127);
    const lenBuf = Buffer.alloc(8);
    lenBuf.writeBigUInt64BE(BigInt(length));
    header = header.concat(Array.from(lenBuf));
  }

  const masked = Buffer.alloc(payload.length);
  for (let i = 0; i < payload.length; i++) {
    masked[i] = payload[i] ^ maskKey[i % 4];
  }

  return Buffer.concat([Buffer.from(header), maskKey, masked]);
}

function parseFrames(buffer, onFrame) {
  let offset = 0;
  while (buffer.length - offset >= 2) {
    const b0 = buffer[offset];
    const b1 = buffer[offset + 1];
    const fin = (b0 & 0x80) !== 0;
    const opcode = b0 & 0x0f;
    const masked = (b1 & 0x80) !== 0;
    let length = b1 & 0x7f;
    let headerLen = 2;

    if (length === 126) {
      if (buffer.length - offset < 4) {
        break;
      }
      length = buffer.readUInt16BE(offset + 2);
      headerLen = 4;
    } else if (length === 127) {
      if (buffer.length - offset < 10) {
        break;
      }
      const bigLen = buffer.readBigUInt64BE(offset + 2);
      if (bigLen > BigInt(Number.MAX_SAFE_INTEGER)) {
        throw new Error('Frame too large');
      }
      length = Number(bigLen);
      headerLen = 10;
    }

    const maskOffset = offset + headerLen;
    const payloadOffset = maskOffset + (masked ? 4 : 0);
    const frameEnd = payloadOffset + length;
    if (buffer.length < frameEnd) {
      break;
    }

    let payload = buffer.slice(payloadOffset, frameEnd);
    if (masked) {
      const maskKey = buffer.slice(maskOffset, maskOffset + 4);
      const unmasked = Buffer.alloc(payload.length);
      for (let i = 0; i < payload.length; i++) {
        unmasked[i] = payload[i] ^ maskKey[i % 4];
      }
      payload = unmasked;
    }

    onFrame({ fin, opcode, payload });
    offset = frameEnd;
  }
  return buffer.slice(offset);
}

function fail(code, msg) {
  console.error(msg);
  process.exit(code);
}

const session = http2.connect(`https://${host}:${port}`, {
  rejectUnauthorized: false,
  ALPNProtocols: ['h2'],
  settings: {
    enableConnectProtocol: true,
  },
});

session.on('error', (err) => {
  fail(1, `Session error: ${err.message}`);
});

const key = crypto.randomBytes(16).toString('base64');
const expectedAccept = acceptKey(key);

const req = session.request({
  ':method': 'CONNECT',
  ':path': path,
  ':authority': `${host}:${port}`,
  ':scheme': 'https',
  ':protocol': 'websocket',
  'sec-websocket-key': key,
  'sec-websocket-version': '13',
}, {
  endStream: false,
});

let responseOk = false;
let gotWelcome = false;
let gotEcho = false;
let buffer = Buffer.alloc(0);
let sentMessage = false;
let sentClose = false;

const payload = Buffer.from('node-test', 'utf8');

req.on('response', (headers, flags) => {
  const status = headers[':status'];
  const accept = headers['sec-websocket-accept'];
  if (status !== 200) {
    fail(2, `CONNECT failed with status ${status}`);
  }
  if (accept !== expectedAccept) {
    fail(3, 'Sec-WebSocket-Accept mismatch');
  }
  if ((flags & http2.constants.NGHTTP2_FLAG_END_STREAM) !== 0) {
    fail(6, 'CONNECT response ended stream (END_STREAM set)');
  }
  responseOk = true;
  if (!sentMessage) {
    sentMessage = true;
    if (req.writableEnded || req.closed) {
      fail(6, 'CONNECT stream closed before client could send data');
    }
    req.write(encodeClientFrame(0x1, payload));
  }
});

req.on('data', (chunk) => {
  buffer = Buffer.concat([buffer, chunk]);
  try {
    buffer = parseFrames(buffer, (frame) => {
      if (frame.opcode === 0x1) {
        const text = frame.payload.toString('utf8');
        if (!gotWelcome) {
          gotWelcome = text === 'welcome';
        } else if (!gotEcho) {
          gotEcho = text === 'node-test';
        }
      }
    });
  } catch (err) {
    fail(4, `Frame parse error: ${err.message}`);
  }

  // Check if we've received all expected messages
  if (responseOk && gotWelcome && gotEcho && !sentClose) {
    sentClose = true;
    // If stream is already ended (race with server close), just exit successfully
    if (req.writableEnded) {
      session.close();
      process.exit(0);
    }
    // Send WebSocket close frame and close the HTTP/2 stream
    const closeFrame = encodeClientFrame(0x8, Buffer.alloc(0));
    req.write(closeFrame, () => {
      // For HTTP/2 CONNECT streams, explicitly close with RST_STREAM
      req.close(http2.constants.NGHTTP2_NO_ERROR, () => {
        session.close();
        process.exit(0);
      });
    });
  }
});

req.on('error', (err) => {
  fail(1, `Stream error: ${err.message}`);
});

req.on('close', () => {
  if (!responseOk) {
    fail(2, 'CONNECT failed before response');
  }
  if (!gotWelcome || !gotEcho) {
    fail(5, 'Did not receive expected welcome/echo messages');
  }
  session.close();
  process.exit(0);
});

setTimeout(() => {
  fail(6, 'Timeout waiting for WebSocket messages');
}, 10000);
