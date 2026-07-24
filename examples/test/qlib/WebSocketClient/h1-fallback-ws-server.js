#!/usr/bin/env node
// h1-fallback-ws-server.js
// Copyright 2026 Qore Technologies, s.r.o.
// Test server that simulates a reverse proxy (e.g. nginx) which negotiates
// HTTP/2 via ALPN but does NOT support RFC 8441 extended CONNECT: it does not
// advertise SETTINGS_ENABLE_CONNECT_PROTOCOL and rejects any CONNECT stream.
// It DOES serve a plain HTTP/1.1 WebSocket 101 Upgrade (allowHTTP1), so a
// correct client must fall back to HTTP/1.1 to establish the WebSocket.

'use strict';

const fs = require('fs');
const http2 = require('http2');
const crypto = require('crypto');

const MAGIC = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';

function usage() {
  console.error('Usage: h1-fallback-ws-server.js <port> <cert.pem> <key.pem>');
  process.exit(2);
}

if (process.argv.length < 5) {
  usage();
}

const port = Number(process.argv[2]);
const certPath = process.argv[3];
const keyPath = process.argv[4];

if (!port || !certPath || !keyPath) {
  usage();
}

const cert = fs.readFileSync(certPath);
const key = fs.readFileSync(keyPath);

function encodeFrame(opcode, payload, fin = true) {
  const firstByte = (fin ? 0x80 : 0x00) | (opcode & 0x0f);
  let header = [firstByte];
  const length = payload.length;
  if (length < 126) {
    header.push(length);
  } else if (length < 65536) {
    header.push(126);
    header.push((length >> 8) & 0xff, length & 0xff);
  } else {
    header.push(127);
    const lenBuf = Buffer.alloc(8);
    lenBuf.writeBigUInt64BE(BigInt(length));
    header = header.concat(Array.from(lenBuf));
  }
  return Buffer.concat([Buffer.from(header), payload]);
}

function acceptKey(keyValue) {
  return crypto.createHash('sha1').update(keyValue + MAGIC).digest('base64');
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

    const maskOffset = headerLen;
    const payloadOffset = masked ? headerLen + 4 : headerLen;
    const totalLen = payloadOffset + length;

    if (buffer.length - offset < totalLen) {
      break;
    }

    let payload = buffer.slice(offset + payloadOffset, offset + totalLen);
    if (masked) {
      const mask = buffer.slice(offset + maskOffset, offset + maskOffset + 4);
      const unmasked = Buffer.alloc(payload.length);
      for (let i = 0; i < payload.length; i++) {
        unmasked[i] = payload[i] ^ mask[i % 4];
      }
      payload = unmasked;
    }

    onFrame({ fin, opcode, masked, payload });
    offset += totalLen;
  }

  return buffer.slice(offset);
}

// allowHTTP1 true so H1 fallback connections are served; enableConnectProtocol
// is deliberately NOT set (defaults to false) so the server behaves like a
// proxy without RFC 8441 support.
const server = http2.createSecureServer({
  key,
  cert,
  allowHTTP1: true,
});

// Any HTTP/2 stream (including a CONNECT with :protocol=websocket) is rejected:
// extended CONNECT is not supported here, forcing the client to fall back.
server.on('stream', (stream) => {
  stream.respond({ ':status': 501 });
  stream.end('extended CONNECT not supported');
});

// HTTP/1.1 fallback path: complete a standard RFC 6455 WebSocket handshake and
// echo text/binary frames.
server.on('upgrade', (req, socket) => {
  const keyHeader = req.headers['sec-websocket-key'];
  const versionHeader = req.headers['sec-websocket-version'];
  if ((req.headers['upgrade'] || '').toLowerCase() !== 'websocket'
      || !keyHeader || String(versionHeader) !== '13') {
    socket.write('HTTP/1.1 400 Bad Request\r\n\r\n');
    socket.destroy();
    return;
  }

  socket.write(
    'HTTP/1.1 101 Switching Protocols\r\n'
    + 'Upgrade: websocket\r\n'
    + 'Connection: Upgrade\r\n'
    + 'Sec-WebSocket-Accept: ' + acceptKey(String(keyHeader)) + '\r\n'
    + '\r\n');

  let recvBuffer = Buffer.alloc(0);
  socket.on('data', (chunk) => {
    recvBuffer = Buffer.concat([recvBuffer, chunk]);
    recvBuffer = parseFrames(recvBuffer, (frame) => {
      if (!frame.masked) {
        // client frames must be masked
        socket.write(encodeFrame(0x8, Buffer.from([0x03, 0xea])));
        socket.end();
        return;
      }
      if (frame.opcode === 0x1 || frame.opcode === 0x2) {
        socket.write(encodeFrame(frame.opcode, frame.payload));
        return;
      }
      if (frame.opcode === 0x8) {
        socket.write(encodeFrame(0x8, frame.payload));
        socket.end();
        return;
      }
      if (frame.opcode === 0x9) {
        socket.write(encodeFrame(0xA, frame.payload));
      }
    });
  });
  socket.on('error', () => {});
});

server.listen(port, '127.0.0.1', () => {
  console.log('READY ' + port);
});

process.on('SIGTERM', () => {
  server.close(() => process.exit(0));
});
