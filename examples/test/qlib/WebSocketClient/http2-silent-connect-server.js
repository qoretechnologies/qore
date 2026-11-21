#!/usr/bin/env node
// http2-silent-connect-server.js
// Copyright 2026 Qore Technologies, s.r.o.
// A TLS server that advertises RFC 8441 extended CONNECT over HTTP/2 but never answers a CONNECT, and accepts an
// HTTP/1.1 WebSocket Upgrade with 101: a WebSocket client in the "auto" mode must fall back to the HTTP/1.1 Upgrade
// when its CONNECT gets no answer (qore#6850).  Prints "READY <port>" when it listens (on a port that the system
// chooses).
// Usage: http2-silent-connect-server.js <cert.pem> <key.pem>

'use strict';

const crypto = require('crypto');
const fs = require('fs');
const http2 = require('http2');

if (process.argv.length < 4) {
  console.error('Usage: http2-silent-connect-server.js <cert.pem> <key.pem>');
  process.exit(2);
}

const server = http2.createSecureServer({
  cert: fs.readFileSync(process.argv[2]),
  key: fs.readFileSync(process.argv[3]),
  allowHTTP1: true,
  settings: { enableConnectProtocol: true },
});

// a CONNECT gets no answer; any other HTTP/2 request gets 404
server.on('stream', (stream, headers) => {
  if (headers[':method'] === 'CONNECT') {
    return;
  }
  stream.respond({ ':status': 404 });
  stream.end();
});

// the HTTP/1.1 WebSocket Upgrade is accepted; the connection is kept open until the client closes it
server.on('upgrade', (req, socket) => {
  const accept = crypto.createHash('sha1')
    .update(req.headers['sec-websocket-key'] + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11')
    .digest('base64');
  socket.write('HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n'
    + 'Sec-WebSocket-Accept: ' + accept + '\r\n\r\n');
  socket.on('error', () => {});
});

server.listen(0, '127.0.0.1', () => {
  console.log('READY ' + server.address().port);
});
