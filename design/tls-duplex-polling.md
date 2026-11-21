# Duplex TLS polling

An HTTP/2 client can receive a response on one stream while its peer has stopped reading an upload on another. A full local TCP send buffer must leave the read half of the connection usable. Each application poll pass services both directions and reports the directions requested by the transport. The HTTP/2 server can dispatch an already completed request while retaining output for its next pass.

## OpenSSL retries

OpenSSL requires a retry of an incomplete write with the same contents and length. Its moving-write-buffer mode permits the pointer to move. A write can need input and a read can need output during TLS protocol processing. On those paths the poller retains the operation and its buffer across retries. An ordinary empty read on an established connection subscribes to readability while leaving the application free to write. Serializing that empty read ahead of all writes would deadlock a client whose peer is waiting for the first request. See the [SSL_get_error](https://docs.openssl.org/3.5/man3/SSL_get_error/) and [SSL_write](https://docs.openssl.org/3.5/man3/SSL_write/) documentation.

## Transport backpressure

The initial write goes directly through the socket BIO. If it would block, the network-facing TLS layer can finish writing the current record into a bounded BIO pair and retain its ciphertext until the socket accepts it. The original read BIO stays on the socket. For a TLS connection through a TLS proxy, the outer TLS session owns the bytes to be sent to the network. The output staging limit is 64 KiB. The staging BIO is allocated on demand and released once drained.

HTTP/2 receive operations borrow a read buffer from their I/O thread. A crossed TLS retry keeps that buffer with the session until its read completes.

The application send operation retains its outstanding plaintext count until the transport accepts the corresponding ciphertext. An aborted connection discards any undrained suffix. Socket and TLS errors propagate to the owner of the operation. HTTP/2 send-completion watches can advance only up to data flushed to the transport.

## Poll operations

* The HTTP/1 client drives its request writer and response reader in each full-duplex pass. The HTTP/1 server's send-and-read-header operations sequence a response and the *next* request on that connection. Unlike independent HTTP/2 streams, pipelined HTTP/1 responses must be written in request order.
* The HTTP/2 client multiplex operation combines the write and read readiness masks. The HTTP/2 server attempts its queued output and dispatches ready requests even when that attempt blocks. Standalone streaming and flush operations also process input for their flow-control windows. A buffered response writer only writes its assigned output. The connection reader handles inbound requests and control frames.
* WebSockets on HTTP/1 have independent frame reader and writer operations. Their parent subscribes to the union of the actual child readiness masks. WebSockets on HTTP/2 use the same multiplexed DATA path as other streams.
* The established QUIC client and server receive UDP datagrams before flushing pending packets. The UDP send queue and pacing timer remain independent of receive readiness. The per-response sends are writers for a connection whose receive operation owns the UDP input.
* The generic send, receive, peek, and handshake operations return the transport direction to their owner. A standalone send or receive operates on one half of the application conversation. The shared TLS transport distinguishes waiting for a socket write from waiting for OpenSSL to finish an operation.
