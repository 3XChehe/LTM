#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[])
{
    // tạo server socket
    int sockfd;
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    // thiết lập địa chỉ server
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(atoi(argv[1]));

    char *welcome_file = argv[2];
    char *output_file = argv[3];
    // bind socket với địa chỉ server
    int ret = bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0)
    {
        perror("bind() failed");
        exit(1);
    }
    // lắng nghe kết nối từ client
    ret = listen(sockfd, 5);
    if (ret < 0)
    {
        perror("listen() failed");
        exit(1);
    }
    printf("Server is listening on port %s...\n", argv[1]);
    while (1)
    {
        // chấp nhận kết nối từ client
        struct sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);
        int client_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sockfd < 0)
        {
            perror("accept() failed");
            continue;
        }
        printf("Accepted connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        // gửi file welcome.txt cho client
        FILE *f_welcome = fopen(welcome_file, "r");
        if (f_welcome != NULL)
        {
            char buffer[BUFFER_SIZE];
            int n;
            while ((n = fread(buffer, 1, BUFFER_SIZE, f_welcome)) > 0)
            {
                send(client_sockfd, buffer, n, 0);
            }
            fclose(f_welcome);
        }
        // nhận dữ liệu từ client và lưu vào file output.txt
        FILE *f_output = fopen(output_file, "a");
        if (f_output != NULL)
        {
            char buffer[BUFFER_SIZE];
            int n;
            memset(buffer, 0, strlen(buffer));
            while ((n = recv(client_sockfd, buffer, BUFFER_SIZE, 0)) > 0)
            {
                printf("Received from client: %s\n", buffer);
                fwrite(buffer, 1, n, f_output);
            }

            fclose(f_output);

            close(client_sockfd);
        }
    }
}