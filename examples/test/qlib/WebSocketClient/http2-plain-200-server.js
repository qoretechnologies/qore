#!/usr/bin/env node
// http2-plain-200-server.js
// Copyright 2026 Qore Technologies, s.r.o.
// A TLS server that answers every request with 200 and a page, as a catch-all web UI handler does: an HTTP/2
// extended CONNECT (RFC 8441) gets the page and the end of the stream instead of a tunnel, and an HTTP/1.1 WebSocket
// Upgrade gets the page instead of 101.  Prints "READY <port>" when it listens (on a port that the system chooses).
// Usage: http2-plain-200-server.js <cert.pem> <key.pem> [headers-only]

'use strict';

const fs = require('fs');
const http2 = require('http2');

if (process.argv.length < 4) {
  console.error('Usage: http2-plain-200-server.js <cert.pem> <key.pem>');
  process.exit(2);
}

const page = '<html><body>index</body></html>';

const server = http2.createSecureServer({
  cert: fs.readFileSync(process.argv[2]),
  key: fs.readFileSync(process.argv[3]),
  allowHTTP1: true,
  settings: { enableConnectProtocol: true },
});

function answerWithPage(req, res) {
  res.writeHead(200, { 'content-type': 'text/html' });
  res.end(page);
}

// the compatibility API answers a CONNECT with 405 unless a 'connect' listener handles it; with the "headers-only"
// argument, a CONNECT gets a 200 response that ends the stream in its HEADERS frame (no body), so that the end of the
// stream arrives with the status
const headersOnly = process.argv[4] === 'headers-only';
server.on('connect', (req, res) => {
  if (headersOnly) {
    req.stream.respond({ ':status': 200 }, { endStream: true });
    return;
  }
  answerWithPage(req, res);
});
server.on('request', answerWithPage);

server.listen(0, '127.0.0.1', () => {
  process.stdout.write('READY ' + server.address().port + '\n');
});

process.on('SIGTERM', () => {
  server.close();
  process.exit(0);
});
