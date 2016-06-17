.SILENT:

default:
	gcc -g -pthread lab1a.c -o lab1a

clean:
	shopt -s extglob; \
	`rm -rf !(Makefile|README|lab1a.c)`

dist:
	tar -zcvf lab1a-704465898.tar.gz lab1a.c Makefile README
