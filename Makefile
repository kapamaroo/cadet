CC=gcc
CFLAGS=-lm -Wall -g -UNDEBUG
#CFLAGS=-lm -Wall -O3 -march=native -DNDEBUG
DEPS = parser.h pool.h datatypes.h
OBJ = main.o pool.o parser_core.o parser.o analysis.o mikami.o

NAME = cadet

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(NAME): $(OBJ)
	gcc -o $@ $^ $(CFLAGS)


.PHONY: clean

clean:
	rm -f *.o *~ core $(NAME)

archive:
	git archive --format=zip master -o $(NAME).zip
