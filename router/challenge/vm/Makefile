
all: vm vm_debug

vm: vm.c mangle.py
	gcc -fcf-protection=none -O2 vm.c -S -masm=intel -fno-pic -no-pie -o vm.s
	python3 mangle.py
	gcc vm.s -no-pie -fcf-protection=none -o vm
	strip --strip-all vm

vm_debug: vm.c mangle.py
	gcc -fcf-protection=none -DDEBUG=1 -O0 -g vm.c -fno-pic -no-pie -o vm_debug

clean:
	rm -f vm vm.s vm_debug
