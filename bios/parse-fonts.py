import sys, os
from PIL import Image
import compress_lzma
import struct

FontImage = Image.open("font.png")
FontImage = FontImage.convert(mode="RGB")

#calculate rough height of each section
RoughLetterHeight = FontImage.height / 4

BoxColor = (204, 163, 40)

Chars = dict()
CharCount = 0x20

#go through each section
for Line in range(0, 4):
	CurHeight = int((RoughLetterHeight * Line) + (RoughLetterHeight / 2))

	#find the starting left side
	LeftStart = 0
	for x in range(0, FontImage.width):
		if FontImage.getpixel((x, CurHeight)) != (0,0,0):
			#print("Left starts at", x, FontImage.getpixel((x, CurHeight)))
			LeftStart = x
			break

	#find the top and bottom
	for y in range(CurHeight, 0, -1):
		if FontImage.getpixel((x + 5, y)) == BoxColor:
			#print("Top starts at", y+1, FontImage.getpixel((x+5, y)))
			TopStart = y + 1
			break
	for y in range(CurHeight, FontImage.height):
		if FontImage.getpixel((x + 5, y)) == BoxColor:
			#print("Height:", y - TopStart - 1, FontImage.getpixel((x+5, y-1)))
			LetterHeight = y - TopStart - 1
			break
		

	#find each area
	LastMatch = LeftStart
	OnBox = True
	for x in range(LeftStart, FontImage.width):
		if OnBox and FontImage.getpixel((x, CurHeight)) != BoxColor:
			LastMatch = x
			OnBox = False
		elif not OnBox and FontImage.getpixel((x, CurHeight)) == BoxColor:
			if x - LastMatch > 10:
				print(f"Found {chr(CharCount)} at {x}/{TopStart}")
				Chars[CharCount] = {"Left": LastMatch, "Top": TopStart, "Height": LetterHeight, "Width": x - LastMatch-1}
				CharCount += 1
			LastMatch = x
			OnBox = True

#create a new image
NewWidth = 0
for i in Chars:
	NewWidth += Chars[i]["Width"]
FontHeight = int(Chars[0x20]["Height"] / 2)

NewFont = Image.new("RGB", (NewWidth, FontHeight), color=(0,0,0))
CurX = 0
MaxWidth = 0
MinWidth = 255

for CurChar in range(0x20, 0x7f):
	CharImg = FontImage.crop((Chars[CurChar]["Left"], Chars[CurChar]["Top"], Chars[CurChar]["Left"] + Chars[CurChar]["Width"], Chars[CurChar]["Top"] + Chars[CurChar]["Height"]))
	CharImg = CharImg.resize((int(Chars[CurChar]["Width"] / 2), FontHeight), Image.BICUBIC)

	NewFont.paste(CharImg, (CurX, 0))
	Chars[CurChar]["Offset"] = CurX
	Chars[CurChar]["FinalWidth"] = CharImg.width
	if CharImg.width > MaxWidth:
		MaxWidth = CharImg.width
	if CharImg.width < MinWidth:
		MinWidth = CharImg.width

	CurX += CharImg.width

NewFont = NewFont.crop((0, 0, CurX, FontHeight))
print("Average width:", NewFont.width / len(Chars))
print("Chars across screen:", 1024 / (NewFont.width / len(Chars)))

#now make sure all pixels are colored
HSplit = int(NewFont.height / 2)
for x in range(0, NewFont.width):
	for y in range(0, HSplit):
		CurPixel = NewFont.getpixel((x,y))
		if CurPixel != (0,0,0):
			if CurPixel[0] < 0x70:
				NewFont.putpixel((x,y), (0,0,0))
			else:
				#NewFont.putpixel((x,y), (255,255,255))
				#NewFont.putpixel((x,y), 0x4f3e37)
				NewFont.putpixel((x,y), 0x8eeafe + (0x090100 * (HSplit-y)))


	for y in range(HSplit, NewFont.height):
		CurPixel = NewFont.getpixel((x,y))
		if CurPixel != (0,0,0):
			if CurPixel[0] < 0x70:
				NewFont.putpixel((x,y), (0,0,0))
			else:
				NewFont.putpixel((x,y), 0x6f4e47 + (0x070909*(y-HSplit)))

#flip the bitmap around to make printing easier
#NewFont = NewFont.transpose(Image.FLIP_LEFT_RIGHT)
NewFont = NewFont.transpose(Image.FLIP_TOP_BOTTOM)

#cut off the bottom few pixels
NewFont = NewFont.crop((0, 0, NewFont.width, NewFont.height))
NewFont.save("font-out.bmp")

CharOffsets = []
for x in range(0x20, 0x7f):
	CharOffsets.append(Chars[x]["Offset"])
CharOffsets.append(NewFont.width)

CData = b""
for x in range(0, len(CharOffsets)):
	CData += struct.pack("<H", CharOffsets[x])
CData += struct.pack("<H", NewFont.width)
f = open("font-out.dat","wb")
f.write(CData)
f.close()

lzdata = compress_lzma.Convert("font-out.bmp")
print(f"Wrote {len(lzdata)} bytes for font data")