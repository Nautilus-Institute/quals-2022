package main

import (
	//"bytes"
	"log"
	"net/http"
	"time"
	"encoding/json"

	"github.com/gorilla/websocket"
	"github.com/golang-jwt/jwt"
)

type JSON map[string]interface{}

const (
	// Time allowed to write a message to the peer.
	writeWait = 10 * time.Second

	// Time allowed to read the next pong message from the peer.
	pongWait = 60 * time.Second

	// Send pings to peer with this period. Must be less than pongWait.
	pingPeriod = (pongWait * 9) / 10

	// Maximum message size allowed from peer.
	maxMessageSize = 512
)

var upgrader = websocket.Upgrader{
	ReadBufferSize:  1024,
	WriteBufferSize: 1024,
}

type Client struct {
	conn *websocket.Conn
	hub *Hub
	send chan []byte
	user string
	authed bool
	isAdmin bool
	ticket string
	sticky_admin int16
	claims jwt.MapClaims
	remote_ip string
}

func jsonDecode(data []byte) (JSON,error) {
	out := make(map[string]interface{})
	err := json.Unmarshal(data, &out);
	return out, err
}

func jsonEncode(data JSON) ([]byte) {
	s,_ := json.Marshal(data)
	return s
}

func (c *Client) readPump() {
	defer func() {
		if c.authed {
			c.hub.unregister <- c
		}
		log.Println("Client exiting")
		c.conn.Close()
	}()
	c.conn.SetReadLimit(maxMessageSize)
	c.conn.SetReadDeadline(time.Now().Add(pongWait))
	c.conn.SetPongHandler(func(string) error {
		c.conn.SetReadDeadline(time.Now().Add(pongWait))
		return nil
	})
	for {
		_, message, err := c.conn.ReadMessage()
		if err != nil {
			if websocket.IsUnexpectedCloseError(err, websocket.CloseGoingAway, websocket.CloseAbnormalClosure) {
				log.Printf("error: %v", err);
			}
			break
		}
		log.Printf("message %v",message)
		blob, err := jsonDecode(message)
		log.Printf("message %v %v",blob, err)
		c.handleMessage(blob)
	}
}

func (c *Client) writePump() {
	ticker := time.NewTicker(pingPeriod)
	defer func() {
		ticker.Stop()
		c.conn.Close()
	}()
	for {
		select {
		case msg, ok := <-c.send:
			c.conn.SetWriteDeadline(time.Now().Add(writeWait))
			if !ok {
				c.conn.WriteMessage(websocket.CloseMessage, []byte{})
				return
			}

			w, err := c.conn.NextWriter(websocket.TextMessage)
			if err != nil {
				return
			}
			w.Write(msg)
			if err := w.Close(); err != nil {
				return
			}
		case <-ticker.C:
			c.conn.SetWriteDeadline(time.Now().Add(writeWait))
			if err := c.conn.WriteMessage(websocket.PingMessage, nil); err != nil {
				return
			}
		}
	}
}

func getString(msg JSON, key string) (string, bool) {
	v, ok := msg[key]; if ok {
		s, ok := v.(string); if ok {
			return s, true
		}
	}
	return "", false
}

func (c *Client) handleMessage(msg JSON) {
	log.Printf("Got ws msg %v", msg)
	mtype, ok := getString(msg,"type")
	if !ok {
		c.sendMessage(JSON{
			"error":"Missing `type`",
		})
		return
	}
	if mtype == "token" {
		stoken, ok := getString(msg, "token"); if !ok {
			c.sendMessage(JSON{
				"error":"Missing `token`",
			})
			return
		}
		claims, err := verify_token(stoken)
		if err != nil || claims == nil {
			log.Printf("JWT error %v", err)
			c.sendMessage(JSON{
				"error":"Token error",
			})
			return
		}
		log.Printf("User %v", claims["username"])
		username := claims["username"].(string)
		//isAdmin := claims["admin"].(bool)
		isAdmin := username == ADMIN

		if isAdmin {
			log.Println("ADMIN IP CHECK", c.remote_ip, MY_IP)
			/*
			if c.remote_ip != MY_IP {
				log.Printf("Denied client ip %s, which tried to connect as admin", c.remote_ip)
				c.sendMessage(JSON{
					"error": "User already connected",
				})
				return
			}
			*/
		}

		var ticket string
		if !isAdmin {
			log.Printf("Checking ticket!")
			new_ticket, ok := getString(msg, "ticket"); if !ok {
				c.sendMessage(JSON{
					"error":"Missing `ticket`",
				})
				return
			}
			log.Printf("Got ticket '%s' ok %v", ticket, ok)

			slug, err := ticketVerify(new_ticket);
			if err != nil {
				c.sendMessage(JSON{
					"error":"Invalid ticket",
				})
				return
			}
			log.Printf("Got '%v' %v from verifyticket", ticket, ok)
			ticket = slug
		}


		c.authed = true
		c.claims = claims
		c.user = username
		c.isAdmin = isAdmin
		c.ticket = ticket

		hub := c.hub
		hub.register <- c
		now := time.Now().Unix()

		log.Printf("User %s is admin= %s", c.user, c.isAdmin)
		if c.isAdmin {
			hub.mux.Lock()
			defer hub.mux.Unlock()

			var target_admin *Admin
			for index,admin := range hub.admins {
				log.Printf("Checking Admin %u: %p %v",index, admin, admin)
				if admin.client != nil {
					continue;
				}
				target_admin = admin
				break
			}
			log.Printf("I am target_admin %p %v", target_admin, target_admin)
			if (target_admin == nil) {
				c.sendMessage(JSON{
					"error": "User already connected",
				})
				return
			}

			target_admin.client = c
			target_admin.launch_time = now

			c.sendMessage(JSON{
				"type":"auth",
				"result": true,
			})


			// If we have no specific messages, take some from hub
			if len(target_admin.pendingMessages) == 0 {
				if len(hub.pendingAdminMessages) > 0 {
					log.Printf("Reading pending message from hub")
				}

				for i,m := range hub.pendingAdminMessages {
					et := target_admin.estimated_end_time
					spent := et - target_admin.launch_time

					if spent + TIME_PER >= LIFETIME {
						// Don't send more than expected
						hub.pendingAdminMessages = hub.pendingAdminMessages[i:]
						return
					}

					if et < now {
						et = now
					}
					target_admin.estimated_end_time = et + TIME_PER

					c.sendMessage(m)
				}
				hub.pendingAdminMessages = make([]JSON, 0, 1)

			// Send all specific messages
			} else {
				log.Printf("Reading pending message for specific ticket %s", target_admin.specific_ticket)
				for _, m := range target_admin.pendingMessages {
					et := target_admin.estimated_end_time
					target_admin.estimated_end_time = et + TIME_PER
					c.sendMessage(m)
				}
				target_admin.pendingMessages = make([]JSON, 0, 1)
			}

			return
		}

		c.sendMessage(JSON{
			"type":"auth",
			"result": true,
		})
		return
	}

	if !c.authed {
		c.sendMessage(JSON{
			"error":"Unauthorized",
		})
		return
	}

	if mtype == "widget" {
		c.hub.handle <- Message{
			from: c,
			msg: msg,
		}
	}
}

func (c *Client) sendMessage(msg JSON) bool {
	select {
		case c.send <- jsonEncode(msg):
			return true
		default:
			return false
	}
}

func serveWs(hub *Hub, w http.ResponseWriter, r *http.Request) {
	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Println(err)
		return
	}
	client := &Client{
		hub: hub,
		conn: conn,
		send: make(chan []byte, 256),
		sticky_admin: -1,
		remote_ip: r.Header.Get("X-Real-IP"),
	}
	log.Printf("Client connecting %p from %s", client, client.remote_ip)

	go func(){
		err := TryFunction(func() {
			client.readPump()
		})

		if err != nil {
			log.Printf("Encountered panic %v", err)
		}
	}()
	go func() {
		err := TryFunction(func() {
			client.writePump()
		})

		if err != nil {
			log.Printf("Encountered panic %v", err)
		}
	}()
}
