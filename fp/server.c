#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <pthread.h>

#define PORT 8080
#define MAX_LEN 1024
#define path "/home/etern1ty/sisop_works/FP/fp/DiscorIT"
#define path_user "/home/etern1ty/sisop_works/FP/fp/DiscorIT/users.csv"
#define path_channels "/home/etern1ty/sisop_works/FP/fp/DiscorIT/channels.csv"

#define DEBUG

void register_func(char username[MAX_LEN], char password[MAX_LEN]) {
    make_folder(path);
    char salt[MAX_LEN], hashed[MAX_LEN];
    FILE *fp;   
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    bool isExist = false;
    int userid = 1;
    char *role;

    fp = fopen(path_user, "r");
    if (fp == NULL) {
        fopen(path_user, "w");
        fp = fopen(path_user, "r");
    }

    fseek(fp, 0, SEEK_END);
    if (ftell(fp) == 0) role = "ROOT";
    else role = "USER";
    rewind(fp);

    while ((read = getline(&line, &len, fp)) != -1) {
        if (strstr(line, username) != NULL) {
            isExist = true;
            break;
        }
        userid++;
    }

    if (isExist) {
        printf("%s already registered\n", username);
        exit(EXIT_FAILURE);
    }

    snprintf(salt, MAX_LEN, "$2a$10$%.22s", "theusernameandorpasswordsaltcode");
    strcpy(hashed, crypt(password, salt));

    fp = fopen(path_user, "a");
    if (fp == NULL) {
        printf("Error opening file\n");
        exit(EXIT_FAILURE);
    }

    fprintf(fp, "%d,%s,%s,%s\n", userid, username, hashed, role);
    fclose(fp);

    printf("%s registered\n", username);
}

void login_func(char username[MAX_LEN], char password[MAX_LEN]) {
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    bool isExist = false;
    char salt[MAX_LEN], hashed[MAX_LEN], file_hashed[MAX_LEN];
    int file_userid;
    char file_username[MAX_LEN], file_role[MAX_LEN];

    snprintf(salt, MAX_LEN, "$2a$10$%.22s", "theusernameandorpasswordsaltcode");
    strcpy(hashed, crypt(password, salt));

    fp = fopen(path_user, "r");
    if (fp == NULL) {
        printf("Error opening file\n");
        exit(EXIT_FAILURE);
    }

    while ((read = getline(&line, &len, fp)) != -1) {
        sscanf(line, "%d,%[^,],%[^,],%s", &file_userid, file_username, file_hashed, file_role);
        if (strcmp(username, file_username) == 0 && strcmp(hashed, file_hashed) == 0) {
            isExist = true;
            break;
        }
    }

    fclose(fp);

    if (!isExist) {
        printf("Invalid username or password\n");
        exit(EXIT_FAILURE);
    }

    printf("%s logged in\n", username);
}

int main(int argc, char *argv[]) {
    int serv_socket, cli_socket;
    struct sockaddr_in serv_addr, cli_addr;
    int addrlen = sizeof(serv_addr);
    char buffer[MAX_LEN] = {0};
    
    // create socket
    if ((serv_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(serv_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // define server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    // bind socket to server address
    if (bind(serv_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // listen on port
    if (listen(serv_socket, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    // Making the program run as a daemon (Modul)
    pid_t pid, sid;
    pid = fork();
    if (pid < 0) {
        printf("Fork Failed!\n");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    umask(0);
    if (setsid() < 0) {
        perror("Error: setsid() failed");
    }
    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    char cmd[MAX_LEN], stat[MAX_LEN], *resp;
    int mark;

    while (1) {
        // accept incoming connection
        if ((cli_socket = accept(serv_socket, (struct sockaddr *)&serv_addr, (socklen_t *)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        #ifdef DEBUG
            printf("Connection established - [%s] on port [%d]\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
        #endif

        // receive command from client
        memset(cmd, 0, sizeof(cmd));
        if (recv(cli_socket, cmd, MAX_LEN, 0) < 0) {
            perror("Receive failed");
            exit(EXIT_FAILURE);
        }

        // discorit handler
    }
}