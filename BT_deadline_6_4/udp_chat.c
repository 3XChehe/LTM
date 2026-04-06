#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <errno.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Usage: %s port_s ip_d port_d\n", argv[0]);
        return 1;
    }

    int port_s = atoi(argv[1]);
    char *ip_d = argv[2];
    int port_d = atoi(argv[3]);

    int sockfd;
    struct sockaddr_in local_addr, dest_addr;
    char buffer[BUFFER_SIZE];

    // Tạo socket UDP
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        exit(1);
    }

    unsigned long ul = 1;
    ioctl(sockfd, FIONBIO, &ul);
    unsigned long ul2 = 1;
    ioctl(STDIN_FILENO, FIONBIO, &ul2);
    // Bind cổng nhận (port_s)
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(port_s);

    if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0)
    {
        perror("bind");
        exit(1);
    }

    // Thiết lập địa chỉ đích
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port_d);
    inet_pton(AF_INET, ip_d, &dest_addr.sin_addr);

    printf("UDP Chat started on port %d\n", port_s);
    char buf[256];
    while (1)
    {
        int len = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, NULL, NULL);

        if (len > 0)
        {
            buffer[len] = '\0';
            printf("\n[RECV]: %s", buffer);
            fflush(stdout);
        }

        if (fgets(buffer, BUFFER_SIZE, stdin) != NULL)
        {
            sendto(sockfd, buffer, strlen(buffer), 0,
                   (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        }
    }

    close(sockfd);
    return 0;
}