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
#define IP "192.168.1.21"

int server;

void make_folder(char *folder_path) {
    struct stat st = {0};
    if (stat(folder_path, &st) == -1) mkdir(folder_path, 0777);
}

void cmd_func(const char *cmd, char *usr, char *ch, char *room) {
    if (send(server, cmd, strlen(cmd), 0) < 0) perror("Command send failed");

    char resp[MAX_LEN];
    memset(resp, 0, sizeof(resp));

    if (recv(server, resp, sizeof(resp), 0) < 0) perror("Response receive failed");
    else {
        if (strstr(resp, "Key:") != NULL) {
            char key[MAX_USER];
            printf("Key: ");
            fgets(key, sizeof(key), stdin);
            key[strcspn(key, "\n")] = 0;

            if (send(server, key, strlen(key), 0) < 0) perror("Key send failed");
            memset(resp, 0, sizeof(resp));
            if (recv(server, resp, sizeof(resp), 0) < 0) perror("Response receive failed");
            else {
            }
        }
    }
}

int main(int argc, char *argv[]) {
    // server connection start
    int sock = 0;
    struct sockaddr_in serv_addr;
    struct hostent *address;
    int num1 = 10, num2 = 20, result;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    if (sock < 0) {
        perror("Error opening socket");
        address = gethostbyname(IP);
        if (address == NULL) {
            fprintf(stderr, "ERROR, no such host\n");
            exit(EXIT_FAILURE);
        }
    }

    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }
    // server connection end

    char cmd[MAX_LEN];
    char username[MAX_USER], channel[MAX_USER], room[MAX_USER];
    memset(cmd, 0, sizeof(cmd));
    
    if (argc < 2) {
        printf("Usage: %s <command>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    else if (strcmp(argv[1], "REGISTER") == 0) {
        if (strcmp(argv[3], "-p") == 0) register_func(argv[2], argv[4]);
        else {
            printf("Usage: %s REGISTER <username> -p <password>\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(argv[1], "LOGIN") == 0) {
        if (strcmp(argv[3], "-p") == 0) login_func(argv[2], argv[4]);
        else {
            printf("Usage: %s REGISTER -p <username> <password>\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    else {
        printf("Invalid command\n");
        exit(EXIT_FAILURE);
    }
    return 0;
}