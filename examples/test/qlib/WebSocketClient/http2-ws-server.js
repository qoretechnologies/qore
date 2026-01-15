#!/usr/bin/env node
// http2-ws-server.js
// Copyright 2026 Qore Technologies, s.r.o.
// Minimal RFC 8441 WebSocket-over-HTTP/2 test server.

'use strict';

const fs = require('fs');
const http2 = require('http2');
const crypto = require('crypto');

const MAGIC = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';

function usage() {
  console.error('Usage: http2-ws-server.js <port> <cert.pem> <key.pem>');
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
  let length = payload.length;
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

const server = http2.createSecureServer({
  key,
  cert,
  allowHTTP1: false,
  settings: {
    enableConnectProtocol: true,
  },
});

server.on('stream', (stream, headers) => {
  const method = headers[':method'];
  const protocol = headers[':protocol'];
  const authority = headers[':authority'] || '';
  const keyHeader = headers['sec-websocket-key'];
  const versionHeader = headers['sec-websocket-version'];

  if (method !== 'CONNECT' || protocol !== 'websocket') {
    stream.respond({ ':status': 404 });
    stream.end('not found');
    return;
  }

  if (!authority.endsWith(':' + port)) {
    stream.respond({ ':status': 400 });
    stream.end('missing or invalid :authority');
    return;
  }

  if (!keyHeader || !versionHeader || String(versionHeader) !== '13') {
    stream.respond({ ':status': 400 });
    stream.end('missing or invalid websocket headers');
    return;
  }

  stream.respond({
    ':status': 200,
    'sec-websocket-accept': acceptKey(String(keyHeader)),
  });

  if (headers['send-data']) {
    const dataMessages = [
      { opcode: 0x1, payload: Buffer.from('test1', 'utf8') },
      { opcode: 0x2, payload: Buffer.from([0xbe, 0xef, 0xfe, 0xed]) },
      { opcode: 0x1, payload: Buffer.from('test3', 'utf8') },
    ];
    for (const msg of dataMessages) {
      stream.write(encodeFrame(msg.opcode, msg.payload));
    }
  }

  if (headers['send-large-data']) {
    const largePayload = Buffer.from('X'.repeat(32768), 'utf8');
    stream.write(encodeFrame(0x1, largePayload));
  }

  let recvBuffer = Buffer.alloc(0);
  let fragOpcode = null;
  let fragBuffer = Buffer.alloc(0);

  stream.on('data', (chunk) => {
    recvBuffer = Buffer.concat([recvBuffer, chunk]);
    recvBuffer = parseFrames(recvBuffer, (frame) => {
      if (!frame.masked) {
        stream.write(encodeFrame(0x8, Buffer.from([0x03, 0xea])));
        stream.end();
        return;
      }

      if (frame.opcode === 0x0) {
        if (fragOpcode === null) {
          stream.write(encodeFrame(0x8, Buffer.from([0x03, 0xea])));
          stream.end();
          return;
        }
        fragBuffer = Buffer.concat([fragBuffer, frame.payload]);
        if (frame.fin) {
          handleMessage(fragOpcode, fragBuffer);
          fragOpcode = null;
          fragBuffer = Buffer.alloc(0);
        }
        return;
      }

      if (frame.opcode === 0x1 || frame.opcode === 0x2) {
        if (frame.fin) {
          handleMessage(frame.opcode, frame.payload);
        } else {
          fragOpcode = frame.opcode;
          fragBuffer = Buffer.from(frame.payload);
        }
        return;
      }

      if (frame.opcode === 0x8) {
        stream.write(encodeFrame(0x8, frame.payload));
        stream.end();
        return;
      }

      if (frame.opcode === 0x9) {
        stream.write(encodeFrame(0xA, frame.payload));
        return;
      }
    });
  });

  stream.on('error', () => {});

  function handleMessage(opcode, payload) {
    if (opcode === 0x1 || opcode === 0x2) {
      stream.write(encodeFrame(opcode, payload));
    }
  }
});

server.listen(port, '127.0.0.1', () => {
  console.log('READY ' + port);
});

process.on('SIGTERM', () => {
  server.close(() => process.exit(0));
});
