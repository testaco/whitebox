#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define PORT 7653

void err(char* str) {
    perror(str);
    exit(1);
}

int main(int argc, char** argv) {
    // Configuration
    int c;
    int32_t num_samples = 512;
    int format_json = 0;

    // UDP Socket
    struct sockaddr_in my_addr, cli_addr;
    int sockfd;
    socklen_t slen=sizeof(cli_addr);

    // Runtime
    size_t len, offset, count;
    char* framebuffer;
    int cur_sample;

    while ((c = getopt(argc, argv, "n:f:")) != -1) {
        switch(c) {
            case 'n':
                num_samples = atoi(optarg);
                break;
            case 'f':
                if (strcmp(optarg, "json") == 0) {
                    format_json = 1;
                }
                break;
        }
    }

    fprintf(stderr, "sizeof(short)=%d\n", sizeof(uint16_t));
    fprintf(stderr, "numsamples=%d\n", num_samples);

    len = num_samples * sizeof(int32_t);
    framebuffer = malloc(len);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        err("socket");
    } else {
        fprintf(stderr, "Server: socket() successful\n");
    }

    bzero(&my_addr, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(PORT);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr*) &my_addr, sizeof(my_addr)) == -1) {
        err("bind");
    } else {
        fprintf(stderr, "Server: bind() successful\n");
    }

    offset = 0;
    while(offset < len) {
        if ((count = recvfrom(sockfd, framebuffer + offset, len - offset, 0, (struct sockaddr*) &cli_addr, &slen)) == -1) {
            err("recvfrom");
        } else if (count == 0) {
            err("connection closed");
        } else {
            offset += count;
        }
    }
    close(sockfd);
    fprintf(stderr, "Server: Sample capture successful\n");

    if (format_json)
        printf("[");
    for (offset = 0, cur_sample = 0; cur_sample < num_samples; offset += 2, cur_sample++) {
        int16_t i = ((int16_t*)framebuffer)[offset];
        int16_t q = ((int16_t*)framebuffer)[offset+1];
        if (format_json) {
            printf("[%d,%d]%c%c", i, q,
                                  (cur_sample < num_samples - 1) ? ',' : ' ',
                                  (cur_sample < num_samples - 1) ? '\n' : ' ');
        } else {
            printf("%d,%d%c", i, q,
                              (cur_sample < num_samples - 1) ? '\n' : ' ');
        }
    }
    if (format_json)
        printf("]");

    return 0;
}
