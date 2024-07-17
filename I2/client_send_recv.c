#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 3) {
        perror("Usage: ./client_recv <ip> <port>");
    }
    int port = atoi(argv[2]);
    char *ip = argv[1];
    char data;
    int s = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    if (inet_aton(ip, &addr.sin_addr) == 0) {
        perror("inet_aton");
        return 1;
    }
    addr.sin_port = htons(port);
    int ret = connect(s, (struct sockaddr *)&addr, sizeof(addr));
    if (ret == -1) {
        perror("connect");
        return 1;
    }
    perror("Sending");
    while (read(STDIN_FILENO, &data, 1) > 0) {
        if (data == EOF) {
            break;
        }
        send(s, &data, 1, 0);
    }
    // shutdown(s, SHUT_WR);
    perror("Sent");
    while (recv(s, &data, 1, 0) > 0) {
        write(STDOUT_FILENO, &data, 1);
    }
    perror("Done");
    close(s);
}