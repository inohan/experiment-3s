#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define BUF_READ 1024
#define BUF_SEND 1060
#define MAX_CLIENTS 30

int send_all(int *client_sockets, int client_exclude, char *msg, int len);
int recv_all(int socket, char *buffer, int length);

int main(int argc, char **argv) {
    int ss, port, addrlen, max_sd, sock_client, sd, valread;
    struct sockaddr_in addr;
    fd_set readfds;
    int client_sockets[MAX_CLIENTS];
    char readbuf[BUF_READ];
    char sendbuf[BUF_SEND];
    char ipbuffer[30];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_sockets[i] = 0;
    }

    if (argc < 2) {
        perror("Usage: ./multi_server <port>");
        exit(EXIT_FAILURE);
    }
    // Server
    if ((port = atoi(argv[1])) == 0) {
        perror("port");
        exit(EXIT_FAILURE);
    }
    if ((ss = socket(PF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    char msg_connect[200];
    snprintf(msg_connect, 200, "0.%02d.0009>>Connected", ss);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(ss, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    printf("Listening on port %d\n", port);
    if (listen(ss, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    addrlen = sizeof(addr);
    printf("Waiting for connections...\n");

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(ss, &readfds);
        max_sd = ss;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];
            if (sd > 0) {
                FD_SET(sd, &readfds);
            }
            if (sd > max_sd) {
                max_sd = sd;
            }
        }
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR)) {
            perror("select");
        }

        // Incoming connection
        if (FD_ISSET(ss, &readfds)) {
            if ((sock_client = accept(ss, (struct sockaddr *)&addr, (socklen_t *)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }
            snprintf(ipbuffer, 30, "%s:%d:Client %d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), sock_client);
            snprintf(sendbuf, BUF_SEND, "1.%02d.%04ld>>%s", sock_client, strlen(ipbuffer) + 1, ipbuffer);
            puts(sendbuf);
            if (send_all(client_sockets, sock_client, sendbuf, 11 + strlen(ipbuffer) + 1) < 0) {
                perror("send_all");
            }
            if (send(sock_client, msg_connect, strlen(msg_connect), 0) != strlen(msg_connect)) {
                perror("send");
            }
            int is_registered = 0;
            // Add new socket to array of sockets
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0 && !is_registered) {
                    client_sockets[i] = sock_client;
                    is_registered = 1;
                } else if (client_sockets[i] != 0) {  // Send old clients to new client
                    char ipbuffer_other[30];
                    getpeername(client_sockets[i], (struct sockaddr *)&addr, (socklen_t *)&addrlen);
                    snprintf(ipbuffer_other, 30, "%s:%d:Client %d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), client_sockets[i]);
                    snprintf(sendbuf, BUF_SEND, "1.%02d.%04ld>>%s", client_sockets[i], strlen(ipbuffer_other) + 1, ipbuffer);
                    if (send(sock_client, sendbuf, 11 + strlen(ipbuffer_other) + 1, 0) != 11 + strlen(ipbuffer_other) + 1) {
                        perror("send_all");
                    }
                }
            }
        }

        // Other sockets (client)
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];
            if (FD_ISSET(sd, &readfds)) {
                if ((valread = recv_all(sd, readbuf, BUF_READ)) == 0) {
                    // Disconnected
                    getpeername(sd, (struct sockaddr *)&addr, (socklen_t *)&addrlen);
                    snprintf(ipbuffer, 30, "%s:%d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
                    snprintf(sendbuf, BUF_SEND, "2.%02d.%04ld>>%s", sd, strlen(ipbuffer) + 1, ipbuffer);
                    puts(sendbuf);
                    if (send_all(client_sockets, sd, sendbuf, 11 + strlen(ipbuffer) + 1) < 0) {
                        perror("send_all");
                    }
                    close(sd);
                    client_sockets[i] = 0;
                } else {
                    // Received
                    if (valread != BUF_READ) {
                        readbuf[valread] = '\0';
                    }
                    snprintf(sendbuf, BUF_SEND, "3.%02d.%04d>>", sd, valread);
                    bcopy(readbuf, sendbuf + 11, valread);
                    puts(sendbuf);
                    if (send_all(client_sockets, sd, sendbuf, 11 + valread) < 0) {
                        perror("send_all");
                    }
                }
            }
        }
    }
    close(ss);
    return 0;
}

int send_all(int *client_sockets, int client_exclude, char *msg, int len) {
    int flag = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != 0 && client_sockets[i] != client_exclude) {
            if (send(client_sockets[i], msg, len, 0) != len) {
                fprintf(stderr, "send_all: %d\n", client_sockets[i]);
                flag = 1;
            }
        }
    }
    return flag;
}

int recv_all(int socket, char *buffer, int length) {
    int total_received = 0;
    int bytes_left = length;
    int n;

    while (total_received < length) {
        n = recv(socket, buffer + total_received, bytes_left, 0);
        if (n <= 0) {
            // Handle error or disconnection
            return n;
        }
        total_received += n;
        bytes_left -= n;
    }

    return total_received;
}