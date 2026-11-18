#!/usr/bin/env node
// http11-only-tls-server.js
// Copyright 2026 Qore Technologies, s.r.o.
// A TLS server that only speaks HTTP/1.1 and only negotiates ALPN "http/1.1": a client that offers "h2" alone gets a
// no_application_protocol alert, as from nginx on an "ssl" listener without "http2 on".  Answers every request with
// 200 and "http/1.1 ok".  Prints "READY <port>" when it listens (on a port that the system chooses).
// Usage: http11-only-tls-server.js <cert.pem> <key.pem>

'use strict';

const fs = require('fs');
const https = require('https');

if (process.argv.length < 4) {
  console.error('Usage: http11-only-tls-server.js <cert.pem> <key.pem>');
  process.exit(2);
}

// ALPNCallback (node >= 18.18 and 20.0) refuses a client that does not offer http/1.1 with the no_application_protocol
// alert (RFC 7301 section 3.2); with ALPNProtocols alone, some node versions (e.g. 18.17) complete the handshake without
// ALPN instead, which the test detects and skips
const server = https.createServer({
  cert: fs.readFileSync(process.argv[2]),
  key: fs.readFileSync(process.argv[3]),
  ALPNCallback: ({ protocols }) => (protocols.includes('http/1.1') ? 'http/1.1' : undefined),
}, (req, res) => {
  res.writeHead(200, { 'content-type': 'text/plain' });
  res.end('http/1.1 ok');
});

server.listen(0, '127.0.0.1', () => {
  process.stdout.write('READY ' + server.address().port + '\n');
});

process.on('SIGTERM', () => {
  server.close();
  process.exit(0);
});
