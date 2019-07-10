all: test_ccli

test_ccli: ccli.c test_ccli.c
	gcc -Wall ccli.c test_ccli.c -o test_ccli

.PHONY: test_ccli