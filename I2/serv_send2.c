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
    if (argc < 2) {
        perror("Usage: ./serv_send <port>");
    }
    int port = atoi(argv[1]);
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
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(struct sockaddr_in);
    int s = accept(ss, (struct sockaddr *)&client_addr, &len);
    // Show IP and port of accepted client
    struct sockaddr_in *pV4addr = (struct sockaddr_in *)&client_addr;
    struct in_addr ipAddr = pV4addr->sin_addr;
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ipAddr, str, INET_ADDRSTRLEN);
    printf("Accepted connection from %s:%d\n", str, ntohs(pV4addr->sin_port));
    close(ss);
    // Connected
    FILE *fp;
    if ((fp = popen("rec -t raw -b 16 -c 1 -e s -r 44100 -", "r")) == NULL) {
        perror("popen");
        return 1;
    }

    char buf[1024];
    int s_buf;
    while ((s_buf = fread(buf, sizeof(char), 1024, fp)) > 0) {
        send(s, buf, s_buf, 0);
    }
    shutdown(s, SHUT_WR);
    close(s);
}