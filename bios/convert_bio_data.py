import compress_lzma
from PIL import Image, ImageFont, ImageDraw

def ConvertBIOData():
	names = ["Endeavor", "Fish", "Fuzyll", "Hoju", "HJ", "Itszn", "Jetboy", "Lightning", "Mike_Pizza", "Perribus", "Thing2", "ThomasWindmill", "Vito"]
	bio_len = 0

	for i in range(0, len(names)):
		name = names[i]
		BioImage = Image.open(f"bio data/bio_{name}.bmp")
		BioImage = BioImage.transpose(Image.FLIP_TOP_BOTTOM)

		BioImage.save(f"bio data/bio_{name}-out.bmp")

		compress_lzma.Convert(f"bio data/bio_{name}-out.bmp")

		BioText = open(f"bio data/bio_{name}.txt", "r").read()
		BioText = BioText.replace("\r\n", "\n")
		open(f"bio data/bio_{name}-out.txt", "wb").write(BioText.encode("utf-8"))
		names[i] = name.upper()

	names.sort()
	name_list = "\x00".join(names) + "\x00\x00"
	open("bio data/names.txt", "wb").write(name_list.encode("utf-8"))
	print(f"Wrote bio data")

	return names

if __name__ == "__main__":
	ConvertBIOData()
