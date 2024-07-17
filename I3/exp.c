#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    char *fifo1 = "fifo1";
    char *fifo2 = "fifo2";
    int fd1, fd2;
    fprintf(stderr, "Opening fifo1\n");
    fd1 = open(fifo1, O_WRONLY);
    fprintf(stderr, "Opened fifo1\nOpening fifo2\n");
    fd2 = open(fifo2, O_WRONLY);
    fprintf(stderr, "Opened fifo2\n");
    write(fd1, "hello", 6);
    write(fd2, "world", 6);
    fprintf(stderr, "Closing fd2");
    close(fd2);
    fprintf(stderr, "Closed fd2\nClosing fd1");
    close(fd1);
    fprintf(stderr, "Closed fd1\n");
}