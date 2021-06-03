all: release

release:clean
	gcc terminal.c -o terminal.bin
	
debug:clean
	gcc terminal.c -o terminal.bin -ggdb
	
run:release
	./terminal.bin
gdb:debug
	gdb ./terminal.bin
	
clean:
	rm terminal.bin || echo -n ''
