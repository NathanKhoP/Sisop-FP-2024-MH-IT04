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
#define MAX_USER 100
#define path "/home/etern1ty/sisop_works/FP/fp/DiscorIT"
#define path_user "/home/etern1ty/sisop_works/FP/fp/DiscorIT/users.csv"
#define path_channels "/home/etern1ty/sisop_works/FP/fp/DiscorIT/channels.csv"

#define DEBUG

typedef struct {
    int sock;
    struct sockaddr_in addr;
    char userLogged[MAX_USER];
    char channelLogged[MAX_USER];
    char roleLogged[MAX_USER];
    char roomLogged[MAX_USER];
} connection_t;

void register_func(char username[MAX_LEN], char password[MAX_LEN], connection_t *conn) {
    if (username == NULL || password == NULL) {
        char resp[] = "Username or password cannot be empty\n";
        if (write(conn->sock, resp, strlen(resp)) < 0) {
            perror("Response send failed");
        }
        return;
    }
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
        char resp[100];
        snprintf(resp, sizeof(resp), "%s already registered", username);
        if (write(conn->sock, resp, strlen(resp)) < 0) {
            perror("Response send failed");
        }
        fclose(fp);
        return;
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

    char resp[100];
    snprintf(resp, sizeof(resp), "%s registered", username);
    if (write(conn->sock, resp, strlen(resp)) < 0) {
        perror("Response send failed");
    }
}

void login_func(char username[MAX_LEN], char password[MAX_LEN], connection_t *conn) {
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
        char resp[] = "Error opening file\n";
        if (write(conn->sock, resp, strlen(resp)) < 0) {
            perror("Response send failed");
        }
    }

    while ((read = getline(&line, &len, fp)) != -1) {
        char *token = strtok(line, ",");
        token = strtok(NULL, ",");
        sscanf(line, "%d,%[^,],%[^,],%s", &file_userid, file_username, file_hashed, file_role);
        if (strcmp(username, file_username) == 0 && strcmp(hashed, file_hashed) == 0) {
            sprintf(conn->userLogged, "%s", username);
            token = strtok(file_role, ",");
            sprintf(conn->roleLogged, "%s", token);
            isExist = true;
            break;
        }
    }

    fclose(fp);

    if (!isExist) {
        char resp[] = "Invalid username or password\n";
        if (write(conn->sock, resp, strlen(resp)) < 0) {
            perror("Response send failed");
        }
    }

    char resp[100];
    snprintf(resp, sizeof(resp), "%s logged in", username);
    if (write(conn->sock, resp, strlen(resp)) < 0) {
        perror("Response send failed");
    }
}

void *discorit_handler(void *input) {
    connection_t *conn = (connection_t *)input;
    char buf[MAX_LEN], *resp;
    int n;

    while (n = recv(conn->sock, buf, sizeof(buf), 0) > 0) {
        buf[n] = 0;
        printf("Received: %s\n", buf);

        char *token = strtok(buf, " ");
        if (token == NULL) {
            resp = "Invalid command\n";
            if (send(conn->sock, resp, strlen(resp), 0) != strlen(resp)) {
                perror("Send failed");
                exit(EXIT_FAILURE);
            }
            continue;
        }   
        if (strcmp(token, "REGISTER") == 0) {
            char *usr = strtok(NULL, " ");
            char *pass = strtok(NULL, " ");
            register_func(usr, pass, conn);
        }
        else if (strcmp(token, "LOGIN") == 0) {
            char *usr = strtok(NULL, " ");
            char *pass = strtok(NULL, " ");
            login_func(usr, pass, conn);
        }
        else {
            printf("Invalid command\n");
            exit(EXIT_FAILURE);
        }
    }

    free(conn);
    pthread_exit(0);

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

        // discorit handler thread creation
        pthread_t tid;
        connection_t *connection = (connection_t *)malloc(sizeof(connection_t));
        connection->sock = cli_socket;
        connection->addr = cli_addr;
        memset(connection->userLogged, 0, sizeof(connection->userLogged));
        memset(connection->channelLogged, 0, sizeof(connection->channelLogged));
        memset(connection->roleLogged, 0, sizeof(connection->roleLogged));
        memset(connection->roomLogged, 0, sizeof(connection->roomLogged));
        pthread_create(&tid, NULL, discorit_handler, (void *)connection);
    }
}