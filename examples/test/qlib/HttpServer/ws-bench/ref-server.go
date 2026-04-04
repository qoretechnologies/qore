// Reference WebSocket + HTTP echo server in Go (gorilla/websocket + net/http).
// Used as a baseline for comparing Qore async I/O performance.
//
// Usage:
//
//	go run ref-server.go -port 0 -port-file /tmp/ref-port
package main

import (
	"flag"
	"fmt"
	"net"
	"net/http"
	"os"
	"time"

	"github.com/gorilla/websocket"
)

var upgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool { return true },
}

func wsHandler(w http.ResponseWriter, r *http.Request) {
	c, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		return
	}
	defer c.Close()
	for {
		mt, msg, err := c.ReadMessage()
		if err != nil {
			break
		}
		if err := c.WriteMessage(mt, msg); err != nil {
			break
		}
	}
}

func httpHandler(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "text/plain")
	w.WriteHeader(200)
	w.Write([]byte("OK"))
}

func main() {
	port := flag.Int("port", 0, "listen port (0 = auto)")
	portFile := flag.String("port-file", "", "write bound port to this file")
	duration := flag.Int("duration", 0, "run for N seconds (0 = forever)")
	flag.Parse()

	mux := http.NewServeMux()
	mux.HandleFunc("/ws", wsHandler)
	mux.HandleFunc("/", httpHandler)

	ln, err := net.Listen("tcp", fmt.Sprintf("127.0.0.1:%d", *port))
	if err != nil {
		fmt.Fprintf(os.Stderr, "listen: %v\n", err)
		os.Exit(1)
	}
	addr := ln.Addr().(*net.TCPAddr)

	if *portFile != "" {
		os.WriteFile(*portFile, []byte(fmt.Sprintf("%d\n", addr.Port)), 0644)
	}
	fmt.Fprintf(os.Stderr, "Go reference server on port %d\n", addr.Port)

	go http.Serve(ln, mux)

	if *duration > 0 {
		time.Sleep(time.Duration(*duration) * time.Second)
	} else {
		select {}
	}
}
