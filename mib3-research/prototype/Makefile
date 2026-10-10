CC ?= cc
AR ?= ar
CFLAGS ?= -O2
override CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Werror

.PHONY: all test sanitize asan clean
all: libcp_router.a

cp_router.o: cp_router.c cp_router.h

libcp_router.a: cp_router.o
	$(AR) rcs $@ $^

test_router: cp_router.c cp_router.h test_router.c
	$(CC) $(CFLAGS) cp_router.c test_router.c -o $@

test: test_router
	./test_router

sanitize:
	$(CC) $(CFLAGS) -O1 -g -fsanitize=undefined -fno-sanitize-recover=all cp_router.c test_router.c -o test_router_san
	./test_router_san

asan:
	$(CC) $(CFLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer cp_router.c test_router.c -o test_router_san
	./test_router_san

clean:
	rm -f cp_router.o libcp_router.a test_router test_router_san
