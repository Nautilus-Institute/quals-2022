#!/usr/bin/python

from PIL import Image, ImageDraw, ImageFont
import sys
import os

def txt2img(text,bg="#ffffff",fg="#000000",FontSize=20):
	font_dir = "/home/lightning/DescMenu.TTF"
	font_size = FontSize
	fnt = ImageFont.truetype(font_dir, font_size)
	lineWidth = 40
	img = Image.new('RGB', (320, 200), "#000000")
	imgbg = Image.new('RGB', img.size, "#000000") # make an entirely black image
	mask = Image.new('L',img.size,"#000000")       # make a mask that masks out all
	draw = ImageDraw.Draw(img)                     # setup to draw on the main image
	drawmask = ImageDraw.Draw(mask)                # setup to draw on the mask
	drawmask.line((0, lineWidth/2, img.size[0],lineWidth/2),
		  fill="#999999", width=40)        # draw a line on the mask to allow some bg through
	img.paste(imgbg, mask=mask)                    # put the (somewhat) transparent bg on the main

	textlines = text.split("\n")
	(linewidth, lineheight) = draw.textsize(textlines[0], font=fnt)
	linewidth = linewidth / len(textlines[0])
	startpos = (200 - (lineheight*len(textlines))) / 2
	for i in range(0, len(textlines)):
		draw.text(((320 - (linewidth*len(textlines[i])))/2,startpos + (i*lineheight)), textlines[i], font=fnt, fill=bg)      # add some text to the main
	del draw 

	img.save("aif.jpg")
	return img

def getaifdata(img, skipcount):
	img_buf = list(img.getdata())

	c = 0
	d = 0
	data = [""]*200
	for i in img_buf:
		(r,g,b) = i
		if r != 0:
			data[d] += "*"
		else:
			data[d] += " "
		c += 1
		if c == 320:
			d += 1
			c = 0

	outdata = b"APERTURE IMAGE FORMAT (c) 1985\n\n\r%d\n\n\r" % (skipcount)

	i = 199
	startline = 199
	dataline = ""
	while(data[i] != ""):
		dataline += data[i]
		data[i] = ""
		i -= skipcount

		if i < 0:
			startline -= 1
			i = startline

	c = 1
	x = 0
	for i in range(0, len(dataline) - 1):
		if (x == 0) and (dataline[i] == dataline[i+1]):
			c += 1
		else:
			outdata += bytes([c + 32])
			c = 1
			x = 0

		if(c == (126-32)):
			outdata += bytes([c + 32])
			c = 0
			x = 1

	if (x == 0) and (dataline[len(dataline) - 2] == dataline[len(dataline) - 1]):
		c += 1
	else:
		outdata += bytes([c + 32])
		c = 0
		
	outdata += bytes([c + 32])
	return outdata

if __name__ == "__main__":

	answer = "The Flag Is:\nDescent ][\nRules"

	#get the image and convert to aif format
	aifdata = getaifdata(txt2img(answer, FontSize=40), 42)
	open("answer.aif","wb").write(aifdata)