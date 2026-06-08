#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

void *client_handler(void *arg)
{
    int client = *(int *)arg;
    free(arg);

    char buf[4096];
    int ret = recv(client, buf, sizeof(buf) - 1, 0);
    if (ret <= 0)
    {
        close(client);
        pthread_exit(NULL);
    }
    buf[ret] = 0;

    char method[16], path[2048];
    sscanf(buf, "%s %s", method, path);

    float a = 0, b = 0, result = 0;
    char op[16] = "";
    char op_symbol = '?';
    int has_calc = 0;
    char err_msg[128] = "";

    char *params = NULL;
    if (strcmp(method, "GET") == 0)
    {
        char *q = strchr(path, '?');
        if (q)
            params = q + 1;
    }
    else if (strcmp(method, "POST") == 0)
    {
        char *body = strstr(buf, "\r\n\r\n");
        if (body)
            params = body + 4;
    }

    if (params && strlen(params) > 0)
    {
        if (sscanf(params, "a=%f&op=%15[^&]&b=%f", &a, op, &b) == 3)
        {
            has_calc = 1;
            if (strcmp(op, "add") == 0)
            {
                result = a + b;
                op_symbol = '+';
            }
            else if (strcmp(op, "sub") == 0)
            {
                result = a - b;
                op_symbol = '-';
            }
            else if (strcmp(op, "mul") == 0)
            {
                result = a * b;
                op_symbol = '*';
            }
            else if (strcmp(op, "div") == 0)
            {
                if (b != 0)
                {
                    result = a / b;
                    op_symbol = '/';
                }
                else
                {
                    strcpy(err_msg, "Loi: Khong the chia cho 0!");
                    has_calc = 0;
                }
            }
        }
    }

    char res_body[4096];
    char html_head[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
                       "<html><head><title>Web Calculator</title></head>"
                       "<body style='font-family: Arial; padding: 20px;'>"
                       "<h2>Calculator</h2>";

    char html_form[] = "<hr>"
                       "<h3>dung GET</h3>"
                       "<form method='GET' action='/'>"
                       "<input type='number' step='any' name='a' required> "
                       "<select name='op'>"
                       "<option value='add'>+</option><option value='sub'>-</option>"
                       "<option value='mul'>*</option><option value='div'>/</option>"
                       "</select> "
                       "<input type='number' step='any' name='b' required> "
                       "<button type='submit'>Tinh bang GET</button>"
                       "</form>"

                       "<h3>dung POST</h3>"
                       "<form method='POST' action='/'>"
                       "<input type='number' step='any' name='a' required> "
                       "<select name='op'>"
                       "<option value='add'>+</option><option value='sub'>-</option>"
                       "<option value='mul'>*</option><option value='div'>/</option>"
                       "</select> "
                       "<input type='number' step='any' name='b' required> "
                       "<button type='submit'>Tinh bang POST</button>"
                       "</form></body></html>";

    strcpy(res_body, html_head);

    if (has_calc)
    {
        char result_str[256];
        sprintf(result_str, "<h3 style='color:blue;'>Ket qua (Phuong thuc %s): %.2f %c %.2f = %.2f</h3>", method, a, op_symbol, b, result);
        strcat(res_body, result_str);
    }
    else if (strlen(err_msg) > 0)
    {
        char err_str[256];
        sprintf(err_str, "<h3 style='color:red;'>%s</h3>", err_msg);
        strcat(res_body, err_str);
    }

    strcat(res_body, html_form);

    send(client, res_body, strlen(res_body), 0);
    close(client);
    pthread_exit(NULL);
}

int main()
{
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

    bind(listener, (struct sockaddr *)&addr, sizeof(addr));
    listen(listener, 10);

    printf("Calculator Server dang chay tren cong 8080...\n");
    printf("Mo trinh duyet Web va truy cap: http://127.0.0.1:8080\n");

    while (1)
    {
        int client = accept(listener, NULL, NULL);
        if (client == -1)
            continue;

        int *new_sock = malloc(sizeof(int));
        *new_sock = client;

        pthread_t tid;
        pthread_create(&tid, NULL, client_handler, new_sock);
        pthread_detach(tid);
    }

    close(listener);
    return 0;
}