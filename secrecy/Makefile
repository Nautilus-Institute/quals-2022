CC = gcc

CFLAGS =-fPIE -pie -O2 -I. -m64 -fstack-protector-all -D_FORTIFY_SOURCE=2 -Werror --param ssp-buffer-size=4 -fcf-protection=full -fstack-clash-protection
LDFLAGS=-Wl,-z,now -Wl,-z,relro -Wl,-z,noexecstack -Wl,-Map,secrecy.map -Wl,-T,secrecy.ld

OBJS = $(patsubst %.c,%.o,$(wildcard *.c))

all: secrecy

%.o: %.c
	@echo gcc $^ -o $@
	@$(CC) -c $^ $(CFLAGS) -o $@
	@python3 alter_funcs.py $@

secrecy: $(OBJS)
	@echo gcc -o secrecy $(OBJS)
	@$(CC) $(CFLAGS) $(LDFLAGS) -o secrecy $(OBJS)
	@python3 encrypt_binary.py
	@objcopy -R ".comment" secrecy
	@objcopy -R ".note.*" secrecy
	@strip -s secrecy

clean:
	rm -f *.o secrecy
