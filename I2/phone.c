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
#define BUF_SIZE 1024

int server_await(int port, int *sock_client, struct sockaddr_in *client_addr);
int client_wait(char *ip, int port, int *sock_client, struct sockaddr_in *client_addr);
int send_audio(int sock, FILE *fp);
int recv_audio(int sock, FILE *fp);

int main(int argc, char **argv) {
    int sock_send;
    struct sockaddr_in sender_addr;
    if (argc >= 3 && strcmp(argv[1], "-s") == 0) {  // Server
        int port = atoi(argv[2]);
        if ((server_await(port, &sock_send, &sender_addr)) != 0) {
            perror("server_await");
            return 1;
        }
    } else if (argc >= 4 && strcmp(argv[1], "-c") == 0) {  // Client
        char *ip = argv[2];
        int port = atoi(argv[3]);
        if ((client_wait(ip, port, &sock_send, &sender_addr)) != 0) {
            perror("client_wait");
            return 1;
        }
    } else {
        perror("Usage: ./serv_send [-s|-c <ip>] <port>");
        return 1;
    }

    struct in_addr ipAddr = sender_addr.sin_addr;
    char ipstr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ipAddr, ipstr, INET_ADDRSTRLEN);
    printf("Accepted connection from %s:%d\n", ipstr, ntohs(sender_addr.sin_port));
    // Open audio
    FILE *fp_in;
    FILE *fp_out;
    if ((fp_in = popen("rec -q -t raw -b 16 -c 1 -e s -r 44100 -", "r")) == NULL) {
        perror("fp_in");
        return 1;
    }
    printf("Finished setting up rec\n");
    if ((fp_out = popen("play -q -t raw -b 16 -c 1 -e s -r 44100 -", "w")) == NULL) {
        perror("fp_out");
        return 1;
    }
    printf("Finished setting up play\n");
    char buf_in[BUF_SIZE];
    char buf_out[BUF_SIZE];
    printf("start sending\n");
    for (int i = 0; i < 10; i++) {
        send_audio(sock_send, fp_in);
    }
    while (1) {
        // Send audio
        send_audio(sock_send, fp_in);
        // Receive audio
        recv_audio(sock_send, fp_out);
    }
    close(sock_send);
}

int server_await(int port, int *sock_client, struct sockaddr_in *client_addr) {
    int ss = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    int ret = bind(ss, (struct sockaddr *)&addr, sizeof(addr));
    if (ret == -1) {
        perror("connect");
        return 1;
    }
    listen(ss, 10);
    socklen_t len = sizeof(struct sockaddr_in);
    *sock_client = accept(ss, (struct sockaddr *)client_addr, &len);
    close(ss);
    return 0;
}

int client_wait(char *ip, int port, int *sock_client, struct sockaddr_in *client_addr) {
    *sock_client = socket(PF_INET, SOCK_STREAM, 0);
    client_addr->sin_family = AF_INET;
    if (inet_aton(ip, &client_addr->sin_addr) == 0) {
        perror("inet_aton (ip address)");
        return 1;
    }
    client_addr->sin_port = htons(port);
    int ret = connect(*sock_client, (struct sockaddr *)client_addr, sizeof(struct sockaddr_in));
    if (ret == -1) {
        perror("connect");
        return 1;
    }
    return 0;
}

int send_audio(int sock, FILE *fp) {
    char buf[BUF_SIZE];
    int s_read = fread(buf, sizeof(char), BUF_SIZE, fp);
    if (s_read == 0) {
        return 1;
    }
    printf("IN/SEND : %d\n", s_read);
    send(sock, buf, s_read, 0);
    return 0;
}

int recv_audio(int sock, FILE *fp) {
    char buf[BUF_SIZE];
    int s_read = recv(sock, buf, BUF_SIZE, 0);
    if (s_read == 0) {
        return 1;
    }
    printf("OUT/RECV: %d\n", s_read);
    fwrite(buf, sizeof(char), s_read, fp);
    return 0;
}