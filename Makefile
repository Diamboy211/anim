CC ?= gcc
CFLAGS ?= -O3 -flto -march=native -Wall -Wextra -pedantic -std=c11
LDFLAGS ?= 
LIBS=-lSDL3 -lSDL3_image

all: engine script offline

engine: anim.exe

script: script.dll

offline: offline.exe

anim.exe: main.o polygon.o anim_internal.o loader.o
	${CC} ${CFLAGS} ${LDFLAGS} -o anim.exe main.o polygon.o anim_internal.o loader.o ${LIBS}
offline.exe: offline.o polygon.o anim_internal.o loader.o
	${CC} ${CFLAGS} ${LDFLAGS} -o offline.exe offline.o polygon.o anim_internal.o loader.o ${LIBS}
anim_internal.o: anim_internal.c
	${CC} -c ${CFLAGS} -MMD -MF anim_internal.d -o anim_internal.o anim_internal.c
main.o: main.c
	${CC} -c ${CFLAGS} -MMD -MF main.d -o main.o main.c
polygon.o: polygon.c
	${CC} -c ${CFLAGS} -MMD -MF polygon.d -o polygon.o polygon.c
loader.o: loader.c
	${CC} -c ${CFLAGS} -MMD -MF loader.d -o loader.o loader.c
offline.o: offline.c
	${CC} -c ${CFLAGS} -MMD -MF offline.d -o offline.o offline.c

script.dll: script.o
	${CC} ${CFLAGS} ${LDFLAGS} -shared -o script.dll script.o
script.o: script.c
	${CC} -c ${CFLAGS} -MMD -MF script.d -fpic -o script.o script.c
clean:
	rm -f *.dll *.d *.o *.exe *.mp4

-include anim_internal.d main.d polygon.d loader.d script.d

.PHONY: all engine script offline clean
