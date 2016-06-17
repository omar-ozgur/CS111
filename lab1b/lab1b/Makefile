.SILENT:

default: client server

client:
	gcc -g -lmcrypt -pthread client.c -o client

server:
	gcc -g -lmcrypt -pthread server.c -o server

clean:
	rm -f client server

dist:
	tar -zcvf lab1b-704465898.tar.gz client.c server.c my.key Makefile README
