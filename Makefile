SRCS := servermain.cpp serverA.cpp serverB.cpp serverC.cpp student.cpp
OBJS := $(SRCS:.cpp=.o)

# Compiler and compilation flags
CC := g++
CFLAGS := -g -Wall

all: servermain serverA serverB serverC student #admin 

student: student.o
	$(CC) $(CFLAGS) -o $@ $^

servermain: servermain.o
	$(CC) $(CFLAGS) -o $@ $^

serverA: serverA.o
	$(CC) $(CFLAGS) -o $@ $^

serverB: serverB.o
	$(CC) $(CFLAGS) -o $@ $^
	
serverC: serverC.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	$(RM) servermain serverA serverB serverC student $(OBJS)
