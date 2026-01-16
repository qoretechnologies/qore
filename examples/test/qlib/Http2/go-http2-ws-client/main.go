// Copyright 2026 Qore Technologies, s.r.o.
// Minimal RFC 8441 WebSocket-over-HTTP/2 client for server verification.

package main

import (
	"bytes"
	"crypto/rand"
	"crypto/sha1"
	"crypto/tls"
	"encoding/base64"
	"errors"
	"flag"
	"fmt"
	"io"
	"net"
	"os"
	"time"

	"golang.org/x/net/http2"
	"golang.org/x/net/http2/hpack"
)

const (
	magicGUID                                    = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
	settingEnableConnectProtocol http2.SettingID = 0x8
)

func main() {
	host := flag.String("host", "", "host")
	port := flag.Int("port", 0, "port")
	path := flag.String("path", "/", "path")
	flag.Parse()

	if *host == "" || *port == 0 || *path == "" {
		fmt.Fprintln(os.Stderr, "Usage: go-http2-ws-client -host <host> -port <port> -path <path>")
		os.Exit(2)
	}

	if err := run(*host, *port, *path); err != nil {
		fmt.Fprintln(os.Stderr, err.Error())
		os.Exit(1)
	}
}

func run(host string, port int, path string) error {
	addr := fmt.Sprintf("%s:%d", host, port)
	conn, err := tls.Dial("tcp", addr, &tls.Config{
		InsecureSkipVerify: true,
		NextProtos:         []string{"h2"},
		ServerName:         host,
	})
	if err != nil {
		return fmt.Errorf("tls dial failed: %w", err)
	}
	defer conn.Close()

	if err := conn.SetDeadline(time.Now().Add(12 * time.Second)); err != nil {
		return fmt.Errorf("set deadline failed: %w", err)
	}

	framer := http2.NewFramer(conn, conn)
	if _, err := conn.Write([]byte(http2.ClientPreface)); err != nil {
		return fmt.Errorf("write preface failed: %w", err)
	}

	if err := framer.WriteSettings(http2.Setting{
		ID:  settingEnableConnectProtocol,
		Val: 1,
	}); err != nil {
		return fmt.Errorf("write settings failed: %w", err)
	}

	if err := readAndAckServerSettings(framer); err != nil {
		return err
	}

	wsKey, err := randomBase64(16)
	if err != nil {
		return err
	}
	expectedAccept := computeAccept(wsKey)

	headers, err := encodeConnectHeaders(host, port, path, wsKey)
	if err != nil {
		return err
	}

	if err := framer.WriteHeaders(http2.HeadersFrameParam{
		StreamID:      1,
		BlockFragment: headers,
		EndStream:     false,
		EndHeaders:    true,
	}); err != nil {
		return fmt.Errorf("write headers failed: %w", err)
	}

	respHeaders, err := readResponseHeaders(framer, 1)
	if err != nil {
		return err
	}

	status, ok := respHeaders[":status"]
	if !ok || status != "200" {
		return fmt.Errorf("CONNECT failed with status %s", status)
	}
	accept, ok := respHeaders["sec-websocket-accept"]
	if !ok || accept != expectedAccept {
		return errors.New("Sec-WebSocket-Accept mismatch")
	}

	if err := sendWsFrame(framer, 1, 0x1, []byte("go-test"), false); err != nil {
		return err
	}

	msgs, err := readWsMessages(framer, 1, 8*time.Second)
	if err != nil {
		return err
	}

	if len(msgs) < 2 || msgs[0] != "welcome" || msgs[1] != "go-test" {
		return fmt.Errorf("unexpected messages: %v", msgs)
	}

	if err := sendWsFrame(framer, 1, 0x8, []byte{}, true); err != nil {
		return err
	}

	return nil
}

func randomBase64(n int) (string, error) {
	buf := make([]byte, n)
	if _, err := rand.Read(buf); err != nil {
		return "", fmt.Errorf("random read failed: %w", err)
	}
	return base64.StdEncoding.EncodeToString(buf), nil
}

func computeAccept(key string) string {
	sum := sha1.Sum([]byte(key + magicGUID))
	return base64.StdEncoding.EncodeToString(sum[:])
}

func encodeConnectHeaders(host string, port int, path string, wsKey string) ([]byte, error) {
	var b bytes.Buffer
	enc := hpack.NewEncoder(&b)
	fields := []hpack.HeaderField{
		{Name: ":method", Value: "CONNECT"},
		{Name: ":path", Value: path},
		{Name: ":scheme", Value: "https"},
		{Name: ":authority", Value: fmt.Sprintf("%s:%d", host, port)},
		{Name: ":protocol", Value: "websocket"},
		{Name: "sec-websocket-key", Value: wsKey},
		{Name: "sec-websocket-version", Value: "13"},
	}
	for _, hf := range fields {
		if err := enc.WriteField(hf); err != nil {
			return nil, fmt.Errorf("hpack encode failed: %w", err)
		}
	}
	return b.Bytes(), nil
}

func readAndAckServerSettings(f *http2.Framer) error {
	for {
		frame, err := f.ReadFrame()
		if err != nil {
			return fmt.Errorf("read settings failed: %w", err)
		}
		if s, ok := frame.(*http2.SettingsFrame); ok {
			if s.IsAck() {
				continue
			}
			if err := f.WriteSettingsAck(); err != nil {
				return fmt.Errorf("write settings ack failed: %w", err)
			}
			return nil
		}
	}
}

func readResponseHeaders(f *http2.Framer, streamID uint32) (map[string]string, error) {
	dec := hpack.NewDecoder(4096, nil)
	headers := map[string]string{}
	var block bytes.Buffer
	for {
		frame, err := f.ReadFrame()
		if err != nil {
			return nil, fmt.Errorf("read frame failed: %w", err)
		}
		switch fr := frame.(type) {
		case *http2.HeadersFrame:
			if fr.StreamID != streamID {
				continue
			}
			block.Write(fr.HeaderBlockFragment())
			if fr.HeadersEnded() {
				if err := decodeHeaderBlock(dec, block.Bytes(), headers); err != nil {
					return nil, err
				}
				return headers, nil
			}
		case *http2.ContinuationFrame:
			if fr.StreamID != streamID {
				continue
			}
			block.Write(fr.HeaderBlockFragment())
			if fr.HeadersEnded() {
				if err := decodeHeaderBlock(dec, block.Bytes(), headers); err != nil {
					return nil, err
				}
				return headers, nil
			}
		}
	}
}

func decodeHeaderBlock(dec *hpack.Decoder, block []byte, headers map[string]string) error {
	dec.SetEmitFunc(func(f hpack.HeaderField) {
		headers[f.Name] = f.Value
	})
	if _, err := dec.Write(block); err != nil {
		return fmt.Errorf("hpack decode failed: %w", err)
	}
	return nil
}

func sendWsFrame(f *http2.Framer, streamID uint32, opcode byte, payload []byte, endStream bool) error {
	frame := encodeWsFrame(opcode, payload)
	if err := f.WriteData(streamID, endStream, frame); err != nil {
		return fmt.Errorf("write data failed: %w", err)
	}
	return nil
}

func encodeWsFrame(opcode byte, payload []byte) []byte {
	var b bytes.Buffer
	first := byte(0x80 | (opcode & 0x0f))
	b.WriteByte(first)
	pl := len(payload)
	switch {
	case pl < 126:
		b.WriteByte(byte(pl))
	case pl < 65536:
		b.WriteByte(126)
		b.WriteByte(byte((pl >> 8) & 0xff))
		b.WriteByte(byte(pl & 0xff))
	default:
		b.WriteByte(127)
		for i := 7; i >= 0; i-- {
			b.WriteByte(byte((uint64(pl) >> (uint(i) * 8)) & 0xff))
		}
	}
	b.Write(payload)
	return b.Bytes()
}

func readWsMessages(f *http2.Framer, streamID uint32, timeout time.Duration) ([]string, error) {
	deadline := time.Now().Add(timeout)
	var wsBuf bytes.Buffer
	var messages []string

	for len(messages) < 2 {
		if time.Now().After(deadline) {
			return nil, errors.New("timeout waiting for websocket messages")
		}
		frame, err := f.ReadFrame()
		if err != nil {
			if ne, ok := err.(net.Error); ok && ne.Timeout() {
				continue
			}
			if err == io.EOF {
				return nil, errors.New("connection closed before messages received")
			}
			return nil, fmt.Errorf("read frame failed: %w", err)
		}
		df, ok := frame.(*http2.DataFrame)
		if !ok || df.StreamID != streamID {
			continue
		}
		wsBuf.Write(df.Data())
		for {
			msg, remaining, done, err := tryParseWsText(wsBuf.Bytes())
			if err != nil {
				return nil, err
			}
			if !done {
				break
			}
			messages = append(messages, msg)
			wsBuf.Reset()
			wsBuf.Write(remaining)
		}
	}
	return messages, nil
}

func tryParseWsText(buf []byte) (string, []byte, bool, error) {
	if len(buf) < 2 {
		return "", buf, false, nil
	}
	b0 := buf[0]
	b1 := buf[1]
	opcode := b0 & 0x0f
	if opcode != 0x1 {
		return "", buf, false, nil
	}
	masked := (b1 & 0x80) != 0
	if masked {
		return "", buf, false, errors.New("masked server frame")
	}
	length := int(b1 & 0x7f)
	offset := 2
	if length == 126 {
		if len(buf) < 4 {
			return "", buf, false, nil
		}
		length = int(buf[2])<<8 | int(buf[3])
		offset = 4
	} else if length == 127 {
		if len(buf) < 10 {
			return "", buf, false, nil
		}
		var v uint64
		for i := 0; i < 8; i++ {
			v = (v << 8) | uint64(buf[2+i])
		}
		if v > uint64(^uint(0)>>1) {
			return "", buf, false, errors.New("frame too large")
		}
		length = int(v)
		offset = 10
	}
	if len(buf) < offset+length {
		return "", buf, false, nil
	}
	payload := buf[offset : offset+length]
	return string(payload), buf[offset+length:], true, nil
}
