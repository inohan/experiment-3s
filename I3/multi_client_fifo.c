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
#include <unistd.h>
#define BUF_READ 1024
#define MAX_CLIENTS 30

typedef struct clientsettings {
    int id;
    char ip[17];
    unsigned int port;
    char usrname[21];
    char filename[21];
    int fd;
    unsigned int volume;
} ClientSettings;

// Global variables
ClientSettings *clients[MAX_CLIENTS];
int s;
FILE *fp_outaudio;

// Function prototypes
ClientSettings *client_setup(int sd, char *str);
int client_free(int sd);
ClientSettings *client_get(int sd);
int outaudio_free();
int outaudio_init();
int recv_all(int socket, char *buffer, int length);
int read_all(int fd, char *buffer, int length);
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
                    client_setup(scan_client, buf);
                    fprintf(stderr, "New client: %s\n", client_get(scan_client)->usrname);
                    outaudio_free();
                    outaudio_init();
                    break;
                case 2:  // client disconnected
                    fprintf(stderr, "%s disconnected\n", client_get(scan_client)->usrname);
                    outaudio_free();
                    client_free(scan_client);
                    outaudio_init();
                    break;
                case 3:  // client data
                    if (client_get(scan_client)->fd == 0) {
                        fprintf(stderr, "%s not ready\n", client_get(scan_client)->usrname);
                        break;
                    }
                    write(client_get(scan_client)->fd, buf, scan_len);
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
    token = strtok(NULL, ":");
    strcpy(client->usrname, token);
    client->volume = 100;
    int fn_index = 0;
    while (1) {
        snprintf(client->filename, 21, "tmp%02d.tmp", fn_index++);
        if (access(client->filename, F_OK) == -1) {
            break;
        }
    }
    if (mkfifo(client->filename, 0666) == -1) {
        fprintf(stderr, "Error creating fifo: %s\n", client->filename);
    }
    return client;
}

int client_free(int sd) {
    if (fp_outaudio != NULL) {
        fprintf(stderr, "Outaudio still running\n");
        return -1;
    }
    ClientSettings *client;
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
    remove(client->filename);
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

int outaudio_free() {
    if (fp_outaudio == NULL) {
        return -1;
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == NULL) {
            continue;
        }
        // Close all fifo files for clients
        close(clients[i]->fd);
        clients[i]->fd = 0;
    }
    fprintf(stderr, "Closing outaudio\n");
    pclose(fp_outaudio);
    fp_outaudio = NULL;
    fprintf(stderr, "Freed outaudio\n");
    return 0;
}

int outaudio_init() {
    if (fp_outaudio != NULL) {
        fprintf(stderr, "Outaudio already initialized\n");
        return -1;
    }
    char outcommand[500] = "sox --buffer 1024 -S -m -r 44100 -t s16 -c 1 -n";
    int filecount = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != NULL) {
            filecount++;
        }
    }
    if (filecount == 0) {
        return -1;
    }
    int last_index = strlen(outcommand);
    for (int i = 0; i < MAX_CLIENTS || last_index >= 300; i++) {
        if (clients[i] != NULL) {
            last_index += snprintf(outcommand + last_index, 300 - last_index, " -t s16 -c 1 -r 44100 %s", clients[i]->filename);
        }
    }
    last_index += snprintf(outcommand + last_index, 300 - last_index, " -d");
    if (last_index >= 300) {
        fprintf(stderr, "Command too long\n");
        return -1;
    }
    fp_outaudio = popen(outcommand, "w");
    if (fp_outaudio == NULL) {
        fprintf(stderr, "Error opening outaudio\n");
        return -1;
    }
    fprintf(stderr, "Command running: %s\n", outcommand);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != NULL) {
            fprintf(stderr, "Opening %s (channel %d)\n", clients[i]->filename, clients[i]->id);
            clients[i]->fd = open(clients[i]->filename, O_WRONLY);
            fprintf(stderr, "Opened %s as fd %d\n", clients[i]->filename, clients[i]->fd);
        }
    }
    return 0;
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
    outaudio_free();
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_free(i);
    }
    close(s);
    exit(0);
}