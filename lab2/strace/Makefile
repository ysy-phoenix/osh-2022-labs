.PHONY: clean
CC = g++
RM = rm
EXE = strace
OBJS = strace.o
$(EXE): $(OBJS)
	$(CC) -o $(EXE) $(OBJS)
shell.o: strace.cpp 
	$(CC) -o strace.o -c strace.cpp
clean:
	$(RM) $(EXE) $(OBJS)