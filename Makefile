
INC_TST += tst/
SRCS += src/conf.c
TST_SRCS += tst/test-suit.c \
            tst/test-conf.c

CFLAGS += -g -std=c11 -Wall -Iinc/

all: app test

app: mkdir-bin
	gcc ${CFLAGS} ${SRCS} -o bin/fs-monitor

test: test-suit

test-suit: mkdir-bin
	gcc ${CFLAGS} -ldotconf src/conf.c src/queue.c tst/test-conf.c tst/test-suit.c tst/test-queue.c -o bin/test-suit

mkdir-bin:
	mkdir -p bin/

clean:
	rm -rf bin/
