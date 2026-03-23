#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

typedef struct
{
    char mssv[20];
    char hoten[50];
    char ngaysinh[20];
    float gpa;
} Student;

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage: %s <server_ip> <port>\n", argv[0]);
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[2]));
    server.sin_addr.s_addr = inet_addr(argv[1]);

    if (connect(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("connect");
        return 1;
    }

    Student sv;

    printf("MSSV: ");
    fgets(sv.mssv, sizeof(sv.mssv), stdin);
    sv.mssv[strcspn(sv.mssv, "\n")] = 0;

    printf("Ho ten: ");
    fgets(sv.hoten, sizeof(sv.hoten), stdin);
    sv.hoten[strcspn(sv.hoten, "\n")] = 0;

    printf("Ngay sinh (YYYY-MM-DD): ");
    fgets(sv.ngaysinh, sizeof(sv.ngaysinh), stdin);
    sv.ngaysinh[strcspn(sv.ngaysinh, "\n")] = 0;

    printf("GPA: ");
    scanf("%f", &sv.gpa);

    send(sockfd, &sv, sizeof(sv), 0);

    close(sockfd);
}