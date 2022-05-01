.PHONY: clean
CC = g++
RM = rm
EXE = shell
OBJS = shell.o
$(EXE): $(OBJS)
	$(CC) -o $(EXE) $(OBJS)
shell.o: shell.cpp shell.h
	$(CC) -o shell.o -c shell.cpp
clean:
	$(RM) $(EXE) $(OBJS)