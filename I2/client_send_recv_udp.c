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

int is_end(char *data);

int main(int argc, char **argv) {
    if (argc < 3) {
        perror("Usage: ./client_recv <ip> <port>");
    }
    int port = atoi(argv[2]);
    char *ip = argv[1];
    char data[1000];
    int data_read = 0;
    int s = socket(PF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    if (inet_aton(ip, &addr.sin_addr) == 0) {
        perror("inet_aton");
        return 1;
    }
    addr.sin_port = htons(port);
    socklen_t addr_len = sizeof(addr);
    // int ret = connect(s, (struct sockaddr *)&addr, sizeof(addr));
    // if (ret == -1) {
    //     perror("connect");
    //     return 1;
    // }
    int c_sent = 0;
    perror("Sending");
    while ((data_read = read(STDIN_FILENO, data, 1000)) > 0 && c_sent < 50) {
        sendto(s, data, data_read, 0, (struct sockaddr *)&addr, sizeof(addr));
        c_sent++;
    }
    // EOD
    for (int i = 0; i < 1000; i++) {
        data[i] = 1;
    }
    sendto(s, data, 1000, 0, (struct sockaddr *)&addr, sizeof(addr));
    perror("Sent");
    while ((data_read = recvfrom(s, data, 1000, 0, (struct sockaddr *)&addr, &addr_len)) > 0) {
        if (is_end(data)) {
            break;
        }
        write(STDOUT_FILENO, data, data_read);
    }
    perror("Done");
    close(s);
}

int is_end(char *data) {
    for (int i = 0; i < 1000; i++) {
        if (data[i] == 1) {
            continue;
        } else {
            return 0;
        }
    }
    return 1;
}