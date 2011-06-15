LDFLAGS += -lalpm
CFLAGS += -std=c99 -g -Wall -Wextra -pedantic

SRC := aurood.c
OBJ = ${SRC:.c=.o}

.c.o:
	${CC} -c ${CFLAGS} $<

aurood: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

strip: aurood
	strip --strip-all aurood

clean:
	${RM} ${OBJ} aurood

