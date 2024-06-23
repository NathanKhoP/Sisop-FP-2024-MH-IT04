#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <crypt.h> // bcrypt func
#include <stdbool.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <arpa/inet.h>

#define MAX_LEN 1024
#define MAX_USER 100
#define PORT 8080

int sock = 0;

void cmd_func(const char *cmd, char *usr, char *ch, char *room) {
    if (send(sock, cmd, strlen(cmd), 0) < 0) perror("Command send failed");

    char resp[MAX_LEN];
    memset(resp, 0, sizeof(resp));

    int n = recv(sock, resp, MAX_LEN - 1, 0);
    if (n < 0) {
        perror("Response receive failed");
        return;
    }
    else {
        n = 0;
        if (strstr(resp, "Key:") != NULL) {
            char key[MAX_LEN];
            printf("Key: ");
            fgets(key, MAX_LEN, stdin);
            key[strcspn(key, "\n")] = 0;
            if (send(sock, key, strlen(key), 0) < 0) perror("Key send failed");

            memset(resp, 0, sizeof(resp));
            if (recv(sock, resp, MAX_LEN - 1, 0) < 0) perror("Response receive failed");
            else {
                if (strstr(resp, "Wrong key") != NULL) {
                    if (strlen(room) > 0) room[0] = '\0';
                    else if (strlen(ch) > 0) ch[0] = '\0';
                }
            }
        }
        else if (strstr(resp, "doesn't exist") != NULL || strstr(resp, "Wrong Key") != NULL || strstr(resp, "You have been banned") != NULL) {
            if (strlen(room) > 0) room[0] = '\0';
            else if (strlen(ch) > 0) ch[0] = '\0';
            printf("%s\n", resp);
        }
        else if (strstr(cmd, "JOIN") != NULL, strstr(resp, "Message sent successfully") != NULL || strstr(resp, "edited") != NULL || strstr(resp, "deleted") != NULL || strstr(cmd, "EXIT") != NULL) n++;
        else if (strstr(resp, "exited the program") != NULL) {
            close(sock);
            exit(0);
        }
        else {
            printf("%s\n", resp);
        }
    }
}

void print_output(const char *usr, const char *channel, const char *room) {
    if (strlen(room) > 0) printf("\r[%s/%s/%s] ", usr, channel, room);
    else if (strlen(channel) > 0) printf("\r[%s/%s] ", usr, channel);
    else printf("[%s] ", usr);
}

int main(int argc, char *argv[]) {
    // server connection start
    struct sockaddr_in serv_addr;
    int num1 = 10, num2 = 20, result;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }
    // server connection end

    char cmd[MAX_LEN];
    char usr[MAX_USER], channel[MAX_USER] = "", room[MAX_USER] = "";
    memset(cmd, 0, sizeof(cmd));
    
    if (argc < 2) {
        printf("Usage: %s <command>\n", argv[0]);
        return 1;
    }
    else if (strcmp(argv[1], "REGISTER") == 0) {
        if (strcmp(argv[3], "-p") == 0) {
            sprintf(usr, "%s", argv[2]);
            char *pass = argv[4];
            sprintf(cmd, "REGISTER %s %s", usr, pass);
        }
        else {
            printf("Usage: %s REGISTER <username> -p <password>\n", argv[0]);
            return 1;
        }
    }
    else if (strcmp(argv[1], "LOGIN") == 0) {
        if (strcmp(argv[3], "-p") == 0) {
            sprintf(usr, "%s", argv[2]);
            char *pass = argv[4];
            sprintf(cmd, "LOGIN %s %s", usr, pass);
            if (send(sock, cmd, strlen(cmd), 0) < 0) perror("Command send failed");
            char resp[MAX_LEN];
            memset(resp, 0, sizeof(resp));
            int n = recv(sock, resp, MAX_LEN - 1, 0);
            if (n < 0) perror("Response receive failed");
            else {
                printf("%s\n", resp);
                if (strstr(resp, "logged in") != NULL) {
                    while (1) {
                        print_output(usr, channel, room);
                        
                        if (fgets(cmd, MAX_LEN, stdin) == NULL) continue;
                        cmd[strcspn(cmd, "\n")] = 0;
                        
                        if (strncmp(cmd, "JOIN ", 5) == 0) {
                            if (strlen(channel) == 0) sprintf(channel, "%s", cmd + 5);
                            else sprintf(room, "%s", cmd + 5);
                        }
                        else if (strcmp(cmd, "EXIT") == 0) {
                            if (strlen(room) > 0) room[0] = '\0';
                            else if (strlen(channel) > 0) channel[0] = '\0';
                        }
                        else if (strncmp(cmd, "EDIT PROFILE SELF -u", 20) == 0) {
                            sprintf(usr, "%s", cmd + 21);
                        }
                        
                        cmd_func(cmd, usr, channel, room);
                    }
                }
            }
        }
        else {
            printf("Usage: %s LOGIN <username> -p <password>\n", argv[0]);
            return 1;
        }
    }
    else {
        printf("Invalid command\n");
        return 1;
    }
    cmd_func(cmd, usr, channel, room);
    close(sock);
    return 0;
}