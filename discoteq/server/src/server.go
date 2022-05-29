package main

import (
	"flag"
	"errors"
	"fmt"
	"time"
	"sync"
	"log"
	"os/exec"
	"strings"
	"net/http"
	"encoding/json"
	"github.com/golang-jwt/jwt"
	"github.com/google/uuid"
	b64 "encoding/base64"
	"crypto/rand"
	"crypto/hmac"
	"crypto/sha256"
	"encoding/hex"
	"net/url"
)

var MY_IP string = "127.0.0.1"
var addr = flag.String("addr", ":8080", "http service address")
var no_launcher = flag.Bool("no-launcher", false, "Disable the admin launcher")
var rpcsock = flag.String("sock", "/launchersock/launchersock", "Unix socket for launcher")

var JWT_SECRET = []byte("52109852ccde123765935f15a51818ad3c3768067df7464f7181d2849726df95");

var ADMIN = "admin#13371337"

var POLL_LIFETIME = int64(300)
var POLL_CLEAN_THRESHOLD = 500

func randomHex(n int) (string, error) {
  bytes := make([]byte, n)
  if _, err := rand.Read(bytes); err != nil {
    return "", err
  }
  return hex.EncodeToString(bytes), nil
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

func ticketVerify(ticket_in string) (string,error) {
	/* ~waves hands~ magic */
	return ticket_in, nil
}

func hmacSign(message string, key []byte) string {
	mac := hmac.New(sha256.New, key)
	mac.Write([]byte(message))
	sig := mac.Sum(nil)
	return b64.RawStdEncoding.EncodeToString(sig)
}

func hmacVerify(message, signature string, key []byte) (bool,error) {
	sig, err := b64.RawStdEncoding.DecodeString(signature)
	if err != nil {
		return false, err
	}
	mac := hmac.New(sha256.New, key)
	mac.Write([]byte(message))
	expected := mac.Sum(nil)
	return hmac.Equal(expected, sig), nil
}

func sign_token(claims jwt.MapClaims) (string, error) {
	username := claims["username"].(string)
	sig := hmacSign(username, JWT_SECRET)
	sig = strings.ReplaceAll(sig, "+", "-")
	sig = strings.ReplaceAll(sig, "/", "_")

	username = url.QueryEscape(username)
	username = strings.ReplaceAll(username, ".", "%2E")

	token := username + "." + sig
	return token, nil
}

func verify_token(stoken string) (jwt.MapClaims, error) {
	parts := strings.Split(stoken, ".")
	log.Printf("token %s %v", stoken, parts);

	if len(parts) < 2 || len(parts) > 2 {
		log.Printf("Too many parts of token")
		return nil, errors.New("Invalid token format")
	}
	username, err := url.QueryUnescape(parts[0])
	if err != nil {
		return nil, err
	}
	sig := parts[1]
	sig = strings.ReplaceAll(sig, "-", "+")
	sig = strings.ReplaceAll(sig, "_", "/")


	good, err := hmacVerify(username, sig, JWT_SECRET)
	if err != nil {
		return nil, err
	}
	if !good {
		return nil, errors.New("Invalid signature on token")
	}

	return jwt.MapClaims{
		"username": username,
	}, nil
}

func auth_cookie(r *http.Request) (jwt.MapClaims, error) {
	cookie, err := r.Cookie("token")
	if err != nil {
		return nil, err
	}
	claims, err := verify_token(cookie.Value)
	if err != nil {
		return nil, err
	}
	return claims, nil
}

func respond_with_json(w http.ResponseWriter, data JSON) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	json.NewEncoder(w).Encode(data)
}

func respond_with_token(w http.ResponseWriter, claims jwt.MapClaims, data JSON) {
	tokenString, err := sign_token(claims)
	if err != nil {
		http.Error(w, "Internal Server Error", http.StatusInternalServerError)
		return
	}

	data["new_token"] = tokenString
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Set-Cookie", "token="+tokenString+";")
	w.WriteHeader(http.StatusOK)
	json.NewEncoder(w).Encode(data)
}

func login(w http.ResponseWriter, r *http.Request) {
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
	log.Printf("LOGIN Data %v", data)

	token, ok := data["token"]; if !ok {
		http.Error(w, "Bad Request", http.StatusBadRequest)
		return
	}
	claims, err := verify_token(token)
	if err != nil {
		http.Error(w, "Unauthorized", http.StatusUnauthorized)
		return
	}
	log.Printf("Claims %v", claims)
	user := claims["username"].(string)
	is_admin := user == ADMIN
	respond_with_token(w, claims, JSON{
		"username": user,
		"is_admin": is_admin,
	})
}

func get_token(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	claims, err := auth_cookie(r)
	if err != nil {
		http.Error(w, "Unauthorized", http.StatusUnauthorized)
		return
	}
	user := claims["username"].(string)
	is_admin := user == ADMIN
	respond_with_token(w, claims, JSON{
		"username": user,
		"is_admin": is_admin,
	})
}

var reg_cooldowns = make(map[string]int64)
var reg_mux sync.RWMutex
func get_cooldown(ticket string) (int64,bool) {
	reg_mux.RLock()
	defer reg_mux.RUnlock()
	t, ok := reg_cooldowns[ticket]
	return t,ok
}
func set_cooldown(ticket string, t int64) {
	reg_mux.Lock()
	defer reg_mux.Unlock()
	reg_cooldowns[ticket] = t
}


func register_client(w http.ResponseWriter, r *http.Request) {
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

	username, ok := data["username"]; if !ok {
		http.Error(w, "Bad Request", http.StatusBadRequest)
		return
	}
	if (len(username) == 0) {
		respond_with_json(w, JSON{
			"error":"Missing username",
		})
		return
	}
	if (len(username) > 32) {
		username = username[:32]
	}

	if (strings.Contains(username, "\x00")) {
		respond_with_json(w, JSON{
			"error":"Invalid username",
		})
		return
	}
	sticket, ok := data["ticket"]; if !ok {
		http.Error(w, "Bad Request", http.StatusBadRequest)
		return
	}
	if (len(sticket) == 0) {
		respond_with_json(w, JSON{
			"error":"Missing ticket",
		})
		return
	}

	ticket, err := ticketVerify(sticket)
	if err != nil {
		respond_with_json(w, JSON{
			"error":"Invalid ticket",
		})
		return
	}

	now := time.Now().Unix()

	cooldown_trigger := int64(5)
	cooldown_time := int64(30)

	cool_t, found := get_cooldown(ticket)
	log.Printf("GOT %v vs %v", cool_t , now)
	if len(ticket) > 0 && found {
		if cool_t < 0 {
			// Had cooldown
			cool_t = -cool_t
			if cool_t > now {
				// In cooldown
				diff := cool_t - now
				respond_with_json(w, JSON{
					"error":fmt.Sprintf("You are registering too fast, please do not brute force! (It won't get you anywhere trust me). Try again in %d seconds", diff),
				})
				return
			}
		} else {
			// Check for trigger
			diff := now - cool_t
			if diff < cooldown_trigger {
				// Trigger cooldown next time
				cool_t = now + cooldown_time
				set_cooldown(ticket, -cool_t)
				respond_with_json(w, JSON{
					"error":fmt.Sprintf("You are registering too fast, please do not brute force! (It won't get you anywhere trust me). Try again in %d seconds", cooldown_time),
				})
				return
			}
		}
	}
	set_cooldown(ticket, now)

	log.Printf("[TICKET] Registered %s with ticket: %s", username, ticket)

	for {
		rn, err := randomHex(4)
		if err != nil {
			http.Error(w, "Internal Server Error", http.StatusInternalServerError)
			return
		}

		name := username + "#" + rn
		if (name == ADMIN) {
			continue
		}

		username = name
		break
	}

	respond_with_token(w, jwt.MapClaims{
		"username": username,
	}, JSON{
		"username": username,
		"is_admin": false,
	})
}

type Poll struct {
	time int64
	options []string
	voted map[string]bool
	results map[string]uint16
	mtx sync.RWMutex
}

var all_polls = make(map[string]*Poll, 10)
var poll_mutex sync.RWMutex

func set_poll(poll_id string, poll *Poll) int {
	poll_mutex.Lock()
	defer poll_mutex.Unlock()
	all_polls[poll_id] = poll
	return len(all_polls)
}
func get_poll(poll_id string) (*Poll,bool) {
	poll_mutex.RLock()
	defer poll_mutex.RUnlock()
	v,ok := all_polls[poll_id]
	return v,ok
}

func get_query(r *http.Request, name string) string {
	query := r.URL.Query()
	l,ok := query[name]
	log.Printf("???? %v", r.URL)
	log.Printf("getting query %s %v %v", name, query, ok)
	if !ok {
		return ""
	}
	if len(l) == 0 {
		return ""
	}
	return l[0];
}

func get_flag(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	claims, err := auth_cookie(r)
	if err != nil {
		http.Error(w, "Unauthorized", http.StatusUnauthorized)
		return
	}
	user := claims["username"].(string)
	is_admin := user == ADMIN
	if !is_admin {
		json.NewEncoder(w).Encode(JSON{
			"error":"User is not admin :(",
		})
		return
	}

	data := make(map[string]string)
	err = json.NewDecoder(r.Body).Decode(&data)
	if err != nil {
		http.Error(w, "Bad request", http.StatusBadRequest)
		return
	}
	log.Printf("Data %v", data)

	json.NewEncoder(w).Encode(JSON{
		"flag":"flag{disco time}",
	})
	return
}

func create_poll(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	_, err := auth_cookie(r)
	if err != nil {
		http.Error(w, "Unauthorized", http.StatusUnauthorized)
		return
	}

	data := make(map[string][]string)
	err = json.NewDecoder(r.Body).Decode(&data)
	if err != nil {
		http.Error(w, "Bad request", http.StatusBadRequest)
		return
	}
	log.Printf("Data %v", data)
	opts, ok := data["options"]
	if !ok || len(opts) == 0 {
		http.Error(w, "Bad request", http.StatusBadRequest)
		return
	}

	if (len(opts) > 4) {
		opts = opts[:4]
	}
	for i, o := range opts {
		if (len(o) > 64) {
			opts[i] = o[:64]
		}
	}

	poll_id := uuid.New().String()
	poll := &Poll{
		options: opts,
		voted: make(map[string]bool),
		results: make(map[string]uint16, len(opts)),
		time: time.Now().Unix(),
	}
	for _,name := range opts {
		poll.results[name] = 0
	}
	num_polls := set_poll(poll_id, poll)

	json.NewEncoder(w).Encode(JSON{
		"poll_id": poll_id,
	})

	if (num_polls > POLL_CLEAN_THRESHOLD) {
		clean_polls()
	}
}

func poll_options(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	_, err := auth_cookie(r)
	if err != nil {
		http.Error(w, "Unauthorized", http.StatusUnauthorized)
		return
	}

	poll_id := get_query(r, "poll")
	poll, ok := get_poll(poll_id)
	log.Printf("Poll %s %v", poll_id, poll)
	if !ok {
		json.NewEncoder(w).Encode(JSON{
			"error":"Missing poll",
		})
		return
	}

	poll.mtx.RLock()
	defer poll.mtx.RUnlock()
	options := make([]JSON, len(poll.options))
	for i,op := range poll.options {
		options[i] = JSON{
			"text": op,
			"count": fmt.Sprintf("%d", poll.results[op]),
		}
	}
	json.NewEncoder(w).Encode(JSON{
		"options": options,
		"token": "foo",
	})
}

func update_poll(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	claims, err := auth_cookie(r)
	if err != nil {
		http.Error(w, "Unauthorized", http.StatusUnauthorized)
		return
	}
	user := claims["username"].(string)

	poll_id := get_query(r, "poll")
	log.Printf("Poll %s", poll_id)
	poll, ok := get_poll(poll_id);
	if !ok {
		json.NewEncoder(w).Encode(JSON{
			"error":"Missing poll",
		})
		return
	}
	poll.mtx.Lock()
	defer poll.mtx.Unlock()

	if did, ok := poll.voted[user]; did || ok {
		json.NewEncoder(w).Encode(JSON{
			"error":"Already voted",
		})
		return
	}

	data := make(map[string]string)
	err = json.NewDecoder(r.Body).Decode(&data)
	if err != nil {
		http.Error(w, "Bad request", http.StatusBadRequest)
		return
	}
	log.Printf("Data %v", data)
	sel, ok := data["selection"]
	if !ok {
		http.Error(w, "Bad request", http.StatusBadRequest)
		return
	}
	current, ok := poll.results[sel]
	if !ok {
		json.NewEncoder(w).Encode(JSON{
			"error":"Invalid selection",
		})
		return
	}
	current += 1
	log.Printf("Updating poll %s with %s=%v",poll_id, sel, current)
	poll.results[sel] = current
	poll.voted[user] = true
	poll.time = time.Now().Unix()

	json.NewEncoder(w).Encode(JSON{
		"result":"submitted",
	})
	return
}

func clean_polls() {
	poll_mutex.Lock()
	defer poll_mutex.Unlock()
	now := time.Now().Unix()
	for poll_id, poll := range all_polls {
		if poll.time + POLL_LIFETIME < now {
			delete(all_polls, poll_id)
		}
	}
}


func get_my_ip() (string, error) {
	my_ip, err := exec.Command("curl", "https://ifconfig.me").Output()
	if err != nil {
		return "127.0.0.1", err
	}
	return string(my_ip), nil
}

func main() {
	flag.Parse()

	my_ip, err := get_my_ip()
	MY_IP = my_ip
	if err != nil {
		fmt.Println("Failed to resolve ip", err)
	} else {
		fmt.Println("MY IP IS %s", MY_IP);
	}

	var launcher *Launcher
	if !*no_launcher {
		launcher = newLauncher()
		go launcher.run()
	}

	hub := newHub()
	hub.launcher = launcher
	go hub.run()

	fs := http.FileServer(http.Dir("./static"))
	widget_fs := http.FileServer(http.Dir("./widgets"))
	http.Handle("/", fs)
	http.Handle("/widget/", http.StripPrefix("/widget/",widget_fs))
	http.HandleFunc("/api/register", register_client)
	http.HandleFunc("/api/login", login)
	http.HandleFunc("/api/token", get_token)
	http.HandleFunc("/api/flag", get_flag)

	http.HandleFunc("/api/poll/options", poll_options)
	http.HandleFunc("/api/poll/create", create_poll)
	http.HandleFunc("/api/poll/vote", update_poll)
	http.HandleFunc("/api/ws/", func(w http.ResponseWriter, r *http.Request) {
		serveWs(hub, w, r)
	})
	err = http.ListenAndServe(*addr, nil)
	if err != nil {
		log.Fatal("ListenAndServe: ", err)
	}
}
