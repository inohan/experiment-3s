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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define BUF_READ 1024
#define BUF_SEND 1060
#define MAX_CLIENTS 30
#define COMMAND_LENGTH 500

typedef struct clientsettings {
    int id;
    char ip[17];
    unsigned int port;
    char usrname[21];
    FILE *fp;
    unsigned int volume;
    int is_muted;
} ClientSettings;

// Global variables
ClientSettings *clients[MAX_CLIENTS];
int s;

// Function prototypes
ClientSettings *client_setup(int sd, char *str);
int client_free(int sd);
ClientSettings *client_get(int sd);
int recv_all(int socket, char *buffer, int length);
int buf_w_bytes(char *sendbuf, int *len, int identifier, char *buf);
int buf_w_str(char *sendbuf, int *len, int identifier, char *str);
void handle_sigint(int sig);

int main(int argc, char **argv) {
    signal(SIGINT, handle_sigint);
    if (argc < 3) {
        perror("Usage: ./multi_client <ip> <port>");
    }
    fd_set readfds;
    int port = atoi(argv[2]);
    char *ip = argv[1];
    FILE *fp_in;
    int max_sd, fd_in, len_buf;
    int scan_type, scan_client, scan_len;
    int is_muted = 0;
    char buf[BUF_READ];
    char sendbuf[BUF_SEND];
    char buf_identifier[12];
    buf_identifier[11] = '\0';

    if ((s = socket(PF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
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

    if ((fp_in = popen("rec -t s16 -c 1 -r 44100 -q --buffer 1024 -", "r")) == NULL) {
        perror("popen");
        return 1;
    }
    fd_in = fileno(fp_in);

    // Send initial information (mute, user name) first
    len_buf = sizeof(char);
    buf_w_bytes(sendbuf, &len_buf, 3, (char *)&is_muted);
    send(s, sendbuf, len_buf, 0);
    fprintf(stderr, is_muted ? ">> Muted\n" : ">> Unmuted\n");
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-u") == 0) {
            if (strlen(argv[i + 1]) > 20) {
                fprintf(stderr, "Username too long\n");
            } else {
                buf_w_str(sendbuf, &len_buf, 4, argv[i + 1]);
                send(s, sendbuf, len_buf, 0);
                fprintf(stderr, ">> Name changed to %s\n", argv[i + 1]);
            }
            break;
        }
    }

    while (1) {
        FD_ZERO(&readfds);
        // Incoming data from server
        FD_SET(s, &readfds);
        // Incoming data from stdin
        FD_SET(STDIN_FILENO, &readfds);
        // Incoming data from sox (mic)
        FD_SET(fd_in, &readfds);
        if (s > STDIN_FILENO && s > fd_in) {
            max_sd = s;
        } else if (fd_in > STDIN_FILENO && fd_in > s) {
            max_sd = fd_in;
        } else {
            max_sd = STDIN_FILENO;
        }
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR)) {
            perror("select");
        }
        // Incoming data from server
        if (FD_ISSET(s, &readfds)) {
            int actual_scanned;
            if (recv_all(s, buf_identifier, 11) == 0) {
                fprintf(stderr, "Server disconnected\n");
                break;
            }
            sscanf(buf_identifier, "%d.%d.%d>>", &scan_type, &scan_client, &scan_len);
            if ((actual_scanned = recv_all(s, buf, scan_len)) != scan_len) {
                fprintf(stderr, "[%s]", buf_identifier);
                fprintf(stderr, "Error: expected %d bytes, got %d bytes\n", scan_len, actual_scanned);
            }
            switch (scan_type) {
                case 0:  // message from server
                    fprintf(stderr, "Connection established as Client %s\n", buf);
                    break;
                case 1:  // new client
                    client_setup(scan_client, buf);
                    fprintf(stderr, "%s>> Connected\n", client_get(scan_client)->usrname);
                    break;
                case 2:  // client disconnected
                    fprintf(stderr, "%s>> Disconnected\n", client_get(scan_client)->usrname);
                    client_free(scan_client);
                    break;
                case 3:  // client data
                    if (client_get(scan_client)->fp < 0) {
                        fprintf(stderr, "%s not ready\n", client_get(scan_client)->usrname);
                        break;
                    }
                    fwrite(buf, sizeof(char), scan_len, client_get(scan_client)->fp);
                    break;
                case 4:  // Client chat
                    fprintf(stderr, "%s>> %s", client_get(scan_client)->usrname, buf);
                    break;
                case 5:  // Mute change
                    client_get(scan_client)->is_muted = buf[0];
                    fprintf(stderr, "%s>> %s\n", client_get(scan_client)->usrname, client_get(scan_client)->is_muted ? "muted" : "unmuted");
                    break;
                case 6:  // Name change
                    strcpy(client_get(scan_client)->usrname, buf);
                    fprintf(stderr, "Name: %s\n", client_get(scan_client)->usrname);
                    if (strcmp(client_get(scan_client)->usrname, "") == 0) {
                        snprintf(client_get(scan_client)->usrname, 21, "Client %d", scan_client);
                    }
                    fprintf(stderr, "Client %d>> Name changed to %s\n", client_get(scan_client)->id, buf);
                    break;
                default:
                    break;
            }
        }
        // Input from STDIN
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            fgets(buf, BUF_READ, stdin);
            if (strncmp(buf, "/", 1) == 0) {  // Commands
                if (strncmp(buf, "/mute", 5) == 0) {
                    is_muted = 1;
                    len_buf = sizeof(char);
                    buf_w_bytes(sendbuf, &len_buf, 3, (char *)&is_muted);
                    send(s, sendbuf, len_buf, 0);
                    fprintf(stderr, ">> Muted\n");
                } else if (strncmp(buf, "/unmute", 7) == 0) {
                    is_muted = 0;
                    len_buf = sizeof(char);
                    buf_w_bytes(sendbuf, &len_buf, 3, (char *)&is_muted);
                    send(s, sendbuf, len_buf, 0);
                    fprintf(stderr, ">> Unmuted\n");
                } else if (strncmp(buf, "/name", 5) == 0) {
                    buf[strlen(buf) - 1] = '\0';
                    if (strlen(buf + 6) > 20) {
                        fprintf(stderr, "Name too long\n");
                    } else {
                        buf_w_str(sendbuf, &len_buf, 4, buf + 6);
                        send(s, sendbuf, len_buf, 0);
                        fprintf(stderr, ">> Name changed to %s\n", buf + 6);
                    }
                } else {
                    fprintf(stderr, "Unknown command\n");
                }
            } else {
                buf_w_str(sendbuf, &len_buf, 2, buf);
                send(s, sendbuf, len_buf, 0);
            }
        }

        // Input from sox (mic)
        if (FD_ISSET(fd_in, &readfds)) {
            if ((len_buf = read(fd_in, buf, BUF_READ)) > 0) {
                if (!is_muted) {
                    buf_w_bytes(sendbuf, &len_buf, 1, buf);
                    send(s, sendbuf, len_buf, 0);
                }
            }
        }
    }
    handle_sigint(SIGINT);
}

ClientSettings *client_setup(int sd, char *str) {
    ClientSettings *client = malloc(sizeof(ClientSettings));
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == NULL) {
            clients[i] = client;
            break;
        }
    }
    client->id = sd;
    char *token = strtok(str, ":");
    strcpy(client->ip, token);
    token = strtok(NULL, ":");
    client->port = atoi(token);
    snprintf(client->usrname, 21, "Client %d", client->id);
    client->volume = 100;
    client->is_muted = 0;
    if (client->fp != NULL) {
        fprintf(stderr, "Audio output for %s already exists\n", client->usrname);
    } else {
        if ((client->fp = popen("play -t s16 -c 1 -r 44100 -q --buffer 1024 -", "w")) == NULL) {
            fprintf(stderr, "Failed to open audio output for %s\n", client->usrname);
        }
    }
    return client;
}

int client_free(int sd) {
    ClientSettings *client = NULL;
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != NULL && clients[i]->id == sd) {
            client = clients[i];
            break;
        }
    }
    if (client == NULL) {
        return -1;
    }
    if (client->fp != NULL) {
        pclose(client->fp);
        client->fp = NULL;
    }
    free(client);
    clients[i] = NULL;
}

ClientSettings *client_get(int sd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != NULL && clients[i]->id == sd) {
            return clients[i];
        }
    }
    return NULL;
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

int buf_w_bytes(char *sendbuf, int *len, int identifier, char *buf) {
    if (*len > BUF_READ) {
        fprintf(stderr, "%d bytes exceeds buffer size\n", *len);
        return -1;
    }
    snprintf(sendbuf, BUF_SEND, "%d.%04d>>", identifier, *len);
    bcopy(buf, sendbuf + 8, *len);
    *len += 8;
    return 0;
}

int buf_w_str(char *sendbuf, int *len, int identifier, char *str) {
    if (strlen(str) + 1 > BUF_READ) {
        fprintf(stderr, "String %s exceeds buffer size\n", str);
        return -1;
    }
    snprintf(sendbuf, BUF_SEND, "%d.%04ld>>%s", identifier, strlen(str) + 1, str);
    *len = 8 + strlen(str) + 1;
    return 0;
}

void handle_sigint(int sig) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_free(i);
    }
    close(s);
    exit(0);
}