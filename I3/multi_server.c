#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <signal.h>
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

// Global
int client_sockets[MAX_CLIENTS];
int muted[MAX_CLIENTS];
char usernames[MAX_CLIENTS][21];
int ss;

int send_all(int *client_sockets, int client_exclude, char *msg, int len);
int recv_all(int socket, char *buffer, int length);
int buf_w_bytes(char *sendbuf, int *len, int identifier, int sd, char *buf);
int buf_w_str(char *sendbuf, int *len, int identifier, int sd, char *str);
int handle_sigint(int sig);

int main(int argc, char **argv) {
    int port;
    int addrlen, max_sd, sd, len_buf, recv_identifier;
    struct sockaddr_in addr;
    fd_set readfds;
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
            if ((sd = accept(ss, (struct sockaddr *)&addr, (socklen_t *)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }
            // Send to preexisting clients the information of newly connected client
            snprintf(readbuf, BUF_READ, "%s:%d:Client %d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), sd);
            buf_w_str(sendbuf, &len_buf, 1, sd, readbuf);
            puts(sendbuf);
            if (send_all(client_sockets, sd, sendbuf, len_buf) < 0) {
                perror("send_all");
            }
            snprintf(readbuf, BUF_READ, "%d", sd);
            buf_w_str(sendbuf, &len_buf, 0, ss, readbuf);
            if (send(sd, sendbuf, len_buf, 0) != len_buf) {
                perror("send");
            }
            int is_registered = 0;
            // Add new socket to array of sockets
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0 && !is_registered) {
                    client_sockets[i] = sd;
                    muted[i] = 0;
                    usernames[i][0] = '\0';
                    is_registered = 1;
                } else if (client_sockets[i] != 0) {  // Send old clients to new client
                    getpeername(client_sockets[i], (struct sockaddr *)&addr, (socklen_t *)&addrlen);
                    snprintf(readbuf, BUF_READ, "%s:%d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
                    buf_w_str(sendbuf, &len_buf, 1, client_sockets[i], readbuf);
                    if (send(sd, sendbuf, len_buf, 0) != len_buf) {
                        perror("send_all");
                    }
                    // Mute status
                    len_buf = sizeof(char);
                    buf_w_bytes(sendbuf, &len_buf, 5, client_sockets[i], (char *)&muted[i]);
                    if (send(sd, sendbuf, len_buf, 0) != len_buf) {
                        perror("send_all");
                    }
                    // Username
                    if (strcmp(usernames[i], "") != 0) {  // If not blank
                        buf_w_str(sendbuf, &len_buf, 6, client_sockets[i], usernames[i]);
                        if (send(sd, sendbuf, len_buf, 0) != len_buf) {
                            perror("send_all");
                        }
                    }
                }
            }
        }

        // Other sockets (client)
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];
            if (FD_ISSET(sd, &readfds)) {
                readbuf[8] = '\0';
                if ((len_buf = recv_all(sd, readbuf, 8)) == 0) {
                    // Disconnected
                    getpeername(sd, (struct sockaddr *)&addr, (socklen_t *)&addrlen);
                    buf_w_str(sendbuf, &len_buf, 2, sd, "");
                    puts(sendbuf);
                    if (send_all(client_sockets, sd, sendbuf, len_buf) < 0) {
                        perror("send_all");
                    }
                    close(sd);
                    client_sockets[i] = 0;
                    muted[i] = 0;
                    usernames[i][0] = '\0';
                } else {
                    int actual_scanned;
                    // Get identifier
                    sscanf(readbuf, "%d.%d>>", &recv_identifier, &len_buf);
                    if ((actual_scanned = recv_all(sd, readbuf, len_buf)) != len_buf) {
                        fprintf(stderr, "Error: expected %d bytes, got %d bytes\n", len_buf, actual_scanned);
                        continue;
                    }
                    switch (recv_identifier) {
                        case 1:  // Sound data
                            buf_w_bytes(sendbuf, &len_buf, 3, sd, readbuf);
                            if (send_all(client_sockets, sd, sendbuf, len_buf) < 0) {
                                perror("send_all");
                            }
                            break;
                        case 2:  // Message
                            buf_w_str(sendbuf, &len_buf, 4, sd, readbuf);
                            if (send_all(client_sockets, sd, sendbuf, len_buf) < 0) {
                                perror("send_all");
                            }
                            break;
                        case 3:  // Mute & unmute
                            muted[i] = (int)readbuf[0];
                            len_buf = sizeof(char);
                            buf_w_bytes(sendbuf, &len_buf, 5, sd, (char *)&muted[i]);
                            if (send_all(client_sockets, sd, sendbuf, len_buf) < 0) {
                                perror("send_all");
                            }
                            break;
                        case 4:  // Username change
                            strcpy(usernames[i], readbuf);
                            buf_w_str(sendbuf, &len_buf, 6, sd, usernames[i]);
                            if (send_all(client_sockets, sd, sendbuf, len_buf) < 0) {
                                perror("send_all");
                            }
                            break;
                        default:
                            break;
                    }
                }
            }
        }
    }
    close(ss);
    return 0;
}

int send_all(int *client_sockets, int client_exclude, char *buf, int len) {
    int flag = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != 0 && client_sockets[i] != client_exclude) {
            if (send(client_sockets[i], buf, len, 0) != len) {
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

int buf_w_bytes(char *sendbuf, int *len, int identifier, int sd, char *buf) {
    if (*len > BUF_READ) {
        fprintf(stderr, "%d bytes exceeds buffer size\n", *len);
        return -1;
    }
    snprintf(sendbuf, BUF_SEND, "%d.%02d.%04d>>", identifier, sd, *len);
    bcopy(buf, sendbuf + 11, *len);
    *len += 11;
    return 0;
}

int buf_w_str(char *sendbuf, int *len, int identifier, int sd, char *str) {
    if (strlen(str) + 1 > BUF_READ) {
        fprintf(stderr, "String %s exceeds buffer size\n", str);
        return -1;
    }
    snprintf(sendbuf, BUF_SEND, "%d.%02d.%04ld>>%s", identifier, sd, strlen(str) + 1, str);
    *len = 11 + strlen(str) + 1;
    return 0;
}

int handle_sigint(int sig) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != 0) {
            close(client_sockets[i]);
        }
    }
    close(ss);
    exit(0);
}