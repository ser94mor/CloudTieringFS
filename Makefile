CFLAGS += -g -std=c11 -Wall -Iinclude/
SRCS += src/evict-queue.c \
        src/tier-engine.c
TST_SRCS += src/evict-queue.c \
            test/test-evict-queue.c

APP_NAME := tier-engine

all: app test

app: mkdir-bin
	gcc ${CFLAGS} ${SRCS} -o bin/${APP_NAME}

test: test-evict-queue

test-evict-queue: mkdir-bin
	gcc ${CFLAGS} ${TST_SRCS} -o bin/test-evict-queue

mkdir-bin:
	mkdir -p bin/

clean:
	rm -rf bin/
