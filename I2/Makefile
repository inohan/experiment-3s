CC = gcc
TARGET = proj
OBJS = project1.o


$(TARGET): $(OBJS)
	$(CC) -o $@ $^

project1.o: project1.c
	$(CC) -c project1.c


.PHONY: clean
clean:
	rm *.o *~ a.out proj