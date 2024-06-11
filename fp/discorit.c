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
#define path "/home/etern1ty/sisop_works/FP/fp/DiscorIT"
#define path_user "/home/etern1ty/sisop_works/FP/fp/DiscorIT/users.csv"
#define path_channels "/home/etern1ty/sisop_works/FP/fp/DiscorIT/channels.csv"

void make_folder(char *folder_path) {
    struct stat st = {0};
    if (stat(folder_path, &st) == -1) {
        mkdir(folder_path, 0777);
    }
}

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