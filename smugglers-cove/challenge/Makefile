.PHONY: all
all: cove dig_up_the_loot
cove: cove.c
	gcc -o cove cove.c -I/LuaJIT/src/ -fPIE -pie /usr/local/lib/libluajit-5.1.so
	strip cove
dig_up_the_loot: dig_up_the_loot.c
	gcc -o dig_up_the_loot dig_up_the_loot.c --static
	strip dig_up_the_loot
