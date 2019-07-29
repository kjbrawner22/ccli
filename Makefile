all: test_ccli

# make sure CCLI_TESTS is defined in ccli.h header file
tests: ccli.c ccli.h
	gcc -Wall ccli.c -o tests

test_ccli: ccli.c test_ccli.c
	gcc -Wall ccli.c test_ccli.c -o test_ccli

.PHONY: test_ccli