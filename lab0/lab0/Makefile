SHELL := /bin/bash
.SILENT:

default:
	gcc -g lab0.c -o lab0

check: default clearLog testCatch testInputFail testOutputFail testNoOptions testInput testOutput testInputAndOutput testAllOptions
	cat testLogFile.txt
	touch testEmptyFile.txt
	cmp testLogFile.txt testEmptyFile.txt &>/dev/null; \
	if [ $$? -eq 0 ]; \
	then \
		echo "(All checks succeeded)"; \
	else \
		echo "(One or more checks failed. See descriptions above)"; \
	fi
	rm -f testLogFile.txt testEmptyFile.txt

clearLog:
	> testLogFile.txt

testCatch:
	echo | ./lab0 --segfault --catch &>/dev/null; \
	if [ $$? -ne 3 ]; \
	then \
		 echo "Test failed: Options '--segfault --catch' did not catch a segmentation fault" >> testLogFile.txt; \
	fi

testInputFail:
	touch testInputFile.txt; \
	chmod u-r testInputFile.txt; \
	./lab0 --input=testInputFile.txt &>/dev/null; \
	if [ $$? -ne 1 ]; \
	then \
		 echo "Test failed: An unreadable file failed to cause an exit status of 1" >> testLogFile.txt; \
	fi; \
	rm -f testInputFile.txt

testOutputFail:
	touch testOutputFile.txt; \
	chmod u-w testOutputFile.txt; \
	echo | ./lab0 --output=testOutputFile.txt &>/dev/null; \
	if [ $$? -ne 2 ]; \
	then \
		 echo "Test failed: An unwritable file failed to cause an exit status of 2" >> testLogFile.txt; \
	fi
	rm -f testOutputFile.txt

testNoOptions:
	echo | ./lab0 &>/dev/null; \
	if [ $$? -ne 0 ]; \
	then \
		 echo "Test failed: Program should have exited with status 0 when no options are provided" >> testLogFile.txt; \
	fi; \

testInput:
	./lab0 --input=Makefile > testOutputFile.txt; \
	cmp Makefile testOutputFile.txt &>/dev/null; \
	if [ $$? -ne 0 ]; \
	then \
		echo "Test failed: Standard output did not match with specified file input" >> testLogFile.txt; \
	fi; \
	rm -f testOutputFile.txt

testOutput:
	cat Makefile | ./lab0 --output=testOutputFile.txt; \
	cmp Makefile testOutputFile.txt &>/dev/null; \
	if [ $$? -ne 0 ]; \
	then \
		echo "Test failed: Standard input did not match with specified file output" >> testLogFile.txt; \
	fi; \
	rm -f testOutputFile.txt

testInputAndOutput:
	./lab0 --input=Makefile --output=testOutputFile.txt; \
	cmp Makefile testOutputFile.txt &>/dev/null; \
	if [ $$? -ne 0 ]; \
	then \
		echo "Test failed: Specified input file was not redirected to specified output file" >> testLogFile.txt; \
	fi; \
	rm -f testOutputFile.txt

testAllOptions:
	./lab0 --input=Makefile --output=testOutputFile.txt --catch --segfault &>/dev/null; \
	if [ $$? -ne 3 ]; \
	then \
		echo "Test failed: Specifying all options did not exit due to the segfault handler" >> testLogFile.txt; \
	fi; \
	rm -f testOutputFile.txt

clean:
	shopt -s extglob; \
	`rm -rf !(Makefile|README|lab0.c|breakpoint.png|backtrace.png)`

dist:
	-tar -zcvf lab0-704465898.tar.gz lab0.c Makefile README backtrace.png breakpoint.png
