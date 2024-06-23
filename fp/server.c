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
#define BUFFER_SIZE 10240
// #define path "/home/etern1ty/sisop_works/FP/fp/DiscorIT"
// #define path_user "/home/etern1ty/sisop_works/FP/fp/DiscorIT/users.csv"
// #define path_channels "/home/etern1ty/sisop_works/FP/fp/DiscorIT/channels.csv"
#define path "/Users/macbook/Kuliah/Sistem Operasi/fp-sisop/fp/DiscorIT"
#define path_user "/Users/macbook/Kuliah/Sistem Operasi/fp-sisop/fp/DiscorIT/users.csv"
#define path_channels "/Users/macbook/Kuliah/Sistem Operasi/fp-sisop/fp/DiscorIT/channels.csv"

#define DEBUG

typedef struct {
    int sock;
    struct sockaddr_in addr;
    char userLogged[MAX_USER];
    char channelLogged[MAX_USER];
    char roleLogged[MAX_USER];
    char roomLogged[MAX_USER];
    } connection_t;

void make_folder(char* folder_path) {
    struct stat st = { 0 };
    if (stat(folder_path, &st) == -1) mkdir(folder_path, 0777);
    }

void register_func(char username[MAX_LEN], char password[MAX_LEN], connection_t* conn) {
    if (username == NULL || password == NULL) {
        char resp[] = "Username or password cannot be empty\n";
        if (write(conn->sock, resp, strlen(resp)) < 0) {
            perror("Response send failed");
            }
        fsync(conn->sock);
        return;
        }
    make_folder(path);
    char salt[MAX_LEN], hashed[MAX_LEN];
    FILE* fp;
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    bool isExist = false;
    int userid = 1;
    char* role;

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
        if (strstr(line, username) != NULL) { // strstr has an issue with usernames that are a substring of another username, but it's fine for now
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
        fsync(conn->sock);
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
    printf("%s\n", resp);
    if (write(conn->sock, resp, strlen(resp)) < 0) {
        perror("Response send failed");
        }
    fsync(conn->sock);
    }

void login_func(char username[MAX_LEN], char password[MAX_LEN], connection_t* conn) {
    FILE* fp;
    char* line = NULL;
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
        fsync(conn->sock);
        }

    while ((read = getline(&line, &len, fp)) != -1) {
        char* token = strtok(line, ",");
        token = strtok(NULL, ",");
        if (strcmp(token, username) != 0) continue;

        isExist = true;
        token = strtok(NULL, ",");
        sprintf(file_hashed, "%s", token);

        if (strcmp(hashed, file_hashed) == 0) {
            sprintf(conn->userLogged, "%s", username);
            token = strtok(file_role, ",");
            sprintf(conn->roleLogged, "%s", token);
            }
        else {
            char resp[] = "Invalid password\n";
            if (write(conn->sock, resp, strlen(resp)) < 0) {
                perror("Response send failed");
                }
            fsync(conn->sock);
            }
        }

    fclose(fp);

    if (!isExist) {
        char resp[] = "Invalid username\n";
        if (write(conn->sock, resp, strlen(resp)) < 0) {
            perror("Response send failed");
            }
        fsync(conn->sock);
        }

    else {
        char resp[100];
        sprintf(resp, "%s logged in", username);
        printf("%s\n", resp);
        if (write(conn->sock, resp, strlen(resp)) < 0) {
            perror("Response send failed");
            }
        fsync(conn->sock);
        }
    }

// ====================================================================================================
// ====================================================================================================
// TODO: Implement the following functions
// ====================================================================================================
// ====================================================================================================

void new_channel(const char* username, const char* channel_name, const char* key, connection_t* conn) {
    FILE* fp = fopen(path_channels, "a");
    if (fp == NULL) {
        perror("Error opening file");
        return;
        }

    char line[MAX_LEN];
    bool chanExist = false;
    int count = 0;

    while (fgets(line, sizeof(line), fp)) {
        char* token = strtok(line, ",");
        token = strtok(NULL, ",");
        if (strcmp(token, channel_name) == 0) {
            chanExist = true;
            break;
            }
        count++;
        }

    if (chanExist) {
        char resp[100];
        sprintf(resp, "%s already exists", channel_name);
        if (write(conn->sock, resp, strlen(resp)) < 0) {
            perror("Response send failed");
            }
        fsync(conn->sock);
        fclose(fp);
        return;
        }

    fseek(fp, 0, SEEK_END);

    char salt[MAX_LEN], hashed[MAX_LEN];
    sprintf(salt, "$2a$10$%.22s", "thechannelbcryptsaltstringcode");
    strcpy(hashed, crypt(key, salt));

    fprintf(fp, "%d,%s,%s\n", count + 1, channel_name, hashed);
    fclose(fp);

    char ch_path[512];
    sprintf(ch_path, "%s/%s", path, channel_name);
    make_folder(ch_path);

    sprintf(ch_path, "%s/%s/admin", path, channel_name);
    make_folder(ch_path);

    sprintf(ch_path, "%s/%s/admin/auth.csv", path, channel_name);
    FILE* auth = fopen(ch_path, "w+");
    if (auth == NULL) {
        char resp[] = "Error opening/writing auth.csv\n";
        if (write(conn->sock, resp, strlen(resp)) < 0) {
            perror("Response send failed");
            }
        }
    else {
        char user_id[10];
        FILE* user = fopen(path_user, "r");
        if (user == NULL) {
            char resp[] = "Error opening users.csv\n";
            if (write(conn->sock, resp, strlen(resp)) < 0) {
                perror("Response send failed");
                }
            fclose(auth);
            return;
            }

        char line_user[MAX_LEN];
        bool userExist = false;

        while (fgets(line_user, sizeof(line_user), user)) {
            char* token = strtok(line_user, ",");
            sprintf(user_id, "%s", token);
            token = strtok(NULL, ",");
            if (strcmp(token, username) == 0) {
                userExist = true;
                break;
                }
            }
        fclose(user);

        if (!userExist) {
            char resp[] = "User not found\n";
            if (write(conn->sock, resp, strlen(resp)) < 0) {
                perror("Response send failed");
                }
            fclose(auth);
            return;
            }
        else {
            fprintf(auth, "%s,%s,%s\n", user_id, username, "ADMIN");
            fclose(auth);
            }
        }


    }

void new_room(const char* username, const char* channel_name, const char* room, connection_t* conn) {
    char path_auth[512];
    sprintf(path_auth, "%s/%s/admin/auth.csv", path, channel_name);

    FILE* auth = fopen(path_auth, "r");
    if (auth == NULL) {
        char resp[] = "Error opening auth.csv - room\n";
        if (write(conn->sock, resp, strlen(resp)) < 0) {
            perror("Response send failed");
            }
        return;
        }

    char line[MAX_LEN];
    bool isAdmin = false;
    bool isRoot = false;

    while (fgets(line, sizeof(line), auth)) {
        char* token = strtok(line, ",");
        if (token == NULL) continue;
        token = strtok(NULL, ",");
        if (token == NULL) continue;

        if (strcmp(token, username) == 0) {
            token = strtok(NULL, ",");
            if (strcmp(token, "ADMIN") == 0) {
                isAdmin = true;
                }
            else if (strcmp(token, "ROOT") == 0) {
                isRoot = true;
                }
            }
        }

    fclose(auth);

    if (!isAdmin && !isRoot) {
        char resp[] = "You are not an admin, you can't make a room on this channel\n";
        if (write(conn->sock, resp, strlen(resp)) < 0) {
            perror("Response send failed");
            }
        return;
        }

    char room_path[512];
    sprintf(room_path, "%s/%s/%s", path, channel_name, room);
    struct stat st = { 0 };
    if (stat(room_path, &st) == -1) make_folder(room_path);
    else {
        char resp[] = "Room already exists\n";
        if (write(conn->sock, resp, strlen(resp)) < 0) {
            perror("Response send failed");
            }
        }

    char chat_path[512];
    sprintf(room_path, "%s/%s/%s/chat.csv", path, channel_name, room);
    FILE* chat = fopen(room_path, "w+");
    if (chat == NULL) {
        char resp[] = "Error opening/writing chat.csv\n";
        if (write(conn->sock, resp, strlen(resp)) < 0) {
            perror("Response send failed");
            }
        }
    else fclose(chat);

    char resp[] = "Room created\n";
    if (write(conn->sock, resp, strlen(resp)) < 0) {
        perror("Response send failed");
        }
    }

void list_channel(connection_t* conn) {
    FILE* fp = fopen(path_channels, "r");
    if (fp == NULL) {
        char resp[] = "Error opening channels.csv\n";
        if (write(conn->sock, resp, strlen(resp)) < 0) {
            perror("Response send failed");
            }
        return;
        }

    char line[MAX_LEN];
    char resp[MAX_LEN] = "";
    int count = 0;

    while (fgets(line, sizeof(line), fp)) {
        char* token = strtok(line, ",");
        if (token == NULL) continue;
        token = strtok(NULL, ",");
        if (token == NULL) continue;

        sprintf(resp + strlen(resp), "%s ", token);
        if (write(conn->sock, resp, strlen(resp)) < 0) {
            perror("Response send failed");
            }
        count++;
        }

    if (count == 0) {
        char resp[] = "No channel available\n";
        if (write(conn->sock, resp, strlen(resp)) < 0) {
            perror("Response send failed");
            }
        }

    fclose(fp);
    }

void list_room(const char* channel, connection_t* conn) {
    char path_room[512];
    sprintf(path_room, "%s/%s", path, channel);
    DIR* dir = opendir(path_room);
    if (dir == NULL) {
        char resp[] = "Error opening directory\n";
        if (write(conn->sock, resp, strlen(resp)) < 0) {
            perror("Response send failed");
            }
        return;
        }

    struct dirent* entry;
    char resp[MAX_LEN] = "";

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 && strcmp(entry->d_name, "admin") != 0) {
            char dir_path[512];
            sprintf(dir_path, "%s/%s", path_room, entry->d_name);

            struct stat st;
            stat(dir_path, &st);
            if (S_ISDIR(st.st_mode)) sprintf(resp + strlen(resp), "%s ", entry->d_name);
            }
        }

    if (strlen(resp) == 0) {
        sprintf(resp, "No room available\n");
        }

    if (write(conn->sock, resp, strlen(resp)) < 0) {
        perror("Response send failed");
        }

    fclose(dir);
    }

void list_user(const char* channel, connection_t* conn) {
    char user_path[512];
    sprintf(user_path, "%s/%s/admin/auth.csv", path, channel);
    FILE* auth = fopen(user_path, "r");
    if (auth == NULL) {
        char resp[] = "Error opening auth.csv\n";
        if (write(conn->sock, resp, strlen(resp)) < 0) {
            perror("Response send failed");
            }
        return;
        }

    char line[MAX_LEN];
    char resp[MAX_LEN] = "";

    while (fgets(line, sizeof(line), auth)) {
        char* token = strtok(line, ",");
        if (token == NULL) continue;
        token = strtok(NULL, ",");
        if (token == NULL) continue;

        sprintf(resp + strlen(resp), "%s ", token);
        }

    if (write(conn->sock, resp, strlen(resp)) < 0) {
        perror("Response send failed");
        }

    fclose(auth);
    }

void join_channel(const char* username, const char* channel, connection_t* conn) {

    }

void join_room(const char* channel, const char* room, connection_t* conn) {

    }

void send_message(const char* username, const char* channel, const char* room, const char* message, connection_t* conn) {

    }

void see_message(const char* channel, const char* room, connection_t* conn) {

    }

void edit_message(const char* channel, const char* room, int id_chat, const char* new_text, connection_t* conn) {

    }

void edit_channel(const char* old_channel, const char* new_channel, connection_t* conn) {

    }

void edit_room(const char* channel, const char* old_room, const char* new_room, connection_t* conn) {

    }

void edit_profile(const char* username, const char* new_value, bool is_password, connection_t* conn) {

    }

void remove_message(const char* channel, const char* room, int chat_id, connection_t* conn) {

    }

void remove_channel(const char* channel, connection_t* conn) {

    }

void remove_room(const char* channel, const char* room, connection_t* conn) {

    }

void remove_all_room(const char* channel, connection_t* conn) {

    }

void remove_folder(const char* folder_path, connection_t* conn) {

    }

void remove_user(const char* channel, const char* target, connection_t* conn) {

    }

void ban_user(const char* channel, const char* target, connection_t* conn) {

    }

void unban_user(const char* channel, const char* target, connection_t* conn) {

    }

// ROOT
void list_user_root(connection_t* conn) {
    FILE* fp = fopen(path_user, "r");
    if (fp == NULL) {
        char resp[] = "Error opening users.csv\n";
        if (write(conn->sock, resp, strlen(resp)) < 0) {
            perror("Response send failed");
            }
        return;
        }

    char line[MAX_LEN];
    char resp[MAX_LEN] = "";

    while (fgets(line, sizeof(line), fp)) {
        char* token = strtok(line, ",");
        if (token == NULL) continue;
        token = strtok(NULL, ",");
        if (token == NULL) continue;

        sprintf(resp + strlen(resp), "%s ", token);
        }

    if (write(conn->sock, resp, strlen(resp)) < 0) {
        perror("Response send failed");
        }

    fclose(fp);
    }

void edit_user(const char* target, const char* new_value, bool is_password, connection_t* conn) {

    }

void remove_root(const char* target, connection_t* conn) {

    }


void exit_func(connection_t* conn) {
    if (strlen(conn->roomLogged) > 0) {
        memset(conn->roomLogged, 0, sizeof(conn->roomLogged));
        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response), "[%s/%s]", conn->userLogged, conn->channelLogged);
        if (write(conn->sock, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
            }
        }
    else if (strlen(conn->channelLogged) > 0) {
        memset(conn->channelLogged, 0, sizeof(conn->channelLogged));
        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response), "[%s]", conn->userLogged);
        if (write(conn->sock, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
            }
        }
    else {
        char response[] = "Anda telah keluar dari aplikasi";
        if (write(conn->sock, response, strlen(response)) < 0) {
            perror("Gagal mengirim respons ke client");
            }
        close(conn->sock);
        pthread_exit(NULL);
        }
    }

// ROOT

// LOG
void log_activity(const char* channel, const char* message) {
    char log_path[512];
    sprintf(log_path, "%s/%s/admin/user.log", path, channel);

    FILE* log_file = fopen(log_path, "a+");
    if (!log_file) {
        perror("Error opening user.log");
        return;
        }

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char date[30];
    strftime(date, sizeof(date), "%d/%m/%Y %H:%M:%S", t);

    fprintf(log_file, "[%s] %s\n", date, message);
    fclose(log_file);
    }

// ====================================================================================================
// ====================================================================================================
// TODO: Implement the functions above
// ====================================================================================================
// ====================================================================================================

void* discorit_handler(void* input) {
    connection_t* conn = (connection_t*)input;
    char buf[MAX_LEN], * resp;
    int n;

    while (1) {
        n = recv(conn->sock, buf, sizeof(buf), 0);
        if (n <= 0) {
            perror("Receive failed");
            break;
            }

        buf[n] = '\0';
        printf("Received: %s\n", buf);

        char* token = strtok(buf, " ");
        if (token == NULL) {
            resp = "Invalid command\n";
            if (send(conn->sock, resp, strlen(resp), 0) != strlen(resp)) {
                perror("Send failed");
                break;
                }
            continue;
            }

        if (strcmp(token, "REGISTER") == 0) {
            char* usr = strtok(NULL, " ");
            char* pass = strtok(NULL, " ");
            register_func(usr, pass, conn);
            }
        else if (strcmp(token, "LOGIN") == 0) {
            char* usr = strtok(NULL, " ");
            char* pass = strtok(NULL, " ");
            login_func(usr, pass, conn);
            }

        // ====================================================================================================
        // CONTINUE
        // ====================================================================================================

        else {
            printf("Invalid command\n");
            break;
            }
        }

    close(conn->sock);
    free(conn);
    pthread_exit(0);
    }

int main(int argc, char* argv[]) {
    int serv_socket, cli_socket;
    struct sockaddr_in serv_addr, cli_addr;
    int addrlen = sizeof(serv_addr);
    char buffer[MAX_LEN] = { 0 };

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
    if (bind(serv_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
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

    while (1) {
        // accept incoming connection
        if ((cli_socket = accept(serv_socket, (struct sockaddr*)&cli_addr, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
            }

#ifdef DEBUG
        printf("Connection established - [%s] on port [%d]\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
#endif

        // discorit handler thread creation
        pthread_t tid;
        connection_t* connection = (connection_t*)malloc(sizeof(connection_t));
        connection->sock = cli_socket;
        connection->addr = cli_addr;
        memset(connection->userLogged, 0, sizeof(connection->userLogged));
        memset(connection->channelLogged, 0, sizeof(connection->channelLogged));
        memset(connection->roleLogged, 0, sizeof(connection->roleLogged));
        memset(connection->roomLogged, 0, sizeof(connection->roomLogged));
        pthread_create(&tid, NULL, discorit_handler, (void*)connection);
        }
    }