CC=gcc
CFLAGS=-lm -Wall -g -UNDEBUG
#CFLAGS=-lm -Wall -O3 -march=native -DNDEBUG
DEPS = parser.h pool.h datatypes.h analysis.h toolbox.h
OBJ = main.o pool.o parser_core.o parser.o analysis.o mikami.o toolbox.o

NAME = cadet

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

$(NAME): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^


.PHONY: clean

clean:
	rm -f *.o *~ core $(NAME)

archive:
	git archive --format=zip master -o $(NAME).zip
