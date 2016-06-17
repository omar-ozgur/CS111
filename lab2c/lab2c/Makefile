.SILENT:

default:
	gcc -pthread -lrt SortedList.c lab2c.c -o lab2c

pg:
	gcc -pg -pthread -lrt SortedList.c lab2c.c -o lab2c

clean:
	rm -f lab2c gmon.out

dist:
	tar -zcvf lab2c-704465898.tar.gz lab2c.c SortedList.h SortedList.c Makefile ratios.png README
