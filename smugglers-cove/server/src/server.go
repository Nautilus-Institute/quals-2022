package main

import (
	"flag"
	"log"
	"fmt"
	"net/http"
	"time"
	"encoding/json"
	"encoding/hex"
	"crypto/rand"
	"sync"
)

var addr = flag.String("addr", ":8080", "http service address")
var rpcsock = flag.String("sock", "/launchersock/launchersock", "Unix socket for launcher")

var RATE_LIMIT = int64(10)
var MAX_LEN = 433

var launcher *Launcher

func randomHex(n int) string {
  bytes := make([]byte, n)
  if _, err := rand.Read(bytes); err != nil {
    log.Printf("UNABLE TO READ RANDOM BYTES %v", err)
	panic(err)
  }
  return hex.EncodeToString(bytes)
}

func respond_with_json(w http.ResponseWriter, data JSON) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	json.NewEncoder(w).Encode(data)
}
func respond_with_text(w http.ResponseWriter, data string) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	w.Write([]byte(data))
}

var waiting = make(map[string]int64)
var waiting_mutex sync.RWMutex
func add_waiting(uid string) {
	waiting_mutex.Lock()
	defer waiting_mutex.Unlock()
	waiting[uid] = time.Now().Unix()
}
func get_waiting(uid string) (int64, bool) {
	waiting_mutex.RLock()
	defer waiting_mutex.RUnlock()
	v,ok := waiting[uid]
	return v, ok
}
func done_waiting(uid string) {
	waiting_mutex.Lock()
	defer waiting_mutex.Unlock()
	delete(waiting, uid)
}

var _last_times_mux sync.RWMutex
var last_times map[string]int64

func get_last_time(ticket string) (int64, bool) {
	_last_times_mux.RLock()
	defer _last_times_mux.RUnlock()
	t,err := last_times[ticket]
	return t,err
}

func set_last_time(ticket string, time int64) {
	_last_times_mux.Lock()
	defer _last_times_mux.Unlock()
	last_times[ticket] = time
}

func ticketVerify(ticket_in string) (string,error) {
	/* ~waves hands~ magic */
	return ticket_in, nil
}

func handle_run_code(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	data := make(map[string]string)
	err := json.NewDecoder(r.Body).Decode(&data)
	if err != nil {
		http.Error(w, "Bad request", http.StatusBadRequest)
		return
	}
	log.Printf("Data %v", data)

	ticket_in, ok := data["ticket"]
	if !ok {
		http.Error(w, "Bad Request", http.StatusBadRequest)
		return
	}

	ticket, err := ticketVerify(ticket_in)

	if err != nil {
		respond_with_json(w, JSON{
			"success": false,
			"error": "Invalid ticket",
		});
		return
	}

	code, ok := data["code"]; if !ok {
		http.Error(w, "Bad Request", http.StatusBadRequest)
		return
	}
	if len(code) > MAX_LEN {
		respond_with_json(w, JSON{
			"success": false,
			"error": fmt.Sprintf("Code too long (>%u)", MAX_LEN),
		});
		return
	}

	now := time.Now().Unix()

	if RATE_LIMIT > 0 {
		last_time, ok := get_last_time(ticket)
		if ok {
			next := last_time + RATE_LIMIT
			if next > now {
				respond_with_json(w, JSON{
					"success": false,
					"error": fmt.Sprintf("Please wait %d seconds before running again", uint(next - now)),
				});
				return
			}
		}
	}
	set_last_time(ticket, now)

	log.Printf("launching code!")
	uid := randomHex(16)
	did_run,err := launcher.launch(uid, code, ticket)
	if err != nil || !did_run {
		respond_with_json(w, JSON{
			"success": false,
			"error": "Error running code",
		});
		return
	}
	add_waiting(uid)

	respond_with_json(w, JSON{
		"success": did_run,
		"runid": uid,
	});
}

func handle_get_output(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	uid_l, ok := r.URL.Query()["runid"]
	if !ok {
		http.Error(w, "Bad request", http.StatusBadRequest)
		return
	}
	if len(uid_l) != 1 {
		http.Error(w, "Bad request", http.StatusBadRequest)
		return
	}
	uid := uid_l[0]
	if len(uid) > 100 {
		uid = uid[:100]
	}
	log.Printf("Checking %s", uid)
	t, ok := get_waiting(uid)
	log.Printf("-> %v %v", t, ok)
	if !ok {
		respond_with_json(w, JSON{
			"output": nil,
			"waiting": false,
		});
		return
	}

	nt := time.Now().Unix()
	if t + 5 > nt {
		respond_with_json(w, JSON{
			"output": nil,
			"waiting": true,
		});
		return
	}

	ready,res,err := launcher.get_output(uid)
	if err != nil {
		done_waiting(uid)
		respond_with_json(w, JSON{
			"output": nil,
			"waiting": false,
		});
		return
	}
	if !ready {
		add_waiting(uid) // update timer
		respond_with_json(w, JSON{
			"output": nil,
			"waiting": true,
		});
		return
	}
	done_waiting(uid)
	respond_with_json(w, JSON{
		"output": res,
		"waiting": false,
	});
	return
}

func serveHome(w http.ResponseWriter, r *http.Request) {
	log.Println(r.URL)
	if r.URL.Path != "/" {
		http.Error(w, "Not found", http.StatusNotFound)
		return
	}
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	http.ServeFile(w, r, "home.html")
}

func main() {
	flag.Parse()

	last_times = make(map[string]int64)

	launcher = newLauncher()
	go launcher.run()

	fs := http.FileServer(http.Dir("./static"))
	http.Handle("/", fs)
	http.HandleFunc("/run", handle_run_code)
	http.HandleFunc("/output", handle_get_output)

	err := http.ListenAndServe(*addr, nil)
	if err != nil {
		log.Fatal("ListenAndServe: ", err)
	}
}
