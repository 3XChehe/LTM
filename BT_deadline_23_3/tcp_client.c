#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <IP> <PORT>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_ip = argv[1];
    int port = atoi(argv[2]);

    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    // Tạo socket
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    // Thiết lập địa chỉ server
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(port); // port là tham số từ argv[2]
    // Kết nối tới server
    int ret = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0)
    {
        perror("connect() failed");
    }
    printf("Connected to %s:%d\n", server_ip, port);
    // nhận phản hồi từ server
    int n = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
    if (n < 0)
    {
        perror("recv");
    }
    buffer[n] = '\0';
    printf("Received from server: %s\n", buffer);
    memset(buffer, 0, strlen(buffer));
    while (1)
    {
        printf("Enter message: ");
        memset(buffer, 0, strlen(buffer));
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL)
            break;
        // gửi dữ liệu
        if (send(sockfd, buffer, strlen(buffer), 0) < 0)
        {
            perror("send");
            break;
        }
    }
    close(sockfd);
}