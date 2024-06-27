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
#include <crypt.h>

#define PORT 8080
#define MAX_LEN 1024

int sock = 0;

char username[100];
char password[100];
char channel[100] = "";
char room[100] = "";

#define DEBUG

int cmd_handler(const char *buf) {
    if (buf == NULL) return -1;
    if (send(sock, buf, strlen(buf), 0) < 0) {
        perror("Command send failed");
        return -1;
    }

    char resp[MAX_LEN];
    memset(resp, 0, sizeof(resp));
    if (recv(sock, resp, MAX_LEN - 1, 0) < 0) {
        perror("Response receive failed");
        return -1;
    }

    char *selected = strtok(resp, " ");
    char *msg = strtok(NULL, " ");

    if (strcmp(selected, "MSG") == 0) {
        printf("%s", msg);
        return 0;
    }
    else if (strcmp(selected, "CHANNEL") == 0) {
        char *channel_name = strtok(NULL, " ");
        if (channel_name == NULL) return -1;
        strcpy(channel, channel_name);
        printf("%s", msg);
        return 0;
    }
    else if (strcmp(selected, "ROOM") == 0){
        char *room_name = strtok(NULL, ",");
        if (room_name == NULL) return -1;
        strcpy(room, room_name);
        printf("%s\n", msg);
        return 0;
    }

    else if (strcmp(selected, "EXIT") == 0){
        char *exit_selected = strtok(NULL, ",");
        if (exit_selected == NULL) {
            perror("exit selected is empty");
            exit(EXIT_FAILURE);
        }

        if (strcmp(exit_selected, "CHANNEL") == 0) {
            memset(channel, 0, 100);
            memset(room, 0, 100);
            return 0;
        }

        else if (strcmp(exit_selected, "ROOM") == 0) {
            memset(room, 0, 100);
            return 0;
        }
    }

    else if (strcmp(selected, "KEY") == 0){
        handle_command("X X");
        return 0;
    }

    // Quit message
    else if (strcmp(selected, "QUIT") == 0){
        printf("%s\n", msg);
        return 2;
    }
}

int account_handler(const char *buf) {
    if (buf == NULL) {
        perror("Buffer is empty");
        exit(EXIT_FAILURE);
    }
    if (send(sock, buf, strlen(buf), 0) < 0) {
        perror("Send failed");
        exit(EXIT_FAILURE);
    }

    char response[MAX_LEN];
    memset(response, 0, MAX_LEN);
    if (recv(sock, response, MAX_LEN, 0) < 0) {
        perror("recv failed");
        exit(EXIT_FAILURE);
    }

    char *type = strtok(response, ",");
    char *message = strtok(NULL, ",");

    if (strcmp(type, "MSG") == 0){
        printf("%s\n", message);
        return 0;
    }
    else if (strcmp(type, "LOGIN") == 0){
        printf("%s\n", message);
        return 1;
    }
}   

void cmd_parse(char *buf) {
    char *cmd1 = strtok(buf, " ");
    if (cmd1 == NULL) return;

    int exit_ = 0;
    if (strlen(room) > 0) {
        if (strstr(buf, "EXIT") != NULL) exit_ = 1;
    }
    else {
        if (strcmp(cmd1, "EXIT") == 0) exit_ = 1;
    }

    if (exit_ == 0) {
        char *channel_name = strtok(NULL, " ");
        char *cmd2 = strtok(NULL, " ");
        char *room_name = strtok(NULL, " ");

        if (channel_name == NULL || cmd2 == NULL || room_name == NULL) {
            printf("Invalid command\n");
            return;
        }

        if (strcmp(cmd1, "-channel") != 0 && strcmp(cmd1, "-room") != 0) {
            printf("Invalid command\n");
            return;
        }

        char buf1[MAX_LEN];
        memset(buf1, 0, sizeof(buf1));
        sprintf(buf1, "JOIN %s", channel_name);
        char buf2[MAX_LEN];
        memset(buf2, 0, sizeof(buf2));
        sprintf(buf2, "JOIN %s", room_name);

        if (cmd_handler(buf1) == 0) cmd_handler(buf2);
        return;
    }

    if (strlen(room) > 0 || strlen(channel) > 0) {
        while (strlen(room) > 0 || strlen(channel) > 0) cmd_handler("EXIT");
        return; 
    }

    if (handle_command(buf) == 2) {
        close(sock);
        exit(EXIT_SUCCESS);
    }
    return;
}

void *input_handler(void *arg) {
    if (strlen(room) > 0) printf("[%s/%s/%s] ", username, channel, room);
    else if (strlen(channel) > 0) printf("[%s/%s] ", username, channel);
    else printf("[%s] ", username);

    char buf[MAX_LEN];
    memset(buf, 0, sizeof(buf));
    fgets(buf, MAX_LEN, stdin);
    buf[strcspn(buf, "\n")] = 0;

    cmd_parse(buf);
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

    char buf[MAX_LEN];
    memset(buf, 0, sizeof(buf));
    strcpy(username, argv[2]);
    strcpy(password, argv[4]);
    sprintf(buf, "LOGIN %s %s", username, password);

    if (account_handler(buf) != 1) {
        close(sock);
        exit(EXIT_FAILURE);
    }

    while(1) {
        while(strlen(room) > 0 ) {
            printf("\033[H\033[J"); //clear
            cmd_handler("SEE CHAT");

            pthread_t tid;
            pthread_create(&tid, NULL, input_handler, NULL);
            sleep(5);
            pthread_cancel(tid);
        }
        pthread_t tid;
        pthread_create(&tid, NULL, input_handler, NULL);
        pthread_join(tid, NULL);
    }
}