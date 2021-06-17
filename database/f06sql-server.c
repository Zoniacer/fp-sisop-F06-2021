#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <libgen.h>
#define PORT 1111
#define FAIL_OR_SUCCESS_LENGTH 10
#define MAX_CREDENTIALS_LENGTH 100
#define MAX_INFORMATION_LENGTH 200
const char failMsg[] = "false";
const char successMsg[] = "true";
char defaultDB[] = "f06database";
char defaultUsersTable[] = "UsersTable";

bool isFileExists(char filename[]) {
    return access(filename, F_OK) == 0;
}

bool isFolderExists(char foldername[]) {
    DIR * dir = opendir("FILES");
    return dir != NULL;
}

bool setupServer(int listen_port, int * server_fd, struct sockaddr_in * address) {
    int valread;
    int opt = 1;
    bool success = true;
    if ((*server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    success &= setsockopt(*server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) == 0;

    address->sin_family = AF_INET;
    address->sin_addr.s_addr = INADDR_ANY;
    address->sin_port = htons( listen_port );
      
    success &= bind(*server_fd, (const struct sockaddr *)address, sizeof(*address)) >= 0;

    return success & (listen(*server_fd, 10) == 0);
}

bool createFile(char filename[]) {
    FILE * file = fopen(filename, "a");
    if(file) {
        fclose(file);
        return true;
    }
    return false;
}

bool acceptConnection(int * new_socket, int * server_fd, struct sockaddr_in * address) {
    int addrlen = sizeof(address);
    return (*new_socket = accept(*server_fd, (struct sockaddr *)address, (socklen_t*)&addrlen)) != -1;
}

bool authentication(int socket, char authenticatedUser[]) {
    char username[MAX_CREDENTIALS_LENGTH], password[MAX_CREDENTIALS_LENGTH];
    memset(username, 0, MAX_CREDENTIALS_LENGTH);
    memset(password, 0, MAX_CREDENTIALS_LENGTH);
    read(socket, username, MAX_CREDENTIALS_LENGTH);
    read(socket, password, MAX_CREDENTIALS_LENGTH);
    printf("Username: %s\n", username);
    printf("Password: %s\n", password);
    if(strcmp(username, "root") == 0) {
        strcpy(authenticatedUser, "root");
        return true;
    }
    chdir(defaultDB);
    FILE * usersTable = fopen(defaultUsersTable, "r");
    char usernameInDB[MAX_CREDENTIALS_LENGTH], passwordInDB[MAX_CREDENTIALS_LENGTH];
    while(fscanf(usersTable, "%s %s", usernameInDB, passwordInDB) != EOF) {
        if(strcmp(username, usernameInDB) == 0 && strcmp(password, passwordInDB) == 0) {
            strcpy(authenticatedUser, username);
            fclose(usersTable);
            chdir("../");
            return true;
        }
    }
    fclose(usersTable);
    printf("User %s gagal login\n", username);
    chdir("../");
    return false;
}

void createUser(int socket, char authenticatedUser[]) {
    char username[MAX_CREDENTIALS_LENGTH], password[MAX_CREDENTIALS_LENGTH];
    memset(username, 0, MAX_CREDENTIALS_LENGTH);
    memset(password, 0, MAX_CREDENTIALS_LENGTH);
    read(socket, username, MAX_CREDENTIALS_LENGTH);
    read(socket, password, MAX_CREDENTIALS_LENGTH);
    if(strcmp(authenticatedUser, "root") != 0) {
        send(socket, failMsg, FAIL_OR_SUCCESS_LENGTH, 0);
        return;
    }
    chdir(defaultDB);
    FILE * usersTable = fopen(defaultUsersTable, "a");
    if(usersTable == NULL) {
        chdir("../");
        send(socket, failMsg, FAIL_OR_SUCCESS_LENGTH, 0);
        return;
    }
    fprintf(usersTable, "%s %s\n", username, password);
    fclose(usersTable);
    chdir("../");
    send(socket, successMsg, FAIL_OR_SUCCESS_LENGTH, 0);
}

void * app(void * vargp) {
    int socket = *((int *)vargp);
    char authenticatedUser[MAX_CREDENTIALS_LENGTH];
    memset(authenticatedUser, 0, MAX_CREDENTIALS_LENGTH);
    if(!authentication(socket, authenticatedUser)) {
        send(socket, failMsg, FAIL_OR_SUCCESS_LENGTH, 0);
        return NULL;
    }
    send(socket, successMsg, FAIL_OR_SUCCESS_LENGTH, 0);
    printf("User %s authenticated\n", authenticatedUser);
    while(true) {
        char action[MAX_INFORMATION_LENGTH];
        memset(action, 0, MAX_INFORMATION_LENGTH);
        int readStatus = read(socket, action, MAX_INFORMATION_LENGTH);
        if(readStatus <= 0)
            return NULL;
        if(strcmp(action, "createUser") == 0)
            createUser(socket, authenticatedUser);
    }
}

int main(int argc, char * argv[]) {
    struct sockaddr_in address;
    int server_fd;
    pthread_t tid;

    if(!setupServer(PORT, &server_fd, &address)) {
        printf("Gagal membuat server\n");
        exit(EXIT_FAILURE);
    }

    chdir(dirname(argv[0]));

    if(!isFolderExists(defaultDB)) mkdir(defaultDB, S_IRWXU);
    chdir(defaultDB);
    if(!isFileExists(defaultUsersTable)) createFile(defaultUsersTable);
    chdir("../");

    while(true) {
        int new_socket;
        if(!acceptConnection(&new_socket, &server_fd, &address)) {
            printf("Tidak dapat menerima koneksi.\n");
            continue;
        }
        printf("Koneksi baru diterima.\n");
        send(new_socket, successMsg, FAIL_OR_SUCCESS_LENGTH, 0);
        pthread_create(&tid, NULL, app, (void *)&new_socket);
        pthread_join(tid, NULL);
        close(new_socket);
        printf("Koneksi selesai.\n");
    }
    return 0;
}