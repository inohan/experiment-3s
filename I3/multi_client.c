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
#define MAX_CLIENTS 30

typedef struct clientsettings {
    char ip[17];
    unsigned int port;
    char usrname[21];
    FILE *fp;
    char *filename;
    unsigned int volume;
} ClientSettings;

// Global variables
ClientSettings *clients[MAX_CLIENTS];
int s;
FILE *fp_outaudio;

// Function prototypes
ClientSettings *client_setup(char *str);
void client_free(ClientSettings *client);
int recv_all(int socket, char *buffer, int length);
int read_all(int fd, char *buffer, int length);
void handle_sigint(int sig);
void update_outcommand();

int main(int argc, char **argv) {
    signal(SIGINT, handle_sigint);
    if (argc < 3) {
        perror("Usage: ./multi_client <ip> <port>");
    }
    fd_set readfds;
    int port = atoi(argv[2]);
    char *ip = argv[1];
    FILE *fp_in;
    int max_sd, fd_in, len_read;
    int scan_type, scan_client, scan_len;
    char buf[BUF_READ];
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
            if (recv_all(s, buf_identifier, 11) != 11) {
                perror("recv header");
            }
            sscanf(buf_identifier, "%d.%d.%d>>", &scan_type, &scan_client, &scan_len);
            if ((actual_scanned = recv_all(s, buf, scan_len)) != scan_len) {
                fprintf(stderr, "[%s]", buf_identifier);
                fprintf(stderr, "Error: expected %d bytes, got %d bytes\n", scan_len, actual_scanned);
            }
            switch (scan_type) {
                case 0:  // message from client
                    break;
                case 1:  // new client
                    fprintf(stderr, "New client: %d\n", scan_client);
                    clients[scan_client] = client_setup(buf);
                    update_outcommand();
                    break;
                case 2:  // client disconnected
                    fprintf(stderr, "%s disconnected\n", clients[scan_client]->usrname);
                    client_free(clients[scan_client]);
                    clients[scan_client] = NULL;
                    update_outcommand();
                    break;
                case 3:  // client data
                    fwrite(buf, 1, scan_len, clients[scan_client]->fp);
                    break;
                default:
                    break;
            }
        }
        // Input from STDIN
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            fgets(buf, BUF_READ, stdin);
            printf("Input from stdin: %s\n", buf);
        }

        // Input from sox (mic)
        if (FD_ISSET(fd_in, &readfds)) {
            if ((len_read = read_all(fd_in, buf, BUF_READ)) > 0) {
                send(s, buf, len_read, 0);
            }
        }
    }
    pclose(fp_in);
    close(s);
}

ClientSettings *client_setup(char *str) {
    ClientSettings *client = malloc(sizeof(ClientSettings));
    char *token = strtok(str, ":");
    strcpy(client->ip, token);
    token = strtok(NULL, ":");
    client->port = atoi(token);
    token = strtok(NULL, ":");
    strcpy(client->usrname, token);
    client->volume = 100;
    client->filename = malloc(sizeof(char) * 20);
    int fn_index = 0;
    while (1) {
        snprintf(client->filename, 20, "tmp%02d.tmp", fn_index++);
        if (access(client->filename, F_OK) == -1) {
            break;
        }
    }
    mkfifo(client->filename, 0666);
    client->fp = fopen(client->filename, "wb");
    return client;
}

void client_free(ClientSettings *client) {
    if (fp_outaudio != NULL) {
        pclose(fp_outaudio);
        fp_outaudio = NULL;
    }
    if (client->fp != NULL) {
        fclose(client->fp);
    }
    if (client->filename != NULL) {
        remove(client->filename);
        free(client->filename);
    }
    free(client);
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

int read_all(int fd, char *buffer, int length) {
    int total_received = 0;
    int bytes_left = length;
    int n;

    while (total_received < length) {
        n = read(fd, buffer + total_received, bytes_left);
        if (n <= 0) {
            // Handle error or disconnection
            return n;
        }
        total_received += n;
        bytes_left -= n;
    }

    return total_received;
}

void handle_sigint(int sig) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != NULL) {
            client_free(clients[i]);
            clients[i] = NULL;
        }
    }
    close(s);
    exit(0);
}

void update_outcommand() {
    if (fp_outaudio != NULL) {
        pclose(fp_outaudio);
        fp_outaudio = NULL;
    }
    char outcommand[300] = "sox --buffer 1024";
    int filecount = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != NULL) {
            filecount++;
        }
    }
    if (filecount >= 2) {
        strcat(outcommand, " -m");
    } else if (filecount == 0) {
        return;
    }
    int last_index = strlen(outcommand);
    for (int i = 0; i < MAX_CLIENTS || last_index >= 300; i++) {
        if (clients[i] != NULL) {
            last_index += snprintf(outcommand + last_index, 300 - last_index, " -t s16 -c 1 -r 44100 %s", clients[i]->filename);
        }
    }
    last_index += snprintf(outcommand + last_index, 300 - last_index, " -d");
    fprintf(stderr, "command running: %s\n", outcommand);
    fp_outaudio = popen(outcommand, "r");
}