package main

import (
	"fmt"
	"log"
	"math"
	"time"
	"sync"
	"github.com/golang-jwt/jwt"
)


type Message struct {
	from *Client
	msg map[string]interface{}
}

type Hub struct {
	activeClients map[*Client]bool
	clients map[string]*Client
	admin_cooldowns map[string]int64
	estimated_queue_time int64
	register chan *Client
	unregister chan *Client
	handle chan Message
	launcher *Launcher
	admins []*Admin
	pendingAdminToken string
	pendingAdminTokenTimestamp int64
	pendingAdminMessages []JSON
	mux sync.RWMutex
}

var NUM_WORKERS = 4
var TIME_PER = int64(7)
var LIFETIME = int64(295)

type Admin struct {
	index uint32
	client *Client
	launch_time int64
	estimated_end_time int64
	specific_ticket string
	pendingMessages []JSON
}

func new_admin(i int) *Admin {
	return &Admin{
		pendingMessages: make([]JSON, 0, 1),
		index: uint32(i),
	}
}

func (admin *Admin) ready(now int64) bool {
	if admin.client == nil {
		return false
	}
	if admin.launch_time + 3 > now  {
		return false
	}
	return true
}

func (admin *Admin) has_space() bool {
	if admin.client == nil {
		return false
	}
	et := admin.estimated_end_time
	spent := et - admin.launch_time

	return spent + TIME_PER < LIFETIME
}

func newHub() *Hub {
	h := &Hub{
		handle: make(chan Message),
		register: make(chan *Client),
		unregister: make(chan *Client),
		activeClients: make(map[*Client]bool),
		clients: make(map[string]*Client),
		admin_cooldowns: make(map[string]int64),
		admins: make([]*Admin, NUM_WORKERS),
		pendingAdminMessages: make([]JSON, 0, 10),
		pendingAdminTokenTimestamp: 0,
		launcher: nil,
	}
	for i := 0; i<NUM_WORKERS; i++ {
		h.admins[i] = new_admin(i)
	}
	return h
}

// TODO clean up admin on disconnect

var shared_admins = false;

func (h *Hub) launch_admin() {
	tokenstr, err := sign_token(jwt.MapClaims{
		"username": ADMIN,
	})
	if err != nil {
		log.Printf("!! Failed to sign admin token %v", err)
		return
	}
	if h.launcher == nil {
		log.Printf("!! No launcher found")
		return
	}

	log.Printf("Starting admin for %s", tokenstr)

	go h.launcher.launch(tokenstr)
}

func (h *Hub) send_to_admin(message Message) {
	user := message.from
	if !user.authed {
		return
	}

	now := time.Now().Unix()
	ticket := message.from.ticket

	var target_admin *Admin
	smallest_time := int64(0xffffffff)

	h.mux.RLock()
	// Start any admins that are not online
	for index,admin := range h.admins {
		if admin.client != nil {
			h.mux.RUnlock()
			h.mux.Lock()

			is_mine := admin.specific_ticket == ticket
			if !shared_admins {
				if is_mine || admin.specific_ticket != "" {
					// Admin with no user
					log.Println("Ready admin found for ticket %s", ticket)

					admin.specific_ticket = ticket


					target_admin = admin
					h.mux.Unlock()
					h.mux.RLock()
					break
				}
				h.mux.Unlock()
				h.mux.RLock()
				continue;
			}
			h.mux.Unlock()
			h.mux.RLock()


			et := admin.estimated_end_time

			// Find the user with the least time
			if et < smallest_time && admin.has_space() {
				smallest_time = et
				target_admin = admin
			}
			continue;
		}

		// Start admin up via launcher
		if index >= NUM_WORKERS && target_admin != nil{
			// Don't start extra workers back up unless needed
			continue;
		}

		// Try to relaunch if broken
		if admin.launch_time + 60 < now  {
			h.launch_admin()
		}

		if !shared_admins {
			// Only start one extra admin
			if target_admin != nil {
				break;
			}

			// Reserve this admin and launch it
			log.Println("Launched admin for ticket %s", ticket)
			h.mux.RUnlock()
			h.mux.Lock()

			admin.specific_ticket = ticket

			h.mux.Unlock()
			h.mux.RLock()

			target_admin = admin

			// Continue to allow next admin to start
			continue
		}
	}
	h.mux.RUnlock()

	if !shared_admins && target_admin == nil {
		// Need more admins
		log.Println("No admin slot found, launching new one for ticket %s", ticket)
		admin := new_admin(len(h.admins))

		h.mux.Lock()

		h.admins = append(h.admins,admin)
		admin.specific_ticket = message.from.ticket

		h.mux.Unlock()

		h.launch_admin()

		target_admin = admin
	}

	h.mux.RLock()
	// Print some stats
	num_ready := uint32(0)
	num_free := uint32(0)
	num_free_and_ready := uint32(0)
	for index,admin := range h.admins {
		log.Printf("Admin %u: %p %v",index, admin, admin)
		if admin.specific_ticket == "" {
			num_free += 1
		}
		if admin.client == nil {
			continue
		}
		if admin.specific_ticket == "" {
			num_free_and_ready += 1
		}
		num_ready += 1
	}
	log.Println("[ADMINS] Current have %u(%u) admins, %u(%u) free", len(h.admins), num_ready, num_free, num_free_and_ready)

	from := message.from

	if shared_admins {
		// Attempt to apply sticky admin
		if from.sticky_admin != -1 {
			sticky_admin := h.admins[from.sticky_admin]
			fmt.Println("Checking sticky admin %v", sticky_admin)
			if sticky_admin.has_space() {
				target_admin = sticky_admin
			} else {
				fmt.Println("Sticky admin not ready! falling back to %v",target_admin)
				from.sticky_admin = -1
			}
		}
	}
	h.mux.RUnlock()

	// If no admin could be reserved, send it to any future one
	if target_admin == nil {
		h.mux.Lock()
		log.Printf("[TICKET] [ADMIN MESSAGE] %s (ticket: %s) sent %v to queue",
					from.user, from.ticket, message.msg)
		h.pendingAdminMessages = append(h.pendingAdminMessages, message.msg)
		h.mux.Unlock()
		return
	}

	if !target_admin.ready(now) {
		h.mux.Lock()
		log.Printf("[TICKET] [ADMIN MESSAGE] %s (ticket: %s) sent %v to specific queue %p", from.user, from.ticket, message.msg, target_admin)
		target_admin.pendingMessages = append(target_admin.pendingMessages, message.msg)
		h.mux.Unlock()
		return
	}

	// Send message to ready admin
	et := target_admin.estimated_end_time
	if et < now {
		et = now
	}
	target_admin.estimated_end_time = et + TIME_PER

	from.sticky_admin = int16(target_admin.index)

	log.Printf("[TICKET] [ADMIN MESSAGE] %s (ticket: %s) sent %v to admin %p",
				from.user, from.ticket, message.msg, target_admin.client)
	if !target_admin.client.sendMessage(message.msg) {
		h.unregister_client(target_admin.client);
	}
}

func (h *Hub) handle_message(message Message) {
	log.Printf("msg %s",jsonEncode(message.msg))
	rec_, ok := message.msg["recipients"];
	if !ok {
		message.from.sendMessage(JSON{
			"error": "Missing recipients",
		})
		return
	}
	rec, ok := rec_.([]interface{})
	if !ok {
		log.Printf("Val %v type %T", rec_, rec_)
		message.from.sendMessage(JSON{
			"error": "Missing recipients type",
		})
		return
	}

	var had_ticket = false

	admin_cooldown_left := int64(0)
	//delete(message.msg,"recipients");
	message.msg["recipients"] = false
	for _, r := range rec {
		user, ok := r.(string)
		if !ok {
			continue
		}
		var target *Client
		target = nil


		if user == ADMIN {
			if message.from.user == ADMIN {
				continue
			}

			now := time.Now().Unix()
			ticket := message.from.ticket

			if ticket != "" && len(ticket) > 0 {
				had_ticket = true
			}

			log.Printf("Admin message with ticket '%s' from %v\n", ticket, message.from)

			h.mux.RLock()
			last_time, found := h.admin_cooldowns[ticket];
			h.mux.RUnlock()

			if had_ticket && found {
				if (last_time > now) {
					admin_cooldown_left = last_time - now
					continue
				}
			}

			qt := h.estimated_queue_time
			if qt < now {
				qt = now
			}

			qt_left := qt - now // Based on time left in queue
			qt_left = qt_left / int64(NUM_WORKERS)
			wait_time := math.Min(math.Max(7, 0.8 * float64(qt_left)), 300)
			if !shared_admins {
				wait_time = 10
			}
			h.mux.Lock()
			h.admin_cooldowns[ticket] = now + int64(wait_time)
			h.mux.Unlock()

			h.estimated_queue_time = qt + int64(TIME_PER)


			h.send_to_admin(message)
			continue
		} else if ftarget, found := h.clients[user]; found {
			target = ftarget
		}
		if target == nil {
			log.Printf("Could not find user %s", user);
			continue
		}
		if !target.sendMessage(message.msg) {
			h.unregister_client(target);
		}
	}

	if (had_ticket && admin_cooldown_left > 0) {
		message.from.sendMessage(JSON{
			"type":"ratelimit",
			"message": fmt.Sprintf("You are sending messages too fast, please wait %d more seconds", admin_cooldown_left),
		})
	} else {
		message.from.sendMessage(message.msg)
	}
}

func (h *Hub) unregister_client(client *Client) {
	log.Printf("Unregistering user %s", client.user)
	if _, found := h.clients[client.user]; found {
		delete(h.clients, client.user)
	}
	if client != nil {
		for _,admin := range h.admins {
			if admin.client == client {
				// Reset admin to be reused
				admin.client = nil
				admin.pendingMessages = make([]JSON, 0, 1)
				admin.specific_ticket = ""
			}
		}
	}
	client.conn.Close()
}

func (h *Hub) run() {
	for {
		select {
		case client := <-h.register:
			err := TryFunction(func() {
				log.Printf("Registering user %s", client.user)
				_, found := h.clients[client.user]
				if found {
					client.sendMessage(JSON{
						"error": "User already connected",
					})
					return
				}
				h.clients[client.user] = client
			})
			if err != nil {
				log.Printf("Encountered panic %v", err)
			}
			break
		case client := <-h.unregister:
			err := TryFunction(func() {
				h.unregister_client(client)
			})
			if err != nil {
				log.Printf("Encountered panic %v", err)
			}
			break
		case message := <-h.handle:
			err := TryFunction(func() {
				h.handle_message(message)
			})
			if err != nil {
				log.Printf("Encountered panic %v", err)
			}
			break
		}
	}
}

