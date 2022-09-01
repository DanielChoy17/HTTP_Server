#*********************************************************************************
# * Daniel Choy
# * 2022 Spring
# * Makefile
# * Tool containing a set of commands to make the compilation process easier
#*********************************************************************************

#------------------------------------------------------------------------------
# make                   makes httpserver
# make all               makes httpserver
# make httpserver        makes httpserver
# make clean             removes all compiler generated files
# make format            formats all source and header files
#------------------------------------------------------------------------------

CC = clang
CFLAGS = -Wall -Wextra -Werror -pedantic

all: httpserver

httpserver: httpserver.o queue.o
	$(CC) $(CFLAGS) -lpthread -o httpserver httpserver.o queue.o

httpserver.o: httpserver.c
	$(CC) $(CFLAGS) -c httpserver.c

queue.o: queue.c
	$(CC) $(CFLAGS) -c queue.c

clean:
	rm -f httpserver *.o

format:
	clang-format -i -style=file *.[ch]