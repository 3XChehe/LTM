#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>

#define PORT 9090
#define MAX_CLIENTS 64

typedef struct
{
    int socket;
    char nickname[64];
    int is_op;
} ClientType;

ClientType clients[MAX_CLIENTS];
int num_clients = 0;
char room_topic[256] = "";
pthread_mutex_t chat_mutex = PTHREAD_MUTEX_INITIALIZER;

void *client_handler(void *param);

int main()
{
    int server = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind() failed");
        return 1;
    }
    listen(server, 10);

    printf("Chat Server is listening on port%d...\n\n", PORT);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        clients[i].socket = -1;
        clients[i].is_op = 0;
        memset(clients[i].nickname, 0, sizeof(clients[i].nickname));
    }

    while (1)
    {
        int client_sock = accept(server, NULL, NULL);

        pthread_mutex_lock(&chat_mutex);
        int idx = -1;
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i].socket == -1)
            {
                idx = i;
                break;
            }
        }

        if (idx != -1)
        {
            clients[idx].socket = client_sock;
            clients[idx].is_op = 0;
            memset(clients[idx].nickname, 0, sizeof(clients[idx].nickname));
            num_clients++;
            pthread_mutex_unlock(&chat_mutex);

            int *p_idx = malloc(sizeof(int));
            *p_idx = idx;
            pthread_t tid;
            pthread_create(&tid, NULL, client_handler, p_idx);
            pthread_detach(tid);
        }
        else
        {
            char *err = "999 ROOM FULL\n";
            send(client_sock, err, strlen(err), 0);
            close(client_sock);
            pthread_mutex_unlock(&chat_mutex);
        }
    }

    close(server);
    return 0;
}

void *client_handler(void *param)
{
    int my_idx = *(int *)param;
    free(param);

    int my_sock = clients[my_idx].socket;
    char buf[4096];

    while (1)
    {
        memset(buf, 0, sizeof(buf));
        int ret = recv(my_sock, buf, sizeof(buf) - 1, 0);
        if (ret <= 0)
            break;

        buf[ret] = 0;
        // Remove line endings (\r\n or \n)
        buf[strcspn(buf, "\r\n")] = 0;
        if (strlen(buf) == 0)
            continue;

        char cmd[16] = {0};
        sscanf(buf, "%s", cmd);

        // =====================================================================
        // LỆNH JOIN <NICKNAME>
        // =====================================================================
        if (strcmp(cmd, "JOIN") == 0)
        {
            char nick[128] = {0};
            if (sscanf(buf, "JOIN %s", nick) != 1)
            {
                send(my_sock, "201 INVALID NICK NAME\n", 22, 0); /* cite: 26 */
                continue;
            }

            int valid = 1;
            for (int i = 0; nick[i] != '\0'; i++)
            {
                if (!isalnum(nick[i]) || isupper(nick[i]))
                {
                    valid = 0;
                    break;
                }
            }

            pthread_mutex_lock(&chat_mutex);
            int in_use = 0;
            int total_active = 0;
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (clients[i].socket != -1)
                {
                    if (strlen(clients[i].nickname) > 0)
                        total_active++;
                    if (strcmp(clients[i].nickname, nick) == 0)
                        in_use = 1;
                }
            }

            if (!valid)
            {
                send(my_sock, "201 INVALID NICK NAME\n", 22, 0); /* cite: 26 */
            }
            else if (in_use)
            {
                send(my_sock, "200 NICKNAME IN USE\n", 20, 0); /* cite: 25 */
            }
            else
            {
                strcpy(clients[my_idx].nickname, nick);
                if (total_active == 0)
                {
                    clients[my_idx].is_op = 1; // Người đầu tiên là chủ phòng [cite: 97, 99]
                }

                // Gửi phản hồi duy nhất cho client thực hiện lệnh [cite: 24]
                send(my_sock, "100 OK\n", 7, 0); /* cite: 24 */

                // Phát tín hiệu thông báo cho toàn bộ thành viên khác
                char notify[256];
                snprintf(notify, sizeof(notify), "JOIN %s\n", nick); /* cite: 91 */
                for (int i = 0; i < MAX_CLIENTS; i++)
                {
                    if (clients[i].socket != -1 && i != my_idx && strlen(clients[i].nickname) > 0)
                    {
                        send(clients[i].socket, notify, strlen(notify), 0); /* cite: 12 */
                    }
                }
            }
            pthread_mutex_unlock(&chat_mutex);
        }

        else if (strlen(clients[my_idx].nickname) == 0)
        {
            send(my_sock, "999 PLEASE JOIN FIRST\n", 22, 0);
            continue;
        }

        // =====================================================================
        // LỆNH MSG <ROOM MESSAGE>
        // =====================================================================
        else if (strcmp(cmd, "MSG") == 0)
        {
            char *msg_content = buf + 3;
            while (*msg_content == ' ')
                msg_content++; /* cite: 32 */

            send(my_sock, "100 OK\n", 7, 0); /* cite: 34 */

            char broadcast[4500];
            snprintf(broadcast, sizeof(broadcast), "MSG %s %s\n", clients[my_idx].nickname, msg_content); /* cite: 91 */

            pthread_mutex_lock(&chat_mutex);
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (clients[i].socket != -1 && i != my_idx && strlen(clients[i].nickname) > 0)
                {
                    send(clients[i].socket, broadcast, strlen(broadcast), 0); /* cite: 12 */
                }
            }
            pthread_mutex_unlock(&chat_mutex);
        }

        // =====================================================================
        // LỆNH PMSG <NICKNAME> <MESSAGE>
        // =====================================================================
        else if (strcmp(cmd, "PMSG") == 0)
        {
            char target_nick[64] = {0};
            sscanf(buf, "PMSG %s", target_nick);

            char *msg_content = strstr(buf, target_nick);
            if (msg_content != NULL)
            {
                msg_content += strlen(target_nick);
                while (*msg_content == ' ')
                    msg_content++;
            }

            pthread_mutex_lock(&chat_mutex);
            int target_sock = -1;
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (clients[i].socket != -1 && strcmp(clients[i].nickname, target_nick) == 0)
                {
                    target_sock = clients[i].socket;
                    break;
                }
            }

            if (target_sock == -1)
            {
                send(my_sock, "202 UNKNOWN NICKNAME\n", 21, 0); /* cite: 44 */
            }
            else
            {
                send(my_sock, "100 OK\n", 7, 0); /* cite: 43 */

                char private_msg[4500];
                snprintf(private_msg, sizeof(private_msg), "PMSG %s %s\n", clients[my_idx].nickname, msg_content); /* cite: 91 */
                send(target_sock, private_msg, strlen(private_msg), 0);                                            /* cite: 12 */
            }
            pthread_mutex_unlock(&chat_mutex);
        }

        // =====================================================================
        // LỆNH OP <NICKNAME>
        // =====================================================================
        else if (strcmp(cmd, "OP") == 0)
        {
            char target_nick[64] = {0};
            sscanf(buf, "OP %s", target_nick);

            if (!clients[my_idx].is_op)
            {
                send(my_sock, "203 DENIED\n", 11, 0); /* cite: 55 */
                continue;
            }

            pthread_mutex_lock(&chat_mutex);
            int target_idx = -1;
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (clients[i].socket != -1 && strcmp(clients[i].nickname, target_nick) == 0)
                {
                    target_idx = i;
                    break;
                }
            }

            if (target_idx == -1)
            {
                send(my_sock, "202 UNKNOWN NICKNAME\n", 21, 0); /* cite: 54 */
            }
            else
            {
                clients[my_idx].is_op = 0;
                clients[target_idx].is_op = 1;

                // CHÚ Ý TEST SCRIPT: Chỉ gửi 100 OK về cho người ra lệnh, không kèm text "OP ..."
                send(my_sock, "100 OK\n", 7, 0); /* cite: 53 */

                // Phát thông báo chuyển quyền cho các client khác (Không gửi lại cho người gửi) /* cite: 12, 91 */
                char notify[256];
                snprintf(notify, sizeof(notify), "OP %s\n", target_nick); /* cite: 91 */
                for (int i = 0; i < MAX_CLIENTS; i++)
                {
                    if (clients[i].socket != -1 && i != my_idx && strlen(clients[i].nickname) > 0)
                    {
                        send(clients[i].socket, notify, strlen(notify), 0); /* cite: 12 */
                    }
                }
            }
            pthread_mutex_unlock(&chat_mutex);
        }

        // =====================================================================
        // LỆNH KICK <NICKNAME>
        // =====================================================================
        else if (strcmp(cmd, "KICK") == 0)
        {
            char target_nick[64] = {0};
            sscanf(buf, "KICK %s", target_nick);

            if (!clients[my_idx].is_op)
            {
                send(my_sock, "203 DENIED\n", 11, 0); /* cite: 67 */
                continue;
            }

            pthread_mutex_lock(&chat_mutex);
            int target_idx = -1;
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (clients[i].socket != -1 && strcmp(clients[i].nickname, target_nick) == 0)
                {
                    target_idx = i;
                    break;
                }
            }

            if (target_idx == -1)
            {
                send(my_sock, "202 UNKNOWN NICKNAME\n", 21, 0); /* cite: 66 */
                pthread_mutex_unlock(&chat_mutex);
            }
            else
            {
                send(my_sock, "100 OK\n", 7, 0); /* cite: 65 */

                // SỬA LỖI TESTCASE: Thầy cô yêu cầu chuỗi quảng bá chung
                char notify[256];
                snprintf(notify, sizeof(notify), "KICK %s %s\n", target_nick, clients[my_idx].nickname); /* cite: 91 */

                int kicked_sock = clients[target_idx].socket;

                // Giải phóng bộ nhớ client bị kick ngay lập tức
                clients[target_idx].socket = -1;
                memset(clients[target_idx].nickname, 0, sizeof(clients[target_idx].nickname));
                clients[target_idx].is_op = 0;
                num_clients--;

                // Gửi thông báo KICK cho tất cả mọi người còn lại (Trừ người ra lệnh) /* cite: 12 */
                for (int i = 0; i < MAX_CLIENTS; i++)
                {
                    if (clients[i].socket != -1 && i != my_idx && strlen(clients[i].nickname) > 0)
                    {
                        send(clients[i].socket, notify, strlen(notify), 0); /* cite: 12 */
                    }
                }
                pthread_mutex_unlock(&chat_mutex);

                // SỬA LỖI: Gửi thông điệp thông báo KICK chuẩn cho nạn nhân rồi mới đóng ngắt socket
                send(kicked_sock, notify, strlen(notify), 0); /* cite: 91 */
                close(kicked_sock);
            }
        }

        // =====================================================================
        // LỆNH TOPIC <TOPIC NAME>
        // =====================================================================
        else if (strcmp(cmd, "TOPIC") == 0)
        {
            char *topic_content = buf + 5;
            while (*topic_content == ' ')
                topic_content++; /* cite: 74 */

            if (!clients[my_idx].is_op)
            {
                send(my_sock, "203 DENIED\n", 11, 0); /* cite: 78 */
                continue;
            }

            pthread_mutex_lock(&chat_mutex);
            strncpy(room_topic, topic_content, sizeof(room_topic) - 1);

            send(my_sock, "100 OK\n", 7, 0); /* cite: 77 */

            char notify[512];
            snprintf(notify, sizeof(notify), "TOPIC %s %s\n", clients[my_idx].nickname, room_topic); /* cite: 91 */
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (clients[i].socket != -1 && i != my_idx && strlen(clients[i].nickname) > 0)
                {
                    send(clients[i].socket, notify, strlen(notify), 0); /* cite: 12 */
                }
            }
            pthread_mutex_unlock(&chat_mutex);
        }

        // =====================================================================
        // LỆNH QUIT
        // =====================================================================
        else if (strcmp(cmd, "QUIT") == 0)
        {
            send(my_sock, "100 OK\n", 7, 0); /* cite: 86 */
            break;
        }

        else
        {
            send(my_sock, "999 UNKNOWN ERROR\n", 18, 0); /* cite: 27 */
        }
    }

    // =====================================================================
    // XỬ LÝ NGẮT KẾT NỐI KHI CLIENT THOÁT KHỎI PHÒNG
    // =====================================================================
    pthread_mutex_lock(&chat_mutex);
    if (strlen(clients[my_idx].nickname) > 0)
    {
        char notify[256];
        snprintf(notify, sizeof(notify), "QUIT %s\n", clients[my_idx].nickname); /* cite: 91 */

        int was_op = clients[my_idx].is_op;

        clients[my_idx].socket = -1;
        memset(clients[my_idx].nickname, 0, sizeof(clients[my_idx].nickname));
        clients[my_idx].is_op = 0;
        num_clients--;

        // Tự động chuyển quyền chủ phòng (OP) cho người tiếp theo nếu người out là OP [cite: 99]
        if (was_op && num_clients > 0)
        {
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (clients[i].socket != -1 && strlen(clients[i].nickname) > 0)
                {
                    clients[i].is_op = 1;

                    char op_notify[256];
                    snprintf(op_notify, sizeof(op_notify), "OP %s\n", clients[i].nickname); /* cite: 91 */
                    for (int j = 0; j < MAX_CLIENTS; j++)
                    {
                        if (clients[j].socket != -1 && strlen(clients[j].nickname) > 0)
                        {
                            send(clients[j].socket, op_notify, strlen(op_notify), 0); /* cite: 12 */
                        }
                    }
                    break;
                }
            }
        }

        // Gửi thông báo QUIT cho những người còn lại trong phòng [cite: 12]
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i].socket != -1 && strlen(clients[i].nickname) > 0)
            {
                send(clients[i].socket, notify, strlen(notify), 0); /* cite: 12 */
            }
        }
    }
    else
    {
        clients[my_idx].socket = -1;
    }
    pthread_mutex_unlock(&chat_mutex);

    close(my_sock);
    return NULL;
}