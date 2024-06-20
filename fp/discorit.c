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
    resp[n] = '\0';
    printf("%s\n", resp);
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
        exit(EXIT_FAILURE);
    }
    else if (strcmp(argv[1], "REGISTER") == 0) {
        if (strcmp(argv[3], "-p") == 0) {
            sprintf(usr, "%s", argv[2]);
            char *pass = argv[4];
            sprintf(cmd, "REGISTER %s %s", usr, pass);
        }
        else {
            printf("Usage: %s REGISTER <username> -p <password>\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(argv[1], "LOGIN") == 0) {
        if (strcmp(argv[3], "-p") == 0) {
            sprintf(usr, "%s", argv[2]);
            char *pass = argv[4];
            sprintf(cmd, "LOGIN %s %s", usr, pass);
        }
        else {
            printf("Usage: %s LOGIN <username> -p <password>\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    else {
        printf("Invalid command\n");
        exit(EXIT_FAILURE);
    }

    cmd_func(cmd, usr, channel, room);
    close(sock);
    return 0;
}