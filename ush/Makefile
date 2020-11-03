# This is a very simple makefile
# cat -e -t -v Makefile to test for improper tabs
#
CC=gcc
CFLAGS= -g -Wall #adds debugging information and turns on most compiler warnings
OBJ = ush.o expand.o builtin.o
DEPS =  defn.h globalv.h
%.o: %.c ${DEPS}
	${CC} -c -o $@ $< ${CFLAGS}

ush: ${OBJ}
	$(CC) $(CFLAGS) -o ush ${OBJ}

clean:
	$(RM) *.o
	${RM} ush
