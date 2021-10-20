package main

import (
	"flag"
	"log"
	"os"
	"runtime"
	"runtime/trace"
	"sync"
	"time"
)

var mutex sync.Mutex

func counter(wg *sync.WaitGroup) {
	wg.Done()
	slice := []int{0}
	c := 1
	for i := 0; i < 100000; i++ {
		c = i + 1 + 2 + 3 + 4 + 5
		slice = append(slice, c)
	}
}

func main() {
	runtime.GOMAXPROCS(5)
	var traceProfile = flag.String("traceprofile", "", "write trace profile to file")
	flag.Parse()

	if *traceProfile != "" {
		f, err := os.Create(*traceProfile)
		if err != nil {
			log.Fatal(err)
		}
		trace.Start(f)
		defer f.Close()
		defer trace.Stop()
	}

	var wg sync.WaitGroup
	wg.Add(3)
	for i := 0; i < 3; i++ {
		mutex.Lock()
		go counter(&wg)
		time.Sleep(time.Millisecond)
		mutex.Unlock()
	}
	wg.Wait()
}
