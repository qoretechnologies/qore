// WebSocket echo benchmark client.
//
// Opens N concurrent WebSocket connections to a server and sends M messages
// per connection, measuring throughput and latency.
//
// Usage:
//
//	go run main.go -url ws://127.0.0.1:PORT/ws -c 50 -n 1000 -size 128
package main

import (
	"flag"
	"fmt"
	"math"
	"os"
	"sort"
	"sync"
	"sync/atomic"
	"time"

	"github.com/gorilla/websocket"
)

func main() {
	url := flag.String("url", "ws://127.0.0.1:8080/ws", "WebSocket server URL")
	connections := flag.Int("c", 10, "number of concurrent connections")
	messages := flag.Int("n", 1000, "messages per connection")
	msgSize := flag.Int("size", 128, "message payload size in bytes")
	warmup := flag.Int("warmup", 100, "warmup messages per connection (not measured)")
	flag.Parse()

	payload := make([]byte, *msgSize)
	for i := range payload {
		payload[i] = byte('A' + (i % 26))
	}

	fmt.Fprintf(os.Stderr, "Benchmark: %d connections x %d messages, %d byte payload\n",
		*connections, *messages, *msgSize)
	fmt.Fprintf(os.Stderr, "Target: %s\n", *url)

	// Collect per-message latencies
	type result struct {
		latencies []time.Duration
		errors    int
	}

	var totalErrors atomic.Int64
	var totalMsgs atomic.Int64
	results := make([]result, *connections)
	var wg sync.WaitGroup

	// Warmup phase
	if *warmup > 0 {
		fmt.Fprintf(os.Stderr, "Warming up (%d messages per connection)...\n", *warmup)
		var warmWg sync.WaitGroup
		for i := 0; i < *connections; i++ {
			warmWg.Add(1)
			go func(id int) {
				defer warmWg.Done()
				c, _, err := websocket.DefaultDialer.Dial(*url, nil)
				if err != nil {
					fmt.Fprintf(os.Stderr, "warmup connect %d: %v\n", id, err)
					return
				}
				defer c.Close()
				for j := 0; j < *warmup; j++ {
					if err := c.WriteMessage(websocket.TextMessage, payload); err != nil {
						break
					}
					if _, _, err := c.ReadMessage(); err != nil {
						break
					}
				}
			}(i)
		}
		warmWg.Wait()
	}

	// Benchmark phase
	fmt.Fprintf(os.Stderr, "Running benchmark...\n")
	start := time.Now()

	for i := 0; i < *connections; i++ {
		wg.Add(1)
		go func(id int) {
			defer wg.Done()
			r := &results[id]
			r.latencies = make([]time.Duration, 0, *messages)

			c, _, err := websocket.DefaultDialer.Dial(*url, nil)
			if err != nil {
				fmt.Fprintf(os.Stderr, "connect %d: %v\n", id, err)
				r.errors = *messages
				totalErrors.Add(int64(*messages))
				return
			}
			defer c.Close()

			for j := 0; j < *messages; j++ {
				t0 := time.Now()
				if err := c.WriteMessage(websocket.TextMessage, payload); err != nil {
					r.errors++
					totalErrors.Add(1)
					continue
				}
				if _, _, err := c.ReadMessage(); err != nil {
					r.errors++
					totalErrors.Add(1)
					continue
				}
				r.latencies = append(r.latencies, time.Since(t0))
				totalMsgs.Add(1)
			}
		}(i)
	}
	wg.Wait()
	elapsed := time.Since(start)

	// Aggregate latencies
	var all []time.Duration
	for _, r := range results {
		all = append(all, r.latencies...)
	}
	sort.Slice(all, func(i, j int) bool { return all[i] < all[j] })

	totalOk := len(all)
	if totalOk == 0 {
		fmt.Fprintf(os.Stderr, "No successful messages\n")
		os.Exit(1)
	}

	var sum time.Duration
	for _, d := range all {
		sum += d
	}
	avg := sum / time.Duration(totalOk)
	p50 := all[totalOk*50/100]
	p99 := all[totalOk*99/100]
	p999 := all[int(math.Min(float64(totalOk*999/1000), float64(totalOk-1)))]
	maxLat := all[totalOk-1]

	rps := float64(totalOk) / elapsed.Seconds()

	fmt.Printf("%-20s %s\n", "Connections:", fmt.Sprintf("%d", *connections))
	fmt.Printf("%-20s %s\n", "Messages/conn:", fmt.Sprintf("%d", *messages))
	fmt.Printf("%-20s %s\n", "Payload:", fmt.Sprintf("%d bytes", *msgSize))
	fmt.Printf("%-20s %s\n", "Duration:", elapsed.Round(time.Millisecond).String())
	fmt.Printf("%-20s %s\n", "Total messages:", fmt.Sprintf("%d", totalOk))
	fmt.Printf("%-20s %s\n", "Errors:", fmt.Sprintf("%d", totalErrors.Load()))
	fmt.Printf("%-20s %.0f\n", "Messages/sec:", rps)
	fmt.Printf("%-20s %s\n", "Avg latency:", avg.Round(time.Microsecond).String())
	fmt.Printf("%-20s %s\n", "p50 latency:", p50.Round(time.Microsecond).String())
	fmt.Printf("%-20s %s\n", "p99 latency:", p99.Round(time.Microsecond).String())
	fmt.Printf("%-20s %s\n", "p99.9 latency:", p999.Round(time.Microsecond).String())
	fmt.Printf("%-20s %s\n", "Max latency:", maxLat.Round(time.Microsecond).String())
}
