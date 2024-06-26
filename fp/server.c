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
#define MAX_USER 100
#define SALT_SIZE 30
#define BCRYPT_HASHSIZE 61
#define BUFFER_SIZE 10240
#define path "/home/etern1ty/sisop_works/FP/fp/DiscorIT"
#define path_user "/home/etern1ty/sisop_works/FP/fp/DiscorIT/users.csv"
#define path_channels "/home/etern1ty/sisop_works/FP/fp/DiscorIT/channels.csv"
// #define path "/Users/macbook/Kuliah/Sistem Operasi/fp-sisop/fp/DiscorIT"
// #define path_user "/Users/macbook/Kuliah/Sistem Operasi/fp-sisop/fp/DiscorIT/users.csv"
// #define path_channels "/Users/macbook/Kuliah/Sistem Operasi/fp-sisop/fp/DiscorIT/channels.csv"

#define DEBUG
typedef struct {
    int sock;
    struct sockaddr_in addr;
    char userLogged[MAX_USER];
    char channelLogged[MAX_USER];
    char roleLogged[MAX_USER];
    char roomLogged[MAX_USER];
    } connection_t;

void verifyKey(const char* user, const char* channel, const char* key, connection_t* conn);
void finalize_modification(const char* original_path, const char* temp_path, bool found, int message_id, connection_t* conn);
void make_folder(char* folder_path);
void register_func(char username[MAX_LEN], char password[MAX_LEN], connection_t* conn);
bool check_if_root_user(FILE* auth_file, const char* logged_user);
bool check_if_admin_user(FILE* auth_file, const char* logged_user);
void process_channels_file(FILE* channels_file, FILE* temp_file, const char* old_channel_name, const char* new_channel_name, bool* channel_found, bool* new_channel_exists);
void finalize_channel_update(const char* temp_file_path, const char* old_channel_name, const char* new_channel_name, bool is_root_user, connection_t* conn);
void edit_room(const char* channel, const char* old_room, const char* new_room, connection_t* conn);
void edit_profile(const char* username, const char* new_value, bool is_password, connection_t* conn);
void remove_message(const char* channel, const char* room, int chat_id, connection_t* conn);
void remove_channel(const char* channel, connection_t* conn);
void remove_room(const char* channel, const char* room, connection_t* conn);
void remove_all_room(const char* channel, connection_t* conn);
void edit_message(const char* channel_name, const char* room_name, int message_id, const char* new_text, connection_t* conn);
void send_response(connection_t* connection, const char* message);
bool process_chat_file(FILE* original_file, FILE* temp_file, int message_id, const char* new_text);
void finalize_modification(const char* original_path, const char* temp_path, bool found, int message_id, connection_t* conn);
void edit_channel(const char* old_channel_name, const char* new_channel_name, connection_t* conn);
bool is_room_name_in_use(const char* channel, const char* room_name);
bool does_room_exist(const char* channel, const char* room_name);
bool rename_room(const char* channel, const char* old_room, const char* new_room);
void log_room_change(const char* channel, const char* old_room, const char* new_room, const char* username);
void remove_user(const char* channel, const char* target, connection_t* conn);
void ban_user(const char* channel, const char* target, connection_t* conn);
void unban_user(const char* channel, const char* target, connection_t* conn);
void remove_root(const char* target, connection_t* conn);
void list_user_root(connection_t* conn);
void new_channel(const char* username, const char* channel_name, const char* key, connection_t* conn);
void new_room(const char* username, const char* channel_name, const char* room, connection_t* conn);
void list_channel(connection_t* conn);
void list_room(const char* channel, connection_t* conn);
void list_user(const char* channel, connection_t* conn);
void join_channel(const char* username, const char* channel, connection_t* conn);
void join_room(const char* channel, const char* room, connection_t* conn);
void send_message(const char* username, const char* channel, const char* room, const char* message, connection_t* conn);
void see_messages(const char* channel_name, const char* room, connection_t* conn);
void exit_func(connection_t* conn);




void sendErrorResponse(connection_t* conn, const char* errorMsg) {
    if (write(conn->sock, errorMsg, strlen(errorMsg)) < 0) {
        perror("Response send failed");
        }
    }

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
    char date[30];
    strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", localtime(&now));

    fprintf(log_file, "[%s] %s\n", date, message);
    fclose(log_file);
    fflush(stdout);
    }

void make_folder(char* folder_path) {
    struct stat st = { 0 };
    if (stat(folder_path, &st) == -1) mkdir(folder_path, 0777);
    }

void delete_folder(char* folder_path) {
    DIR* dir = opendir(folder_path);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            char fpath[512];
            sprintf(fpath, "%s/%s", folder_path, entry->d_name);
            remove(fpath);
            }
        closedir(dir);
        rmdir(folder_path);
        }
    }

void register_func(char username[MAX_LEN], char password[MAX_LEN], connection_t* conn) {

#ifdef DEBUG
    printf("Registering %s\n", username);
#endif

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

#ifdef DEBUG
    printf("Role: %s\n", role);
#endif

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

    sprintf(salt, "$2a$10$%.22s", "SISOPGOATIT04SALTCODEREAL");
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
    bool isPasswordCorrect = false;
    char salt[MAX_LEN], hashed[MAX_LEN], file_hashed[MAX_LEN];
    int file_userid;
    char file_username[MAX_LEN], file_role[MAX_LEN];

    snprintf(salt, MAX_LEN, "$2a$10$%.22s", "SISOPGOATIT04SALTCODEREAL");
    strcpy(hashed, crypt(password, salt));

    fp = fopen(path_user, "r");
    if (fp == NULL) {
        char resp[] = "Error opening file\n";
        if (write(conn->sock, resp, strlen(resp)) < 0) {
            perror("Response send failed");
            }
        fsync(conn->sock);
        return;
        }

    while ((read = getline(&line, &len, fp)) != -1) {
        char* token = strtok(line, ",");
        token = strtok(NULL, ",");
        if (strcmp(token, username) != 0) continue;

        isExist = true;
        token = strtok(NULL, ",");
        strcpy(file_hashed, token);
        if (strcmp(hashed, file_hashed) == 0) {
            isPasswordCorrect = true;
            strcpy(conn->userLogged, username);
            token = strtok(NULL, ",");
            strcpy(conn->roleLogged, token);
            break;
            }
        }

    fclose(fp);
    if (line) free(line);

    char resp[100];
    if (!isExist) strcpy(resp, "Invalid username\n");
    else if (!isPasswordCorrect) strcpy(resp, "Invalid password\n");
    else {
        sprintf(resp, "%s logged in", username);
        printf("%s\n", resp);
        }
    if (write(conn->sock, resp, strlen(resp)) < 0) {
        perror("Response send failed");
        }
    fsync(conn->sock);
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
    sprintf(salt, "$2a$10$%.22s", "SISOPGOATIT04SALTCODEREAL");
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
            if (strstr(token, "ADMIN") == 0) {
                isAdmin = true;
                }
            else if (strstr(token, "ROOT") == 0) {
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
        memset(resp, 0, sizeof(resp));
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
            char dir_path[1024];
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

    closedir(dir);
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
    char channel_path[512];
    sprintf(channel_path, "%s/%s", path, channel);
    struct stat st;
    if (stat(channel_path, &st) == -1 || !S_ISDIR(st.st_mode)) { // does it exist
        char response[BUFFER_SIZE];
        sprintf(response, "%s doesn't exist", channel);
        if (write(conn->sock, response, strlen(response)) < 0) {
            perror("Response send failed");
            }
        return;
        }

    FILE* users_file = fopen(path_user, "r"); // is user root
    if (!users_file) {
        char response[] = "Error opening users.csv";
        if (write(conn->sock, response, strlen(response)) < 0) {
            perror("Response send failed");
            }
        return;
        }

    char line[256];
    bool is_root = false;
    char user_id[10];

    while (fgets(line, sizeof(line), users_file)) {
        char* token = strtok(line, ",");
        strcpy(user_id, token);
        token = strtok(NULL, ",");
        char* name = token;
        if (token && strstr(name, username) != NULL) {
            token = strtok(NULL, ",");
            token = strtok(NULL, ",");
            char* role = token;
            if (strstr(role, "ROOT") != NULL) {
                is_root = true;
                }
            break;
            }
        }

    fclose(users_file);

    if (is_root) {
        snprintf(conn->channelLogged, sizeof(conn->channelLogged), "%s", channel); // join instantly if root

        // ensure root is written in auth.csv
        char auth_path[512];
        sprintf(auth_path, "%s/%s/admin/auth.csv", path, channel);
        FILE* auth_file = fopen(auth_path, "r+");
        if (auth_file) {
            bool root_exists = false;
            while (fgets(line, sizeof(line), auth_file)) {
                char* token = strtok(line, ",");
                if (token == NULL) continue;
                token = strtok(NULL, ",");
                if (token == NULL) continue;
                if (strcmp(token, username) == 0) {
                    root_exists = true;
                    break;
                    }
                }

            if (!root_exists) {
                auth_file = fopen(auth_path, "a");
                if (auth_file) {
                    fprintf(auth_file, "%s,%s,ROOT\n", user_id, username);
                    fclose(auth_file);
                    }
                }
            else {
                fclose(auth_file);
                }
            }
        else {
            char response[] = "Error opening auth.csv";
            if (write(conn->sock, response, strlen(response)) < 0) {
                perror("Response send failed");
                }
            return;
            }

        char response[BUFFER_SIZE];
        sprintf(response, "[%s/%s]", username, channel);
        if (write(conn->sock, response, strlen(response)) < 0) {
            perror("Response send failed");
            }
        return;
        }

    // is user admin/user/banned
    char auth_path[512];
    sprintf(auth_path, "%s/%s/admin/auth.csv", path, channel);
    FILE* auth_file = fopen(auth_path, "r");
    if (!auth_file) {
        char response[] = "Error opening auth.csv";
        if (write(conn->sock, response, strlen(response)) < 0) {
            perror("Response send failed");
            }
        return;
        }

    bool is_admin = false;
    bool is_user = false;
    bool is_banned = false;

    while (fgets(line, sizeof(line), auth_file)) {
        char* token = strtok(line, ",");
        if (token == NULL) continue;
        token = strtok(NULL, ",");
        if (token == NULL) continue;
        if (strcmp(token, username) == 0) {
            token = strtok(NULL, ",");
            if (strstr(token, "ADMIN") != NULL) {
                is_admin = true;
                break;
                }
            else if (strstr(token, "USER") != NULL) {
                is_user = true;
                break;
                }
            else if (strstr(token, "BANNED") != NULL) {
                is_banned = true;
                break;
                }
            }
        }

    fclose(auth_file);

    if (is_banned) {
        char response[] = "You have been banneed, please contact an admin.";
        if (write(conn->sock, response, strlen(response)) < 0) {
            perror("Response send failed");
            }
        return;
        }

    if (is_admin || is_user) {
        sprintf(conn->channelLogged, "%s", channel);
        char response[BUFFER_SIZE];
        sprintf(response, "[%s/%s]", username, channel);
        if (write(conn->sock, response, strlen(response)) < 0) {
            perror("Response send failed");
            }
        return; // admin/user immidiately join
        }
    else {
        // else, ask for key
        char response[] = "Key: ";
        if (write(conn->sock, response, strlen(response)) < 0) {
            perror("Response send failed");
            }

        char key[BUFFER_SIZE];
        memset(key, 0, sizeof(key));

        if (recv(conn->sock, key, sizeof(key), 0) < 0) {
            perror("Failed to receive key from client");
            return;
            }
        verifyKey(username, channel, key, conn);
        }
    }

void join_room(const char* channel, const char* room, connection_t* conn) {
    char room_path[256];
    sprintf(room_path, "%s/%s/%s", path, channel, room);
    struct stat st;
    if (stat(room_path, &st) == -1 || !S_ISDIR(st.st_mode)) { // does it exist
        char response[] = "Room doesn't exist in the channel";
        if (write(conn->sock, response, strlen(response)) < 0) {
            perror("Response send failed");
            }
        return;
        }

    sprintf(conn->roomLogged, "%s", room);
    char response[BUFFER_SIZE];
    sprintf(response, "[%s/%s/%s]", conn->userLogged, channel, room);
    if (write(conn->sock, response, strlen(response)) < 0) {
        perror("Response send failed");
        }
    }

int find_last_id(FILE* chat_file) {
    char line[512];
    int last_id = 0;

    while (fgets(line, sizeof(line), chat_file)) {
        char* token = strtok(line, "|");
        token = strtok(NULL, "|");
        if (token) {
            last_id = atoi(token);
            }
        }

    return last_id;
    }

void send_message(const char* username, const char* channel, const char* room, const char* message, connection_t* conn) {
    char* start_quote = strchr(message, '\"');
    char* end_quote = strrchr(message, '\"');

    if (start_quote == NULL || end_quote == NULL || start_quote == end_quote) {
        sendErrorResponse(conn, "Usage: CHAT \"<message>\"");
        return;
        }

    size_t message_length = end_quote - start_quote - 1;
    char trimmed[message_length + 1];
    strncpy(trimmed, message + 1, message_length);
    trimmed[message_length] = '\0';

    if (trimmed[0] == '\0') {
        sendErrorResponse(conn, "Message cannot be empty");
        return;
        }

    char chat_path[512];
    sprintf(chat_path, "%s/%s/%s/chat.csv", path, channel, room);

    FILE* chat_file = fopen(chat_path, "a");
    if (!chat_file) {
        sendErrorResponse(conn, "Failed to open chat.csv");
        return;
        }

    int last_id = find_last_id(chat_file);
    int id = last_id + 1;

    time_t current_time = time(NULL);
    struct tm* time_info = localtime(&current_time);
    char date[30];
    strftime(date, sizeof(date), "%d/%m/%Y %H:%M:%S", time_info);

    fprintf(chat_file, "%s|%d|%s|%s\n", date, id, username, trimmed);
    fclose(chat_file);

    char response[100];
    sprintf(response, "Message sent successfully");
    if (write(conn->sock, response, strlen(response)) < 0) {
        perror("Failed to send response to client");
        }
    }

void see_messages(const char* channel_name, const char* room, connection_t* conn) {
    char messages_path[512];
    sprintf(messages_path, "%s/%s/%s/chat.csv", path, channel_name, room);

    FILE* messages_file = fopen(messages_path, "r");
    if (!messages_file) {
        char response[] = "Error opening chat.csv or no messages in room";
        if (write(conn->sock, response, strlen(response)) < 0) {
            perror("Failed to send response to client");
            }
        return;
        }

    char line[512];
    char response[BUFFER_SIZE] = "";
    bool has_messages = false;

    while (fgets(line, sizeof(line), messages_file)) {
        has_messages = true;
        char* date = strtok(line, "|");
        char* message_id = strtok(NULL, "|");
        char* sender = strtok(NULL, "|");
        char* message = strtok(NULL, "|");

        message[strcspn(message, "\n")] = '\0';

        if (date && message_id && sender && message) {
            sprintf(response + strlen(response), "[%s][%s][%s] “%s” \n", date, message_id, sender, message);
            }
        }

    if (!has_messages) {
        sprintf(response, "No messages");
        }

    if (write(conn->sock, response, strlen(response)) < 0) {
        perror("Failed to send response to client");
        }

    fclose(messages_file);
    }

bool process_chat_file(FILE* original_file, FILE* temp_file, int message_id, const char* new_text) {
    char buffer[512];
    bool found = false;

    while (fgets(buffer, sizeof(buffer), original_file)) {
        char* tokens[4];
        int i = 0;
        char* token = strtok(buffer, "|");
        while (token != NULL && i < 4) {
            tokens[i++] = token;
            token = strtok(NULL, "|");
            }

        int current_id = atoi(tokens[1]);
        if (current_id == message_id) {
            found = true;
            fprintf(temp_file, "%s|%d|%s|%s\n", tokens[0], current_id, tokens[2], new_text);
            }
        else {
            fprintf(temp_file, "%s|%d|%s|%s\n", tokens[0], current_id, tokens[2], tokens[3]);
            }
        }

    return found;
    }

void finalize_modification(const char* original_path, const char* temp_path, bool found, int message_id, connection_t* conn) {
    if (found) {
        remove(original_path);
        rename(temp_path, original_path);
        char success_msg[100];
        snprintf(success_msg, sizeof(success_msg), "Chat with ID %d edited", message_id);
        send_response(conn, success_msg);
        }
    else {
        remove(temp_path);
        char not_found_msg[100];
        snprintf(not_found_msg, sizeof(not_found_msg), "Chat with ID %d not found", message_id);
        send_response(conn, not_found_msg);
        }
    }

void edit_message(const char* channel_name, const char* room_name, int message_id, const char* new_text, connection_t* conn) {
    char chat_file[512];
    snprintf(chat_file, sizeof(chat_file), "%s/%s/%s/chat.csv", path, channel_name, room_name);

    FILE* original_file = fopen(chat_file, "r");
    if (!original_file) {
        const char error_msg[] = "Error opening chat.csv";
        send_response(conn, error_msg);
        return;
        }

    char temp_file[512];
    snprintf(temp_file, sizeof(temp_file), "%s/%s/%s/chat_temp.csv", path, channel_name, room_name);
    FILE* temp = fopen(temp_file, "w");
    if (!temp) {
        const char error_msg[] = "Error creating temporary file for editing chat";
        send_response(conn, error_msg);
        fclose(original_file);
        return;
        }

    bool found = process_chat_file(original_file, temp, message_id, new_text);

    fclose(original_file);
    fclose(temp);

    finalize_modification(chat_file, temp_file, found, message_id, conn);
    }

void send_response(connection_t* connection, const char* message) {
    if (write(connection->sock, message, strlen(message)) < 0) {
        perror("Failed to send response to client");
        }
    }

void edit_channel(const char* old_channel_name, const char* new_channel_name, connection_t* conn) {
    char auth_file_path[256];
    snprintf(auth_file_path, sizeof(auth_file_path), "%s/%s/admin/auth.csv", path, old_channel_name);
    FILE* auth_file = fopen(auth_file_path, "r");
    if (!auth_file) {
        char response[] = "Error opening auth.csv";
        if (write(conn->sock, response, strlen(response)) < 0) {
            perror("Response send failed");
            }
        return;
        }

    FILE* users_file = fopen(path_user, "r");
    if (!users_file) {
        char response[] = "Error opening users.csv";
        if (write(conn->sock, response, strlen(response)) < 0) {
            perror("Response send failed");
            }
        return;
        }

    bool is_root_user = check_if_root_user(users_file, conn->userLogged);
    fclose(users_file);

    bool is_admin_user = check_if_admin_user(auth_file, conn->userLogged);
    fclose(auth_file);

    if (!is_admin_user && !is_root_user) {
        char response[] = "You do not have permission to edit the channel";
        if (write(conn->sock, response, strlen(response)) < 0) {
            perror("Response send failed");
            }
        return;
        }

    FILE* channels_file = fopen(path_channels, "r+");
    if (!channels_file) {
        char response[] = "Error opening channels.csv";
        if (write(conn->sock, response, strlen(response)) < 0) {
            perror("Response send failed");
            }
        return;
        }

    char temp_file_path[256];
    snprintf(temp_file_path, sizeof(temp_file_path), "%s/channels_temp.csv", path);
    FILE* temp_file = fopen(temp_file_path, "w+");
    if (!temp_file) {
        char response[] = "Error creating temporary file for editing channel";
        if (write(conn->sock, response, strlen(response)) < 0) {
            perror("Response send failed");
            }
        fclose(channels_file);
        return;
        }

    bool channel_found = false;
    bool new_channel_exists = false;
    process_channels_file(channels_file, temp_file, old_channel_name, new_channel_name, &channel_found, &new_channel_exists);

    fclose(channels_file);
    fclose(temp_file);

    if (new_channel_exists) {
        remove(temp_file_path);
        char response[] = "Channel name already in use";
        if (write(conn->sock, response, strlen(response)) < 0) {
            perror("Response send failed");
            }
        return;
        }

    if (channel_found) {
        finalize_channel_update(temp_file_path, old_channel_name, new_channel_name, is_root_user, conn);
        }
    else {
        remove(temp_file_path);
        char response[100];
        snprintf(response, sizeof(response), "Channel %s not found", old_channel_name);
        if (write(conn->sock, response, strlen(response)) < 0) {
            perror("Response send failed");
            }
        }
    }

bool check_if_root_user(FILE* users_file, const char* logged_user) {
    char line[256];
    while (fgets(line, sizeof(line), users_file)) {
        char* token = strtok(line, ",");
        token = strtok(NULL, ",");
        if (token && strcmp(token, logged_user) == 0) {
            token = strtok(NULL, ",");
            token = strtok(NULL, ",");
            if (strstr(token, "ROOT") != NULL) {
                return true;
                }
            break;
            }
        }
    return false;
    }

bool check_if_admin_user(FILE* auth_file, const char* logged_user) {
    char line[256];
    while (fgets(line, sizeof(line), auth_file)) {
        char* token = strtok(line, ",");
        if (token == NULL) continue;
        token = strtok(NULL, ",");
        if (token == NULL) continue;
        if (strcmp(token, logged_user) == 0) {
            token = strtok(NULL, ",");
            if (strstr(token, "ADMIN") != NULL) {
                return true;
                }
            }
        }
    return false;
    }

void process_channels_file(FILE* channels_file, FILE* temp_file, const char* old_channel_name, const char* new_channel_name, bool* channel_found, bool* new_channel_exists) {
    char line[256];
    while (fgets(line, sizeof(line), channels_file)) {
        char* id = strtok(line, ",");
        char* channel_name = strtok(NULL, ",");
        char* key = strtok(NULL, ",");
        if (channel_name && strcmp(channel_name, new_channel_name) == 0) {
            *new_channel_exists = true;
            break;
            }
        if (channel_name && strcmp(channel_name, old_channel_name) == 0) {
            *channel_found = true;
            fprintf(temp_file, "%s,%s,%s", id, new_channel_name, key);
            }
        else {
            fprintf(temp_file, "%s,%s,%s", id, channel_name, key);
            }
        }
    }

void finalize_channel_update(const char* temp_file_path, const char* old_channel_name, const char* new_channel_name, bool is_root_user, connection_t* conn) {
    remove(path_channels);
    rename(temp_file_path, path_channels);

    char old_path[256];
    snprintf(old_path, sizeof(old_path), "%s/%s", path, old_channel_name);
    char new_path[256];
    snprintf(new_path, sizeof(new_path), "%s/%s", path, new_channel_name);
    rename(old_path, new_path);

    char log_message[100];
    if (is_root_user) {
        snprintf(log_message, sizeof(log_message), "ROOT changed channel %s to %s", old_channel_name, new_channel_name);
        }
    else {
        snprintf(log_message, sizeof(log_message), "ADMIN changed channel %s to %s", old_channel_name, new_channel_name);
        }
    log_activity(new_channel_name, log_message);

    char response[100];
    snprintf(response, sizeof(response), "%s successfully changed to %s", old_channel_name, new_channel_name);
    if (write(conn->sock, response, strlen(response)) < 0) {
        perror("Response send failed");
        }
    }

bool is_user_root(const char* username) {
    FILE* users_file = fopen(path_user, "r");
    if (!users_file) return false;

    char line[256];
    bool is_root = false;
    while (fgets(line, sizeof(line), users_file)) {
        char* token = strtok(line, ",");
        token = strtok(NULL, ",");
        if (token && strcmp(token, username) == 0) {
            token = strtok(NULL, ",");
            token = strtok(NULL, ",");
            if (strstr(token, "ROOT") != NULL) {
                is_root = true;
                break;
                }
            }
        }
    fclose(users_file);
    return is_root;
    }

bool is_user_admin(const char* username, const char* channel) {
    char auth_path[256];
    snprintf(auth_path, sizeof(auth_path), "%s/%s/admin/auth.csv", path, channel);
    FILE* auth_file = fopen(auth_path, "r");
    if (!auth_file) return false;

    char line[256];
    bool is_admin = false;
    while (fgets(line, sizeof(line), auth_file)) {
        char* token = strtok(line, ",");
        token = strtok(NULL, ",");
        if (token && strcmp(token, username) == 0) {
            token = strtok(NULL, ",");
            if (strstr(token, "ADMIN") != NULL) {
                is_admin = true;
                break;
                }
            }
        }
    fclose(auth_file);
    return is_admin;
    }

bool has_permission_to_edit_room(connection_t* conn, const char* channel) {
    return is_user_root(conn->userLogged) || is_user_admin(conn->userLogged, channel);
    }

bool is_room_name_in_use(const char* channel, const char* room_name) {
    char check_path[256];
    snprintf(check_path, sizeof(check_path), "%s/%s/%s", path, channel, room_name);
    struct stat st;
    return stat(check_path, &st) == 0;
    }

bool does_room_exist(const char* channel, const char* room_name) {
    char room_path[256];
    snprintf(room_path, sizeof(room_path), "%s/%s/%s", path, channel, room_name);
    struct stat st;
    return stat(room_path, &st) != -1 && S_ISDIR(st.st_mode);
    }

bool rename_room(const char* channel, const char* old_room, const char* new_room) {
    char old_path[256];
    snprintf(old_path, sizeof(old_path), "%s/%s/%s", path, channel, old_room);
    char new_path[256];
    snprintf(new_path, sizeof(new_path), "%s/%s/%s", path, channel, new_room);
    return rename(old_path, new_path) == 0;
    }

void log_room_change(const char* channel, const char* old_room, const char* new_room, const char* username) {
    char log_message[100];
    if (is_user_root(username)) {
        snprintf(log_message, sizeof(log_message), "ROOT changed room %s to %s", old_room, new_room);
        }
    else {
        snprintf(log_message, sizeof(log_message), "ADMIN changed room %s to %s", old_room, new_room);
        }
    log_activity(channel, log_message);
    }

void edit_room(const char* channel, const char* old_room, const char* new_room, connection_t* conn) {
    if (!has_permission_to_edit_room(conn, channel)) {
        send_response(conn, "You do not have permission to edit the room");
        return;
        }

    if (is_room_name_in_use(channel, new_room)) {
        send_response(conn, "Room name already in use");
        return;
        }

    if (!does_room_exist(channel, old_room)) {
        char response[100];
        snprintf(response, sizeof(response), "Room %s does not exist in channel %s", old_room, channel);
        send_response(conn, response);
        return;
        }

    if (rename_room(channel, old_room, new_room)) {
        log_room_change(channel, old_room, new_room, conn->userLogged);
        char response[100];
        snprintf(response, sizeof(response), "%s successfully changed to %s", old_room, new_room);
        send_response(conn, response);
        }
    else {
        send_response(conn, "Failed to change room name");
        }
    }


void update_user_profile(FILE* temp_file, const char* user_id, const char* user_name, const char* hash, const char* role, const char* new_value, bool is_password) {
    if (is_password) {
        char salt[MAX_LEN];
        snprintf(salt, sizeof(salt), "$2y$12$%.22s", "SISOPGOATIT04SALTCODEREAL");
        char new_hash[BCRYPT_HASHSIZE];
        crypt(new_hash, (new_value, salt));
        fprintf(temp_file, "%s,%s,%s,%s\n", user_id, user_name, new_hash, role);
        }
    else {
        fprintf(temp_file, "%s,%s,%s,%s\n", user_id, new_value, hash, role);
        }
    }

void edit_profile(const char* username, const char* new_value, bool is_password, connection_t* conn) {
    FILE* file = fopen(path_user, "r+");
    if (!file) {
        send_response(conn, "Failed to open users.csv");
        return;
        }

    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "%s/users_temp.csv", path);
    FILE* temp_file = fopen(temp_path, "w");
    if (!temp_file) {
        send_response(conn, "Failed to create temporary file");
        fclose(file);
        return;
        }

    char line[256];
    bool found = false;
    bool name_exists = false;
    while (fgets(line, sizeof(line), file)) {
        char* user_id = strtok(line, ",");
        char* user_name = strtok(NULL, ",");
        char* hash = strtok(NULL, ",");
        char* role = strtok(NULL, ",");

        if (user_name && strcmp(user_name, new_value) == 0 && !is_password) {
            name_exists = true;
            break;
            }

        if (user_name && strcmp(user_name, username) == 0) {
            found = true;
            update_user_profile(temp_file, user_id, user_name, hash, role, new_value, is_password);
            }
        else {
            fprintf(temp_file, "%s,%s,%s,%s\n", user_id, user_name, hash, role);
            }
        }

    fclose(file);
    fclose(temp_file);

    if (name_exists) {
        remove(temp_path);
        send_response(conn, "Username already exists");
        return;
        }

    if (found) {
        remove(path_user);
        rename(temp_path, path_user);
        char response[100];
        snprintf(response, sizeof(response), is_password ? "Password updated" : "Profile updated");
        send_response(conn, response);
        }
    else {
        remove(temp_path);
        send_response(conn, "User not found");
        }
    }

void remove_message(const char* channel, const char* room, int chat_id, connection_t* conn) {
    char chat_path[256];
    sprintf(chat_path, "%s/%s/%s/chat.csv", path, channel, room);
    FILE* chat_file = fopen(chat_path, "r");
    if (!chat_file) {
        send_response(conn, "Error opening chat.csv");
        return;
        }

    char temp_path[256];
    sprintf(temp_path, "%s/%s/%s/chat_temp.csv", path, channel, room);
    FILE* temp_file = fopen(temp_path, "w");
    if (!temp_file) {
        send_response(conn, "Error creating temporary file for editing chat");
        fclose(chat_file);
        return;
        }

    bool found = process_chat_file(chat_file, temp_file, chat_id, "");

    fclose(chat_file);
    fclose(temp_file);

    finalize_modification(chat_path, temp_path, found, chat_id, conn);
    }

void remove_channel(const char* channel, connection_t* conn) {
    FILE* users_file = fopen(path_user, "r");
    if (!users_file) {
        send_response(conn, "Error opening users.csv");
        return;
        }

    char line[256];
    bool is_root_user = check_if_root_user(users_file, conn->userLogged);
    bool is_admin_user = check_if_admin_user(users_file, conn->userLogged);
    fclose(users_file);

    if (!is_root_user && !is_admin_user) {
        send_response(conn, "You do not have permission to remove the channel");
        return;
        }

    char path_channel[256];
    sprintf(path_channel, "%s/%s", path, channel);
    struct stat st;
    if (stat(path_channel, &st) == -1 || !S_ISDIR(st.st_mode)) {
        send_response(conn, "Channel does not exist");
        return;
        }

    delete_folder(path_channel);

    FILE* channels_file = fopen(path_channels, "r");
    if (!channels_file) {
        send_response(conn, "Error opening channels.csv");
        return;
        }

    char temp_file_path[256];
    sprintf(temp_file_path, "%s/channels_temp.csv", path);
    FILE* tfile = fopen(temp_file_path, "w+");
    if (!tfile) {
        send_response(conn, "Error creating temporary file for editing channel");
        fclose(channels_file);
        return;
        }

    while (fgets(line, sizeof(line), channels_file)) {
        char* token = strtok(line, ",");
        token = strtok(NULL, ",");
        if (token && strcmp(token, channel) != 0) {
            fprintf(tfile, "%s", line);
            }
        }

    fclose(channels_file);
    fclose(tfile);

    remove(path_channels);
    rename(temp_file_path, path_channels);

    char resp[100];
    sprintf(resp, "Channel %s removed", channel);
    send_response(conn, resp);

    }

void remove_room(const char* channel, const char* room, connection_t* conn) {
    bool has_permission = has_permission_to_edit_room(conn, channel);
    if (!has_permission) {
        send_response(conn, "You do not have permission to remove the room");
        return;
        }

    char room_path[256];
    sprintf(room_path, "%s/%s/%s", path, channel, room);
    struct stat st;
    if (stat(room_path, &st) == -1 || !S_ISDIR(st.st_mode)) {
        send_response(conn, "Room does not exist");
        return;
        }

    delete_folder(room_path);

    char resp[100];
    sprintf(resp, "Room %s removed", room);
    send_response(conn, resp);
    }

void remove_all_room(const char* channel, connection_t* conn) {
    bool has_permission = has_permission_to_edit_room(conn, channel);
    if (!has_permission) {
        send_response(conn, "You do not have permission to remove all rooms");
        return;
        }

    char channel_path[256];
    sprintf(channel_path, "%s/%s", path, channel);
    DIR* channel_dir = opendir(channel_path);
    struct stat st;
    if (stat(channel_path, &st) == -1 || !S_ISDIR(st.st_mode)) {
        send_response(conn, "Channel does not exist");
        return;
        }

    struct dirent* entry;
    while ((entry = readdir(channel_dir)) != NULL) {
        if (strcmp(entry->d_name, "ADMIN") != 0 && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char room_path[1024];
            sprintf(room_path, "%s/%s", channel_path, entry->d_name);

            delete_folder(room_path);
            }
        }

    char resp[100];
    sprintf(resp, "All rooms in channel %s removed", channel);
    send_response(conn, resp);
    }

void remove_user(const char* channel, const char* target, connection_t* conn) {
    char auth_path[256];
    sprintf(auth_path, "%s/%s/admin/auth.csv", path, channel);
    FILE* auth_file = fopen(auth_path, "r");
    if (!auth_file) {
        send_response(conn, "Error opening auth.csv");
        return;
        }

    char temp_file_path[256];
    sprintf(temp_file_path, "%s/auth_temp.csv", path);
    FILE* temp_file = fopen(temp_file_path, "w");
    if (!temp_file) {
        send_response(conn, "Error creating temporary file for editing auth");
        fclose(auth_file);
        return;
        }

    bool found = false;
    char line[256];
    while (fgets(line, sizeof(line), auth_file)) {
        char* token = strtok(line, ",");
        if (token == NULL) continue;
        token = strtok(NULL, ",");
        if (token == NULL) continue;
        if (strcmp(token, target) == 0) {
            found = true;
            continue;
            }
        fprintf(temp_file, "%s", line);
        }

    fclose(auth_file);
    fclose(temp_file);

    if (found) {
        remove(auth_path);
        rename(temp_file_path, auth_path);
        char resp[100];
        sprintf(resp, "User %s removed from channel %s", target, channel);
        send_response(conn, resp);
        }
    else {
        remove(temp_file_path);
        send_response(conn, "User not found in channel");
        }

    }

// Helper function to check if a user has admin or root privileges
bool check_user_privileges(const char* user, const char* channel, bool* is_admin, bool* is_root) {
    char auth_file_path[256];
    snprintf(auth_file_path, sizeof(auth_file_path), "%s/%s/admin/auth.csv", path, channel);
    FILE* auth_file = fopen(auth_file_path, "r");
    if (!auth_file) {
        return false;
        }

    char line[256];
    while (fgets(line, sizeof(line), auth_file)) {
        char* token = strtok(line, ",");
        if (token == NULL) continue;
        token = strtok(NULL, ",");
        if (token == NULL) continue;
        if (strcmp(token, user) == 0) {
            token = strtok(NULL, ",");
            if (strstr(token, "ROOT") != NULL) {
                *is_root = true;
                }
            else if (strstr(token, "ADMIN") != NULL) {
                *is_admin = true;
                }
            break;
            }
        }

    fclose(auth_file);
    return true;
    }

// Helper function to update the auth file with the banned user
bool update_auth_file(const char* channel, const char* target, char* error_message) {
    char auth_file_path[256];
    snprintf(auth_file_path, sizeof(auth_file_path), "%s/%s/admin/auth.csv", path, channel);
    FILE* auth_file = fopen(auth_file_path, "r");
    if (!auth_file) {
        strcpy(error_message, "Gagal membuka file auth.csv");
        return false;
        }

    char temp_file_path[256];
    snprintf(temp_file_path, sizeof(temp_file_path), "%s/%s/admin/auth_temp.csv", path, channel);
    FILE* temp_file = fopen(temp_file_path, "w+");
    if (!temp_file) {
        strcpy(error_message, "Gagal membuat file sementara");
        fclose(auth_file);
        return false;
        }

    bool user_found = false;
    bool can_ban = true;
    char line[256];

    while (fgets(line, sizeof(line), auth_file)) {
        char* user_id = strtok(line, ",");
        char* user_name = strtok(NULL, ",");
        char* role = strtok(NULL, ",");

        if (user_name && strcmp(user_name, target) == 0) {
            user_found = true;
            if (strstr(role, "ROOT") != NULL || strstr(role, "ADMIN") != NULL) {
                can_ban = false;
                }
            else {
                fprintf(temp_file, "%s,%s,BANNED\n", user_id, user_name);
                continue;
                }
            }
        fprintf(temp_file, "%s,%s,%s", user_id, user_name, role);
        }

    fclose(auth_file);
    fclose(temp_file);

    if (!user_found) {
        remove(temp_file_path);
        strcpy(error_message, "User tidak ditemukan di channel ini");
        return false;
        }

    if (!can_ban) {
        remove(temp_file_path);
        strcpy(error_message, "Anda tidak bisa melakukan ban pada ROOT atau ADMIN");
        return false;
        }

    remove(auth_file_path);
    rename(temp_file_path, auth_file_path);
    return true;
    }

// Main function to ban a user
void ban_user(const char* channel, const char* target, connection_t* conn) {
    bool is_admin = false;
    bool is_root = false;

    if (!check_user_privileges(conn->userLogged, channel, &is_admin, &is_root)) {
        sendErrorResponse(conn, "Gagal membuka file auth.csv");
        return;
        }

    if (!is_admin && !is_root) {
        sendErrorResponse(conn, "Anda tidak memiliki izin untuk melakukan ban user");
        return;
        }

    char error_message[100];
    if (!update_auth_file(channel, target, error_message)) {
        sendErrorResponse(conn, error_message);
        return;
        }

    char response[100];
    snprintf(response, sizeof(response), "%s diban", target);
    if (write(conn->sock, response, strlen(response)) < 0) {
        perror("Gagal mengirim respons ke client");
        }

    char log_message[100];
    snprintf(log_message, sizeof(log_message), "%s ban %s", is_root ? "ROOT" : "ADMIN", target);
    log_activity(channel, log_message);
    }


// Helper function to update the auth file with the unbanned user
bool update_auth_file_with_unban(const char* channel, const char* target, char* error_message) {
    char auth_file_path[256];
    snprintf(auth_file_path, sizeof(auth_file_path), "%s/%s/admin/auth.csv", path, channel);
    FILE* auth_file = fopen(auth_file_path, "r");
    if (!auth_file) {
        strcpy(error_message, "Gagal membuka file auth.csv");
        return false;
        }

    char temp_file_path[256];
    snprintf(temp_file_path, sizeof(temp_file_path), "%s/%s/admin/auth_temp.csv", path, channel);
    FILE* temp_file = fopen(temp_file_path, "w+");
    if (!temp_file) {
        strcpy(error_message, "Gagal membuat file sementara");
        fclose(auth_file);
        return false;
        }

    bool user_found = false;
    char line[256];

    while (fgets(line, sizeof(line), auth_file)) {
        char* user_id = strtok(line, ",");
        char* user_name = strtok(NULL, ",");
        char* role = strtok(NULL, ",");

        if (user_name && strcmp(user_name, target) == 0) {
            user_found = true;
            if (strstr(role, "BANNED") == NULL) {
                strcpy(error_message, "User tidak dalam status banned");
                fclose(auth_file);
                fclose(temp_file);
                remove(temp_file_path);
                return false;
                }
            else {
                fprintf(temp_file, "%s,%s,USER\n", user_id, user_name);
                continue;
                }
            }
        fprintf(temp_file, "%s,%s,%s", user_id, user_name, role);
        }

    fclose(auth_file);
    fclose(temp_file);

    if (!user_found) {
        remove(temp_file_path);
        strcpy(error_message, "User tidak ditemukan di channel ini");
        return false;
        }

    remove(auth_file_path);
    rename(temp_file_path, auth_file_path);
    return true;
    }

// Main function to unban a user
void unban_user(const char* channel, const char* target, connection_t* conn) {
    bool is_admin = false;
    bool is_root = false;

    if (!check_user_privileges(conn->userLogged, channel, &is_admin, &is_root)) {
        sendErrorResponse(conn, "Gagal membuka file auth.csv");
        return;
        }

    if (!is_admin && !is_root) {
        sendErrorResponse(conn, "Anda tidak memiliki izin untuk melakukan unban user");
        return;
        }

    char error_message[100];
    if (!update_auth_file_with_unban(channel, target, error_message)) {
        sendErrorResponse(conn, error_message);
        return;
        }

    char response[100];
    snprintf(response, sizeof(response), "%s kembali", target);
    if (write(conn->sock, response, strlen(response)) < 0) {
        perror("Gagal mengirim respons ke client");
        }

    char log_message[100];
    snprintf(log_message, sizeof(log_message), "%s unban %s", is_root ? "ROOT" : "ADMIN", target);
    log_activity(channel, log_message);
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

void remove_user_entry(const char* username, const char* temp_file_path, connection_t* conn) {
    FILE* user_file = fopen(path_user, "r");
    if (!user_file) {
        sendErrorResponse(conn, "Gagal membuka file users.csv");
        return;
        }

    FILE* temp_file = fopen(temp_file_path, "w+");
    if (!temp_file) {
        sendErrorResponse(conn, "Gagal membuat file sementara");
        fclose(user_file);
        return;
        }

    char buffer[256];
    bool user_found = false;

    while (fgets(buffer, sizeof(buffer), user_file)) {
        char* id = strtok(buffer, ",");
        char* name = strtok(NULL, ",");
        char* password_hash = strtok(NULL, ",");
        char* user_role = strtok(NULL, ",");

        if (name && strcmp(name, username) == 0) {
            user_found = true;
            continue;  // Skip writing this user to temp file
            }
        fprintf(temp_file, "%s,%s,%s,%s", id, name, password_hash, user_role);
        }

    fclose(user_file);
    fclose(temp_file);

    if (user_found) {
        remove(path_user);
        rename(temp_file_path, path_user);
        char success_msg[100];
        snprintf(success_msg, sizeof(success_msg), "%s berhasil dihapus", username);
        if (write(conn->sock, success_msg, strlen(success_msg)) < 0) {
            perror("Gagal mengirim respons ke client");
            }
        }
    else {
        remove(temp_file_path);
        sendErrorResponse(conn, "User tidak ditemukan");
        }
    }

bool is_logged_user_root(const char* logged_user) {
    FILE* user_file = fopen(path_user, "r");
    if (!user_file) {
        return false;
        }

    char buffer[256];
    bool is_root = false;

    while (fgets(buffer, sizeof(buffer), user_file)) {
        char* id = strtok(buffer, ",");
        char* name = strtok(NULL, ",");
        if (name && strcmp(name, logged_user) == 0) {
            char* password_hash = strtok(NULL, ",");
            char* user_role = strtok(NULL, ",");
            if (strstr(user_role, "ROOT") != NULL) {
                is_root = true;
                break;
                }
            }
        }

    fclose(user_file);
    return is_root;
    }

void remove_root(const char* target_user, connection_t* conn) {
    if (!is_logged_user_root(conn->userLogged)) {
        sendErrorResponse(conn, "Anda tidak memiliki izin untuk menghapus user secara permanen");
        return;
        }

    char temp_file_path[256];
    snprintf(temp_file_path, sizeof(temp_file_path), "%s/users_temp.csv", path);
    remove_user_entry(target_user, temp_file_path, conn);
    }

void verifyKey(const char* user, const char* channel, const char* key, connection_t* conn) {
    FILE* channelsFile = fopen(path_channels, "r");
    if (!channelsFile) {
        sendErrorResponse(conn, "Error opening channels.csv\n");
        return;
        }

    char line[MAX_LEN];
    char storedHash[MAX_LEN];
    bool keyValid = false;

    while (fgets(line, sizeof(line), channelsFile)) {
        char* token = strtok(line, ",");
        token = strtok(NULL, ",");
        if (token && strcmp(token, channel) == 0) {
            token = strtok(NULL, ",");
            strcpy(storedHash, token);
            storedHash[strcspn(storedHash, "\n")] = 0;
            if (token && !strcmp(crypt(key, storedHash), storedHash)) {
                keyValid = true;
                break;
                }
            }
        }

    fclose(channelsFile);

    if (keyValid) {
        FILE* usersFile = fopen(path_user, "r");
        if (!usersFile) {
            sendErrorResponse(conn, "Error opening users.csv\n");
            return;
            }

        char userLine[MAX_LEN];
        char userId[10];
        bool userFound = false;

        while (fgets(userLine, sizeof(userLine), usersFile)) {
            char* token = strtok(userLine, ",");
            strcpy(userId, token);
            token = strtok(NULL, ",");
            if (token && strcmp(token, user) == 0) {
                userFound = true;
                break;
                }
            }

        fclose(usersFile);

        if (userFound) {
            char authPath[256];
            snprintf(authPath, sizeof(authPath), "%s/%s/admin/auth.csv", path, channel);
            FILE* authFile = fopen(authPath, "a");
            if (authFile) {
                fprintf(authFile, "%s,%s,USER\n", userId, user);
                fclose(authFile);

                snprintf(conn->userLogged, sizeof(conn->userLogged), "%s", user);
                char response[BUFFER_SIZE];
                snprintf(response, sizeof(response), "[%s/%s]", user, channel);
                if (write(conn->sock, response, strlen(response)) < 0) {
                    perror("Response send failed");
                    }
                }
            else {
                sendErrorResponse(conn, "Error opening auth.csv\n");
                }
            }
        else {
            sendErrorResponse(conn, "User not found\n");
            }
        }
    else {
        sendErrorResponse(conn, "Wrong key\n");
        }
    }

void exit_func(connection_t* conn) {
    if (strlen(conn->roomLogged) > 0) {
        memset(conn->roomLogged, 0, sizeof(conn->roomLogged));
        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response), "[%s/%s]", conn->userLogged, conn->channelLogged);
        if (write(conn->sock, response, strlen(response)) < 0) {
            perror("Response send failed");
            }
        }
    else if (strlen(conn->channelLogged) > 0) {
        memset(conn->channelLogged, 0, sizeof(conn->channelLogged));
        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response), "[%s]", conn->userLogged);
        if (write(conn->sock, response, strlen(response)) < 0) {
            perror("Response send failed");
            }
        }
    else {
        char response[] = "You have exited the program";
        if (write(conn->sock, response, strlen(response)) < 0) {
            perror("Response send failed");
            }
        close(conn->sock);
        pthread_exit(NULL);
        }
    }

// ROOT

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
        else if (strcmp(token, "CREATE") == 0) {
            token = strtok(NULL, " ");
            if (strcmp(token, "CHANNEL") == 0) {
                char* channel_name = strtok(NULL, " ");
                token = strtok(NULL, " ");
                char* key = strtok(NULL, " ");
                new_channel(conn->userLogged, channel_name, key, conn);
                }
            else if (strcmp(token, "ROOM") == 0) {
                char* room = strtok(NULL, " ");
                new_room(conn->userLogged, conn->channelLogged, room, conn);
                }
            else {
                resp = "Invalid command\n";
                if (send(conn->sock, resp, strlen(resp), 0) != strlen(resp)) {
                    perror("Send failed");
                    }
                }
            }
        else if (strcmp(token, "LIST") == 0) {
            token = strtok(NULL, " ");
            if (strcmp(token, "CHANNEL") == 0) {
                list_channel(conn);
                }
            else if (strcmp(token, "ROOM") == 0) {
                list_room(conn->channelLogged, conn);
                }
            else if (strcmp(token, "USER") == 0) {
                list_user(conn->channelLogged, conn);
                }
            else {
                resp = "Invalid command\n";
                if (send(conn->sock, resp, strlen(resp), 0) != strlen(resp)) {
                    perror("Send failed");
                    }
                }
            }
        else if (strcmp(token, "JOIN") == 0) {
            token = strtok(NULL, " ");
            if (strlen(conn->channelLogged) == 0) join_channel(conn->userLogged, token, conn);
            else if (token == NULL) {
                resp = "Invalid command\n";
                if (send(conn->sock, resp, strlen(resp), 0) != strlen(resp)) {
                    perror("Send failed");
                    }
                }
            else join_room(conn->channelLogged, token, conn);
            }
        else if (strcmp(token, "CHAT") == 0) {
            char* message = buf + 5;

            if (conn->channelLogged == NULL || conn->roomLogged == NULL) {
                resp = "You are not in a channel or room\n";
                if (send(conn->sock, resp, strlen(resp), 0) != strlen(resp)) {
                    perror("Send failed");
                    }
                continue;
                }
            send_message(conn->userLogged, conn->channelLogged, conn->roomLogged, message, conn);
            }
        else if (strcmp(token, "SEE") == 0) {
            token = strtok(NULL, " ");
            if (strcmp(token, "CHAT") == 0) {
                see_messages(conn->channelLogged, conn->roomLogged, conn);
                }
            else {
                resp = "Invalid command\n";
                if (send(conn->sock, resp, strlen(resp), 0) != strlen(resp)) {
                    perror("Send failed");
                    }
                }
            }
        else if (strcmp(token, "EDIT") == 0) {
            token = strtok(NULL, " ");
            if (token == NULL) {
                sendErrorResponse(conn, "Invalid command\n");
                continue;
                }
            if (strcmp(token, "CHAT") == 0) {
                token = strtok(NULL, " ");
                if (token == NULL) {
                    sendErrorResponse(conn, "Invalid command\n");
                    continue;
                    }
                char* message_id = token;
                token = strtok(NULL, " ");
                if (token == NULL) {
                    sendErrorResponse(conn, "Invalid command\n");
                    continue;
                    }
                edit_message(conn->channelLogged, conn->roomLogged, atoi(message_id), token, conn);
                }
            else if (strcmp(token, "CHANNEL") == 0) {
                token = strtok(NULL, " ");
                if (token == NULL) {
                    sendErrorResponse(conn, "Invalid command\n");
                    continue;
                    }
                char* old_channel = token;
                token = strtok(NULL, " ");
                token = strtok(NULL, " ");
                edit_channel(old_channel, token, conn);
                }
            else if (strcmp(token, "ROOM") == 0) {
                token = strtok(NULL, " ");
                char* old_room = token;
                token = strtok(NULL, " ");
                token = strtok(NULL, " ");
                if (token == NULL) {
                    sendErrorResponse(conn, "Invalid command\n");
                    continue;
                    }
                edit_room(conn->channelLogged, old_room, token, conn);
                }
            // else if (strcmp(token, "PROFILE") == 0) {
            //     token = strtok(NULL, " ");
            //     if (token == NULL) {
            //         sendErrorResponse(conn, "Invalid command\n");
            //         continue;
            //         }
            //     edit_profile(conn->userLogged, token, conn);
            //     }
            else {
                sendErrorResponse(conn, "Invalid command\n");
                }
            }
        else if (strcmp(token, "DEL") == 0) {
            token = strtok(NULL, " ");
            if (token == NULL) {
                sendErrorResponse(conn, "Invalid command\n");
                continue;
                }
            if (strcmp(token, "CHAT") == 0) {
                token = strtok(NULL, " ");
                if (token == NULL) {
                    sendErrorResponse(conn, "Invalid command\n");
                    continue;
                    }
                remove_message(conn->channelLogged, conn->roomLogged, atoi(token), conn);
                }
            else if (strcmp(token, "CHANNEL") == 0) {
                char* channel = strtok(NULL, " ");
                if (channel == NULL) {
                    sendErrorResponse(conn, "Invalid command\n");
                    continue;
                    }
                remove_channel(channel, conn);
                }
            else if (strcmp(token, "ROOM") == 0) {
                token = strtok(NULL, " ");
                if (token == NULL) {
                    sendErrorResponse(conn, "Invalid command\n");
                    continue;
                    }
                if (strcmp(token, "ALL") == 0) {
                    remove_all_room(conn->channelLogged, conn);
                    }
                else {
                    remove_room(conn->channelLogged, token, conn);
                    }
                }
            }
        else if (strcmp(token, "BAN") == 0) {
            token = strtok(NULL, " ");
            if (token == NULL) {
                sendErrorResponse(conn, "Invalid command\n");
                continue;
                }
            ban_user(conn->channelLogged, token, conn);
            }
        else if (strcmp(token, "UNBAN") == 0) {
            token = strtok(NULL, " ");
            if (token == NULL) {
                sendErrorResponse(conn, "Invalid command\n");
                continue;
                }
            unban_user(conn->channelLogged, token, conn);
            }
        else if (strcmp(token, "REMOVE") == 0) {
            token = strtok(NULL, " ");
            if (token == NULL) {
                sendErrorResponse(conn, "Invalid command\n");
                continue;
                }
            if (strcmp(token, "USER") == 0) {
                token = strtok(NULL, " ");
                if (token == NULL) {
                    sendErrorResponse(conn, "Invalid command\n");
                    continue;
                    }
                remove_user(conn->channelLogged, token, conn);
                }
            }
        else if (strcmp(token, "ROOT") == 0) {
            token = strtok(NULL, " ");
            if (token == NULL) {
                sendErrorResponse(conn, "Invalid command\n");
                continue;
                }
            remove_root(token, conn);
            }
        else if (strcmp(token, "EXIT") == 0) {
            exit_func(conn);
            }
        else {
            resp = "Invalid command\n";
            send_response(conn, resp);
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