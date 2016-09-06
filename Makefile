CFLAGS += -g -std=c11 -Wall -Iinc/
SRCS += src/conf.c
TST_SRCS += tst/test-suit.c \
            tst/test-conf.c

all: app test

app: mkdir-bin
	gcc ${CFLAGS} ${SRCS} -o bin/fs-monitor

test: test-suit

test-suit:
	gcc ${CFLAGS} ${TST_SRCS} ${SRCS} -o bin/test-suit

mkdir-bin:
	mkdir -p bin/

clean:
	rm -rf bin/
