#!/usr/bin/env node
// http2-no-connect-protocol-server.js
// Copyright 2026 Qore Technologies, s.r.o.
// A TLS server that speaks HTTP/2 but does not advertise SETTINGS_ENABLE_CONNECT_PROTOCOL (RFC 8441), as nginx with
// "http2 on", and answers every HTTP/1.1 request (such as a WebSocket Upgrade) with 200 and a page.  Prints
// "READY <port>" when it listens (on a port that the system chooses).
// Usage: http2-no-connect-protocol-server.js <cert.pem> <key.pem>

'use strict';

const fs = require('fs');
const http2 = require('http2');

if (process.argv.length < 4) {
  console.error('Usage: http2-no-connect-protocol-server.js <cert.pem> <key.pem>');
  process.exit(2);
}

const server = http2.createSecureServer({
  cert: fs.readFileSync(process.argv[2]),
  key: fs.readFileSync(process.argv[3]),
  allowHTTP1: true,
});

server.on('request', (req, res) => {
  res.writeHead(200, { 'content-type': 'text/html' });
  res.end('<html><body>index</body></html>');
});

server.listen(0, '127.0.0.1', () => {
  process.stdout.write('READY ' + server.address().port + '\n');
});

process.on('SIGTERM', () => {
  server.close();
  process.exit(0);
});
