#!/usr/bin/env python
import sys, os
import random
import struct
import zlib

#up down left right in out
DirectionMath = [[0,-1, 0], [0, 1, 0], [-1, 0, 0], [1, 0, 0], [0, 0, -1], [0, 0, 1], [-2,-2,-2]]

class GenMapSpot:
	def __init__(self, x, y, z, PrevDir, AllowTeleport=False):
		self.x = x
		self.y = y
		self.z = z

		self.Directions = list(range(0, 7))
		if PrevDir >= 4:
			self.Directions.remove(4)
			self.Directions.remove(5)

		if (not AllowTeleport) or (PrevDir >= 4):
			self.Directions.remove(6)

		random.shuffle(self.Directions)

	def GetDirection(self):
		#get the next entry
		NewDirection = -1
		while(len(self.Directions)):
			NewDirection = self.Directions.pop(0)
			if NewDirection >= 4 and (NewDirection <= 5):
				#don't always allow moving between levels
				if random.randint(0, 4) != 0:
					NewDirection = -1
					continue

				#allowing up, down, or teleport - remove the other entry
				if NewDirection == 4:
					if 5 in self.Directions:
						self.Directions.remove(5)
					if 6 in self.Directions:
						self.Directions.remove(6)
				elif NewDirection == 5:
					if 4 in self.Directions:
						self.Directions.remove(4)
					if 6 in self.Directions:
						self.Directions.remove(6)
			elif NewDirection == 6:
				#don't always allow moving between levels
				if random.randint(0, 6) != 0:
					NewDirection = -1
					continue

				if 4 in self.Directions:
					self.Directions.remove(4)
				if 5 in self.Directions:
					self.Directions.remove(5)
			break

		return NewDirection

def HandleSpot(map, MapSpots, size, depth, allow_teleport, teleport_locations):
	while len(MapSpots):
		#get end entry
		CurSpot = MapSpots[-1]

		#get new direction, if -1 then remove it from the list
		NewDirection = CurSpot.GetDirection()
		if NewDirection == -1:
			MapSpots.pop()
			continue

		#if teleport then pick a random location and if it is an X then allow it
		if NewDirection == 6:
			#attempt to locate an X 4 times, if it fails then stop trying
			for i in range(0, 4):
				NewX1 = random.randint(1, size-2) | 1
				NewX2 = NewX1
				NewY1 = random.randint(1, size-2) | 1
				NewY2 = NewY1
				NewZ = CurSpot.z + random.randint(-4, 4) #teleport +/- 3 levels from where we are
				if (NewZ < 0):
					NewZ = 0
				elif NewZ > depth-2:
					NewZ = depth-2
				if map[NewZ][NewX1][NewY1] == "X":
					break
		else:
			#got a direction, work on it
			NewX1 = CurSpot.x + DirectionMath[NewDirection][0]
			NewY1 = CurSpot.y + DirectionMath[NewDirection][1]
			NewZ = CurSpot.z + DirectionMath[NewDirection][2]
			NewX2 = NewX1 + DirectionMath[NewDirection][0]
			NewY2 = NewY1 + DirectionMath[NewDirection][1]

		if (NewX2 <= 0) or (NewX2 >= size-1) or (NewY2 <= 0) or (NewY2 >= size-1) or (NewZ < 0) or (NewZ >= depth):
			continue

		if (map[NewZ][NewY1][NewX1] == 'X') and (map[NewZ][NewY2][NewX2] == 'X'):
			if DirectionMath[NewDirection][2] == -1:
				map[NewZ][NewY1][NewX1] = '+'
				map[CurSpot.z][CurSpot.y][CurSpot.x] = '-'
			elif DirectionMath[NewDirection][2] == 1:
				map[NewZ][NewY1][NewX1] = '-'
				map[CurSpot.z][CurSpot.y][CurSpot.x] = '+'
			elif NewDirection == 6:
				map[CurSpot.z][CurSpot.y][CurSpot.x] = 'T'
				map[NewZ][NewY1][NewX1] = "T"
				teleport_locations[str([NewZ, NewY1, NewX1])] = [CurSpot.z, CurSpot.y, CurSpot.x]
				teleport_locations[str([CurSpot.z, CurSpot.y, CurSpot.x])] = [NewZ, NewY1, NewX1]
			else:
				map[NewZ][NewY1][NewX1] = ' '
				map[NewZ][NewY2][NewX2] = ' '

			MapSpots.append(GenMapSpot(NewX2, NewY2, NewZ, NewDirection, allow_teleport))

def GenerateSolution(size, depth, AllowTeleport):
	size = size | 1

	#generate the initial map
	mapstr = "#"*size + "\n"
	mapstr += ("#" + "X"*(size-2) + "#\n")*(size-2)
	mapstr += "#"*size + "\n"
	maplines = mapstr.split("\n")

	map = []
	for z in range(0, depth):
		maplevel = []
		for i in range(0, size):
			maplevel.append(list(maplines[i]))
		map.append(maplevel)

	#pick a random starting location along the edge
	StartPos = random.randint(1, size-2) | 1
	StartZ = 0
	StartingSide = random.randint(0, 3)
	if StartingSide == 0:
		#top
		map[StartZ][0][StartPos] = "S"
		StartPos = [StartZ, StartPos, 1]
	elif StartingSide == 1:
		#bottom
		map[StartZ][-1][StartPos] = "S"
		StartPos = [StartZ, StartPos, size-2]
	elif StartingSide == 2:
		#left
		map[StartZ][StartPos][0] = "S"
		StartPos = [StartZ, 1, StartPos]
	elif StartingSide == 3:
		#right
		map[StartZ][StartPos][-1] = "S"
		StartPos = [StartZ, size-2, StartPos]

	#generate a path through the map
	MapSpots = [GenMapSpot(StartPos[1], StartPos[2], StartPos[0], -1, AllowTeleport)]
	Teleports = {}
	HandleSpot(map, MapSpots, size, depth, AllowTeleport, Teleports)

	#get a side opposite of where we start
	EndingSide = StartingSide ^ 1

	EndZ = depth-1

	#get a position that is acceptable as we don't want them too close to each other
	while(1):
		EndPos = random.randint(1, size-2)

		if EndingSide in [0, 1]:
			#top/bottom
			#see if start was on the left or right and if so if our selected position is past the half way mark
			if (StartingSide == 2) and (EndPos < ((size / 3) * 2)):
				continue
			elif (StartingSide == 3) and (EndPos > ((size / 3) * 2)):
				continue
		else:
			#left/right, same as above
			if (StartingSide == 0) and (EndPos < ((size / 3) * 2)):
				continue
			elif (StartingSide == 1) and (EndPos > ((size / 3) * 2)):
				continue

		#must be good
		break

	if EndingSide == 0:
		#top
		while map[EndZ][1][EndPos] != " ":
			EndPos = (EndPos + 1) % size
			if (EndPos == 0) or (EndPos == size - 1):
				EndPos = 1
		map[EndZ][0][EndPos] = "E"
		EndX = 0
		EndY = EndPos
	elif EndingSide == 1:
		#bottom
		while map[EndZ][-2][EndPos] != " ":
			EndPos = (EndPos + 1) % size
			if (EndPos == 0) or (EndPos == size - 1):
				EndPos = 1
		map[EndZ][-1][EndPos] = "E"
		EndX = len(map[EndZ]) - 1
		EndY = EndPos
	elif EndingSide == 2:
		#left
		while map[EndZ][EndPos][1] != " ":
			EndPos = (EndPos + 1) % size
			if (EndPos == 0) or (EndPos == size - 1):
				EndPos = 1

		map[EndZ][EndPos][0] = "E"
		EndX = EndPos
		EndY = 0
	elif EndingSide == 3:
		#right
		while map[EndZ][EndPos][-2] != " ":
			EndPos = (EndPos + 1) % size
			if (EndPos == 0) or (EndPos == size - 1):
				EndPos = 1

		map[EndZ][EndPos][-1] = "E"
		EndX = EndPos
		EndY = len(map[EndZ]) - 1

	return map, StartPos, [EndZ, EndX, EndY], Teleports

def SaveFullMap(map, counter):
	size = len(map[0])
	depth = len(map)
	Padding = " "*4
	LineData = ""
	f = open(f"map{counter}.txt", "w")
	for z in range(0, depth):
		LineData += "Level %d"%(z) + " "*(size-7) + Padding
	f.write(LineData + "\n")

	for i in range(0, len(map[0])):	
		LineData = ""
		for z in range(0, depth):
			LineData += "".join(map[z][i]) + Padding
		f.write(LineData + "\n")
	f.close()

def DisplayCurrentPos(map, CurPos):
	#generate an area to display to the player and return valid directions to move
	size = len(map[0])

	z, y, x = CurPos

	#first draw the border, majority of the time it will just be # but we need to make sure
	#Start and End are also seen
	DisplayData = []
	DisplayData.append(map[z][0])
	for i in range(1, len(map[z]) -1):
		DisplayData.append([map[z][i][0]] + [" "]*(size-2) + [map[z][i][-1]])
	DisplayData.append(map[z][-1])

	#we allow up to 3 blocks away to be seen in a spotlight like manner but you can't see through walls
	BlockPathX = [0,0]
	BlockPathY = [0,0]
	for i in range(0, 4):
		#check coordinates for any walls and draw what is seen

		#check north
		if (y-i >= 0) and not BlockPathY[0]:
			if map[z][y-i][x] == "X":
				BlockPathY[0] = 1
			DisplayData[y-i][x] = map[z][y-i][x]
			if(x-1) >= 0:
				DisplayData[y-i][x-1] = map[z][y-i][x-1]
			if(x+1) < size:
				DisplayData[y-i][x+1] = map[z][y-i][x+1]

		#check south
		if (y+i < size) and not BlockPathY[1]:
			if map[z][y+i][x] == "X":
				BlockPathY[1] = 1
			DisplayData[y+i][x] = map[z][y+i][x]
			if(x-1) >= 0:
				DisplayData[y+i][x-1] = map[z][y+i][x-1]
			if(x+1) < size:
				DisplayData[y+i][x+1] = map[z][y+i][x+1]

		#check west
		if (x-i >= 0) and not BlockPathX[0]:
			if map[z][y][x-i] == "X":
				BlockPathX[0] = 1
			DisplayData[y][x-i] = map[z][y][x-i]
			if(y-1) >= 0:
				DisplayData[y-1][x-i] = map[z][y-1][x-i]
			if(y+1) < size:
				DisplayData[y+1][x-i] = map[z][y+1][x-i]

		#check east
		if (x+i < size) and not BlockPathX[1]:
			if map[z][y][x+i] == "X":
				BlockPathX[1] = 1
			DisplayData[y][x+i] = map[z][y][x+i]
			if(y-1) >= 0:
				DisplayData[y-1][x+i] = map[z][y-1][x+i]
			if(y+1) < size:
				DisplayData[y+1][x+i] = map[z][y+1][x+i]

	#now fill in the player
	DisplayData[y][x] = '@'

	LineData = ""
	for i in range(0, len(DisplayData)):
		LineData += "".join(DisplayData[i]) + "\n"

	Directions = ""
	if ((y-1) >= 0) and (map[z][y-1][x] not in "X#S"):
		Directions += "N"
	if ((y+1) < size) and (map[z][y+1][x] not in "X#S"):
		Directions += "S"
	if ((x-1) >= 0) and (map[z][y][x-1] not in "X#S"):
		Directions += "W"
	if ((x+1) < size) and (map[z][y][x+1] not in "X#S"):
		Directions += "E"
	if map[z][y][x] == "-":
		Directions += "D"
	if map[z][y][x] == "+":
		Directions += "U"
	if map[z][y][x] == "T":
		Directions += "T"

	return LineData, Directions

def SendZLib(data):
	outdata = zlib.compress(data.encode("utf-8"))
	os.write(1, struct.pack("<I", len(outdata)))
	os.write(1, outdata)

if __name__ == "__main__":
	try:
		open("flag","r").read()
	except:
		print("Failed to access flag file, please report to admin")
		sys.exit(0)

	print("Welcome to Twisty, you will be sent 4 bytes indicating the length of incoming data followed by zlib compressed data.")
	print("The expected response is one or more directions to move as a normal text string with newline.")
	print("Good luck")
	sys.stdout.flush()

	mapsizes = [[11, 1, 0],
				[21, 1, 0],
				[21, 2, 0],
				[41, 7, 0],
				[61, 10, 1],
				]
	counter = 0
	for entry in mapsizes:
		map, Start, End, Teleports = GenerateSolution(entry[0], entry[1], entry[2])
		counter += 1
		#SaveFullMap(map, counter)
		#DisplayFullMap(map)
		CurPos = Start

		MoveCount = 0
		while(1):
			LineData, Directions = DisplayCurrentPos(map, CurPos)
			LineData += "Directions: " + Directions + " Q(uit)"
			SendZLib(LineData)

			NewDirection = input()
			if (len(NewDirection) == 0):
				sys.exit(0)

			NewDirections = list(NewDirection.upper())
			NewPos = list(CurPos)
			FoundEnd = False
			for NewDirection in NewDirections:
				MoveCount += 1
				if NewDirection == "Q":
					sys.exit(0)
				elif NewDirection == "N":
					NewPos[1] -= 1
				elif NewDirection == "S":
					NewPos[1] += 1
				elif NewDirection == "W":
					NewPos[2] -= 1
				elif NewDirection == "E":
					NewPos[2] += 1
				elif NewDirection == "U":
					NewPos[0] += 1
				elif NewDirection == "D":
					NewPos[0] -= 1
				elif NewDirection == "T":
					if str(NewPos) not in Teleports:
						SendZLib("Invalid Direction")
						break
					else:
						NewPos = Teleports[str(NewPos)]
						SendZLib(f"Teleported to {NewPos[2]}/{NewPos[1]}/{NewPos[0]}")
				else:
					SendZLib("Invalid Direction")
					break

				if (NewPos[0] < 0) or (NewPos[1] < 0) or (NewPos[2] < 0) or \
					(NewPos[0] >= entry[1]) or (NewPos[1] >= entry[0]) or (NewPos[2] >= entry[0]) or \
					(map[NewPos[0]][NewPos[1]][NewPos[2]] in "X#S"):
					SendZLib("Invalid Direction")
					break

				CurPos = NewPos
				if map[CurPos[0]][CurPos[1]][CurPos[2]] == "E":
					SendZLib("Congratulations!")
					FoundEnd = True
					break

			if FoundEnd:
				break
		#os.write(2, f"Moves: {MoveCount}\n".encode("utf-8"))
	#they have to get through all maps to get here, quitting calls sys.exit
	FlagData = open("flag", "r").read()
	SendZLib(FlagData)
