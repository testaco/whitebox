#include "CMSIS/a2fxxxm3.h"

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
#define BUFLEN 736
#define PORT 7653

#include "TOPLEVEL_hw_platform.h"
#include "drivers/CoreGPIO/core_gpio.h"
#include "drivers/mss_pdma/mss_pdma.h"

void err(char* str) {
    perror(str);
    exit(1);
}

int main(void) {
    struct sockaddr_in my_addr, cli_addr;
    int sockfd, i;
    socklen_t slen=sizeof(cli_addr);
    int16_t buf[BUFLEN];

    PDMA_init();

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        err("socket");
    } else {
        printf("Server: socket() successful\n");
    }

    bzero(&my_addr, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(PORT);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr*) &my_addr, sizeof(my_addr)) == -1) {
        err("bind");
    } else {
        printf("Server: bind() successful\n");
    }

    // Setup DMA
    PDMA_configure(PDMA_CHANNEL_0,
        PDMA_MEM_TO_MEM,
        PDMA_LOW_PRIORITY | PDMA_HALFWORD_TRANSFER | PDMA_INC_SRC_TWO_BYTES,
        PDMA_DEFAULT_WRITE_ADJ);

    while(1) {
        if (recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr*) &cli_addr, &slen) == -1) {
            err("recvfrom");
        } else {
            //int i;
            //for (i = 0; i < BUFLEN/sizeof(int16_t); i+=2)
            //    printf("%d %d\n", *(buf+i), *(buf+i+1));
            //printf("\n\n");
            PDMA_start(PDMA_CHANNEL_0, (uint32_t)buf, RADIO_0, BUFLEN);
            uint32_t status;
            do {
                status = PDMA_status(PDMA_CHANNEL_0);
            } while(status == 0);
        }
    }

    close(sockfd);
    return 0;
}
