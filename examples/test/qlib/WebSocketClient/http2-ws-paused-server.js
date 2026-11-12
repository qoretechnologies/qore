#!/usr/bin/env node
// http2-ws-paused-server.js
// Copyright 2026 Qore Technologies, s.r.o.
// RFC 8441 WebSocket-over-HTTP/2 test server that reads no data from a WebSocket stream until it is released, so it
// grants the client no flow-control credit (qore#5879).
//
// Usage: http2-ws-paused-server.js <port> <cert.pem> <key.pem>
// - prints "READY <port>" once listening
// - a line "release" on stdin resumes reading the WebSocket streams; a line "close <code>[:<reason>][,...]" writes a
//   close frame for each code (with its reason) in one write; a line "ping <payload>[,...]" writes a ping frame for
//   each payload in one write; a line "big-header" writes the header of a binary frame announcing 1 GiB
// - prints "PONG <payload>" for each pong received, and "END" when the client ends its side of the stream
// - prints "MSG <size> <first byte> <last byte>" for each message received, and "CLOSE <code>" for a close frame,
//   which it answers

'use strict';

const fs = require('fs');
const http2 = require('http2');
const crypto = require('crypto');
const readline = require('readline');

const MAGIC = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';

if (process.argv.length < 5) {
  console.error('Usage: http2-ws-paused-server.js <port> <cert.pem> <key.pem>');
  process.exit(2);
}

const port = Number(process.argv[2]);
const cert = fs.readFileSync(process.argv[3]);
const key = fs.readFileSync(process.argv[4]);

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

// the WebSocket streams, paused until released
const streams = [];
let released = false;

readline.createInterface({ input: process.stdin }).on('line', (line) => {
  const cmd = line.trim().split(' ');
  if (cmd[0] === 'release') {
    released = true;
    for (const s of streams) {
      s.resume();
    }
  } else if (cmd[0] === 'close' && cmd[1]) {
    // writes a close frame for each "<code>[:<reason>]" given (comma-separated) in one write
    const frames = [];
    for (const item of cmd[1].split(',')) {
      const [code, reason] = item.split(':');
      const n = Number(code);
      if (!Number.isInteger(n) || n < 0 || n > 65535) {
        console.log('BAD CLOSE ' + item);
        return;
      }
      const p = Buffer.alloc(2);
      p.writeUInt16BE(n);
      frames.push(encodeFrame(0x8, Buffer.concat([p, Buffer.from(reason || '', 'utf8')])));
    }
    for (const s of streams) {
      if (!s.destroyed && s.writable) {
        s.write(Buffer.concat(frames));
      }
    }
    console.log('SENT CLOSE ' + cmd[1]);
  } else if (cmd[0] === 'ping' && cmd[1]) {
    // writes a ping frame for each payload given (comma-separated) in one write
    const frames = cmd[1].split(',').map((p) => encodeFrame(0x9, Buffer.from(p, 'utf8')));
    for (const s of streams) {
      if (!s.destroyed && s.writable) {
        s.write(Buffer.concat(frames));
      }
    }
    console.log('SENT PING ' + cmd[1]);
  } else if (cmd[0] === 'big-header') {
    // writes only the header of a binary frame announcing 1 GiB, which the client rejects with 1009
    const lenBuf = Buffer.alloc(8);
    lenBuf.writeBigUInt64BE(BigInt(1024 * 1024 * 1024));
    for (const s of streams) {
      if (!s.destroyed && s.writable) {
        s.write(Buffer.concat([Buffer.from([0x82, 127]), lenBuf]));
      }
    }
    console.log('SENT BIG-HEADER');
  }
});

const server = http2.createSecureServer({
  key,
  cert,
  allowHTTP1: false,
  settings: {
    enableConnectProtocol: true,
  },
});

server.on('stream', (stream, headers) => {
  if (headers[':method'] !== 'CONNECT' || headers[':protocol'] !== 'websocket') {
    stream.respond({ ':status': 404 });
    stream.end('not found');
    return;
  }
  stream.respond({
    ':status': 200,
    'sec-websocket-accept': acceptKey(String(headers['sec-websocket-key'])),
  });
  // nothing is read from the stream, and no flow-control credit is granted, until it is released
  if (!released) {
    stream.pause();
  }
  streams.push(stream);

  let recvBuffer = Buffer.alloc(0);
  let msgBuffer = null;
  stream.on('data', (chunk) => {
    recvBuffer = Buffer.concat([recvBuffer, chunk]);
    recvBuffer = parseFrames(recvBuffer, (frame) => {
      if (frame.opcode === 0xA) {
        console.log('PONG ' + frame.payload.toString('utf8'));
        return;
      }
      if (frame.opcode === 0x8) {
        const code = frame.payload.length >= 2 ? frame.payload.readUInt16BE(0) : 1005;
        console.log('CLOSE ' + code);
        stream.write(encodeFrame(0x8, frame.payload));
        stream.end();
        return;
      }
      if (frame.opcode === 0x1 || frame.opcode === 0x2) {
        msgBuffer = Buffer.from(frame.payload);
      } else if (frame.opcode === 0x0 && msgBuffer) {
        msgBuffer = Buffer.concat([msgBuffer, frame.payload]);
      } else {
        return;
      }
      if (frame.fin) {
        console.log('MSG ' + msgBuffer.length + ' ' + msgBuffer[0] + ' ' + msgBuffer[msgBuffer.length - 1]);
        msgBuffer = null;
      }
    });
  });
  stream.on('error', () => {});
  // the end of the client's side of the stream, after all its frames
  stream.on('end', () => {
    console.log('END');
  });
});

server.listen(port, '127.0.0.1', () => {
  console.log('READY ' + server.address().port);
});

// exits at once: closing the server would wait for the client's sessions to end
process.on('SIGTERM', () => {
  process.exit(0);
});
