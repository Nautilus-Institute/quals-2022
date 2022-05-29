import sys, os
import socket
import select
import time
import struct
import zlib
import traceback

AllPositions = []

class XYZ:
	def __init__(self, x, y, z):
		self.x = x
		self.y = y
		self.z = z

class GenMapSpot:
	def __init__(self, map, directions, prev_move, PrevSpot):
		global AllPositions
		self.x = PrevSpot.x
		self.y = PrevSpot.y
		self.z = PrevSpot.z

		#add all directions we can move
		self.Directions = list(directions)
		self.PrevMove = prev_move
		self.Backwards = ""

		if self.PrevMove == "N":
			self.Directions.remove("S")
			self.Backwards = "S"
			self.y -= 1
		elif self.PrevMove == "S":
			self.Directions.remove("N")
			self.Backwards = "N"
			self.y += 1
		elif self.PrevMove == "W":
			self.Directions.remove("E")
			self.Backwards = "E"
			self.x -= 1
		elif self.PrevMove == "E":
			self.Directions.remove("W")
			self.Backwards = "W"
			self.x += 1
		elif self.PrevMove == "U":
			self.Directions.remove("D")
			self.Backwards = "D"
			self.z += 1
		elif self.PrevMove == "D":
			self.Directions.remove("U")
			self.Backwards = "U"
			self.z -= 1
		elif self.PrevMove == "T":
			self.Directions.remove("T")
			self.Backwards = "T"

		if [self.x, self.y, self.z] in AllPositions:
			self.Directions = []
		else:
			AllPositions.append([self.x, self.y, self.z])
			#if this map has E then see if we can just move straight to it
			if "E" in map:
				if "@E" in map:
					self.Directions = ["E"]
				elif "E@" in map:
					self.Directions = ["W"]
				else:
					maplines = map.split("\n")
					if "E" in maplines[0]:
						offset = maplines[0].find("E")
						if maplines[1][offset] == "@":
							self.Directions = ["N"]
					elif "E" in maplines[-1]:
						offset = maplines[-1].find("E")
						if maplines[-2][offset] == "@":
							self.Directions = ["S"]

	def GetDirection(self):
		if len(self.Directions) == 0:
			return -1
		return self.Directions.pop(0)

def GetMap(s):
	teleport = []
	while(1):
		maplen = struct.unpack("<I",s.recv(4))[0]
		mapdata = b""
		while len(mapdata) < maplen:
			mapdata += s.recv(maplen - len(mapdata))

		mapdata = zlib.decompress(mapdata).decode("utf-8")
		if mapdata == "Congratulations!":
			print("Found Congratulations!")
			return True, "Congratulations!", []
		elif mapdata.lower().find("flag") != -1:
			return True, mapdata, []
		elif mapdata.lower().startswith("teleported to"):
			#got teleport locations, snag them, we might be walking backwards so
			#only care about latest entry
			locdata = mapdata.split(" ")[-1].split("/")
			teleport = XYZ(int(locdata[0]), int(locdata[1]), int(locdata[2]))
		else:
			break

	mapdata = mapdata.split("\n")
	new_directions = mapdata.pop().split(" ")[1]
	new_map = "\n".join(mapdata)
	return new_map, new_directions, teleport

def HandleMap(s, map, MapSpots):
	NextMove = ""
	Backwards = ""
	while len(MapSpots):
		#get end entry
		CurSpot = MapSpots[-1]

		#get new direction, if -1 then remove it from the list
		NextMove = CurSpot.GetDirection()
		if NextMove == -1:
			Backwards += CurSpot.Backwards
			MapSpots.pop()
			continue

		s.send((Backwards + NextMove + "\n").encode("utf-8"))
		if len(Backwards):
			Backwards = ""
		new_map, new_directions, new_teleport = GetMap(s)
		if new_map == True:
			return new_directions

		if NextMove == "T":
			CurSpot = new_teleport

		MapSpots.append(GenMapSpot(new_map, new_directions, NextMove, CurSpot))

def GetXYZ(map):
	#find @ in the map
	mapdata = map.split("\n")
	NewXYZ = XYZ(0, 0, 0)
	for y in range(1, len(mapdata)):
		if "@" in mapdata[y]:
			NewXYZ.y = y
			NewXYZ.x = mapdata[y].find("@")
			break
	return NewXYZ

print("Warning, this may take awhile as we brute force the maze with no smarts")
#s = socket.create_connection(("127.0.0.1", 11000))
s = socket.create_connection(("twisty-2mjh4xgp7zubo.shellweplayaga.me", 10000))
s.recv(4096)
s.send("ticket{TradewindWindlass7075n22:yP50SsnLu2rTLgJycqxwnFsmk6xSxaSvB42DuroeU8rGlumv}\n".encode("utf-8"))

#get past the intro
data = b""
while(data.find(b"Good luck\n") == -1):
	data += s.recv(1)

counter = 0
while(1):
	AllPositions = []
	map, directions, teleports = GetMap(s)
	if map == True:
		print(directions)
		break

	MapSpots = [GenMapSpot(map, directions, "", GetXYZ(map))]
	ret = HandleMap(s, map, MapSpots)
	counter += 1
	print(f"Finished map #{counter}")
