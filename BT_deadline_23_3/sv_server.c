#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

typedef struct
{
    char mssv[20];
    char hoten[50];
    char ngaysinh[20];
    float gpa;
} Student;

int main(int argc, char *argv[])
{
    int port = atoi(argv[1]);
    char *logfile = argv[2];

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server, client;
    socklen_t client_len = sizeof(client);

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;

    bind(sockfd, (struct sockaddr *)&server, sizeof(server));
    listen(sockfd, 5);

    printf("Server listening on port %d...\n", port);

    while (1)
    {
        int clientfd = accept(sockfd, (struct sockaddr *)&client, &client_len);

        Student sv;
        recv(clientfd, &sv, sizeof(sv), 0);

        // Lấy thời gian
        time_t now = time(NULL);
        struct tm *t = localtime(&now);

        char timestr[64];
        strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", t);

        char *ip = inet_ntoa(client.sin_addr);

        // In ra màn hình
        printf("%s %s %s %s %s %.2f\n", ip, timestr,
               sv.mssv, sv.hoten, sv.ngaysinh, sv.gpa);

        // Ghi file
        FILE *f = fopen(logfile, "a");
        if (f != NULL)
        {
            fprintf(f, "%s %s %s %s %s %.2f\n",
                    ip, timestr,
                    sv.mssv, sv.hoten, sv.ngaysinh, sv.gpa);
            fclose(f);
        }

        close(clientfd);
    }

    close(sockfd);
}