#include <arpa/inet.h>
#include <fftw3.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define SAMPLE_RATE 44100

void* send_audio(void* arg);
void* recv_audio(void* arg);
void formant_shift(double* input, double* output, int size, double shift_ratio);

void start_server(int port);
void start_client(const char* server_ip, int port);

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s -s <port> (server) | %s -c <server_ip> <port> (client)\n", argv[0], argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-s") == 0 && argc == 3) {
        int port = atoi(argv[2]);
        start_server(port);
    } else if (strcmp(argv[1], "-c") == 0 && argc == 4) {
        const char* server_ip = argv[2];
        int port = atoi(argv[3]);
        start_client(server_ip, port);
    } else {
        printf("Usage: %s -s <port> (server) | %s -c <server_ip> <port> (client)\n", argv[0], argv[0]);
        return 1;
    }

    return 0;
}

void start_server(int port) {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_size = sizeof(client_addr);

    server_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("socket");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(server_sock);
        exit(1);
    }

    if (listen(server_sock, 1) == -1) {
        perror("listen");
        close(server_sock);
        exit(1);
    }

    printf("Waiting for client connection on port %d...\n", port);
    client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_size);
    if (client_sock == -1) {
        perror("accept");
        close(server_sock);
        exit(1);
    }

    printf("Client connected.\n");

    pthread_t send_thread, recv_thread;
    pthread_create(&send_thread, NULL, send_audio, (void*)&client_sock);
    pthread_create(&recv_thread, NULL, recv_audio, (void*)&client_sock);

    pthread_join(send_thread, NULL);
    pthread_join(recv_thread, NULL);

    close(client_sock);
    close(server_sock);
}

void start_client(const char* server_ip, int port) {
    int client_sock;
    struct sockaddr_in server_addr;

    client_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        perror("socket");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(port);

    if (connect(client_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        close(client_sock);
        exit(1);
    }

    printf("Connected to server %s on port %d.\n", server_ip, port);

    pthread_t send_thread, recv_thread;
    pthread_create(&send_thread, NULL, send_audio, (void*)&client_sock);
    pthread_create(&recv_thread, NULL, recv_audio, (void*)&client_sock);

    pthread_join(send_thread, NULL);
    pthread_join(recv_thread, NULL);

    close(client_sock);
}

void* send_audio(void* arg) {
    int sock = *((int*)arg);
    // Use sox to apply a pitch shift of 1200 cents (12半音)
    FILE* rec_pipe = popen("rec -t raw -b 16 -c 1 -e s -r 44100 - pitch 400", "r");
    if (!rec_pipe) {
        perror("popen rec");
        return NULL;
    }

    char buffer[BUFFER_SIZE];
    double input[BUFFER_SIZE];
    double output[BUFFER_SIZE];
    while (1) {
        size_t bytes_read = fread(buffer, 1, BUFFER_SIZE, rec_pipe);
        if (bytes_read > 0) {
            // フォルマントシフト処理
            for (size_t i = 0; i < bytes_read / sizeof(int16_t); ++i) {
                input[i] = ((int16_t*)buffer)[i];
            }
            formant_shift(input, output, bytes_read / sizeof(int16_t), 1.4);  // 2.0倍のシフト
            for (size_t i = 0; i < bytes_read / sizeof(int16_t); ++i) {
                ((int16_t*)buffer)[i] = (int16_t)output[i];
            }
            send(sock, buffer, bytes_read, 0);
        } else {
            break;
        }
    }

    pclose(rec_pipe);
    return NULL;
}

void* recv_audio(void* arg) {
    int sock = *((int*)arg);
    FILE* play_pipe = popen("play -t raw -b 16 -c 1 -e s -r 44100 -", "w");
    if (!play_pipe) {
        perror("popen play");
        return NULL;
    }

    char buffer[BUFFER_SIZE];
    while (1) {
        ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received > 0) {
            fwrite(buffer, 1, bytes_received, play_pipe);
        } else {
            break;
        }
    }

    pclose(play_pipe);
    return NULL;
}

// フォルマントシフト関数
void formant_shift(double* input, double* output, int size, double shift_ratio) {
    fftw_complex *in, *out;
    fftw_plan p;

    in = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * size);
    out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * size);

    for (int i = 0; i < size; i++) {
        in[i][0] = input[i];
        in[i][1] = 0.0;
    }

    p = fftw_plan_dft_1d(size, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(p);

    // 周波数ドメインでのシフト
    for (int i = 0; i < size; i++) {
        double freq = (double)i / size * SAMPLE_RATE;
        double new_freq = freq * shift_ratio;
        int new_index = (int)(new_freq / SAMPLE_RATE * size);
        if (new_index < size) {
            output[new_index] = out[i][0];
        }
    }

    p = fftw_plan_dft_1d(size, out, in, FFTW_BACKWARD, FFTW_ESTIMATE);
    fftw_execute(p);

    for (int i = 0; i < size; i++) {
        output[i] = in[i][0] / size;
    }

    fftw_destroy_plan(p);
    fftw_free(in);
    fftw_free(out);
}
