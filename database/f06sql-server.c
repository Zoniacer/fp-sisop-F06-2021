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
char accessDBFileName[] = "_AccessTable";
char currentDatabase[MAX_INFORMATION_LENGTH];
char currentPath[MAX_INFORMATION_LENGTH];

bool isFileExists(char filename[]) {
    return access(filename, F_OK) == 0;
}

bool isFolderExists(char foldername[]) {
    DIR * dir = opendir(foldername);
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

void logging(char authenticatedUser[], char command[]) {
    FILE * logFile = fopen("db.log", "a");
    if(logFile == NULL) return;
    time_t now = time(NULL);
    struct tm * nowLocalTM = localtime(&now);
    char formattedTime[MAX_INFORMATION_LENGTH];
    strftime(formattedTime, sizeof formattedTime, "%Y-%m-%d %H:%M:%S", nowLocalTM);
    fprintf(logFile, "%s:%s:%s\n", formattedTime, authenticatedUser, command);
    fclose(logFile);
}

bool isUserExists(char username[]) {
    chdir(defaultDB);
    FILE * usersTable = fopen(defaultUsersTable, "r");
    char usernameInDB[MAX_CREDENTIALS_LENGTH];
    while(fscanf(usersTable, "%s %*s", usernameInDB) != EOF) {
        if(strcmp(username, usernameInDB) == 0) {
            fclose(usersTable);
            chdir("../");
            return true;
        }
    }
    fclose(usersTable);
    chdir("../");
    return false;
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

bool isRoot(char authenticatedUser[]) {
    return strcmp(authenticatedUser, "root") == 0;
}

void createUser(int socket, char authenticatedUser[]) {
    char username[MAX_CREDENTIALS_LENGTH], password[MAX_CREDENTIALS_LENGTH];
    memset(username, 0, MAX_CREDENTIALS_LENGTH);
    memset(password, 0, MAX_CREDENTIALS_LENGTH);
    read(socket, username, MAX_CREDENTIALS_LENGTH);
    read(socket, password, MAX_CREDENTIALS_LENGTH);
    char command[1000];
    sprintf(command, "CREATE USER %s IDENTIFIED BY %s", username, password);
    logging(authenticatedUser, command);
    if(!isRoot(authenticatedUser)) {
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

// void createDatabase();

void useDatabaseSuccess(int socket, char dbName[]) {
    strcpy(currentDatabase, dbName);
    send(socket, successMsg, FAIL_OR_SUCCESS_LENGTH, 0);
}

void useDatabase(int socket, char authenticatedUser[]) {
    char dbName[MAX_CREDENTIALS_LENGTH];
    memset(dbName, 0, MAX_CREDENTIALS_LENGTH);
    read(socket, dbName, MAX_CREDENTIALS_LENGTH);
    char command[1000];
    sprintf(command, "USE %s", dbName);
    logging(authenticatedUser, command);
    printf("%s\n", dbName);
    if(!isFolderExists(dbName)) {
        send(socket, failMsg, FAIL_OR_SUCCESS_LENGTH, 0);
        char reason[MAX_INFORMATION_LENGTH];
        sprintf(reason, "DB %s tidak ditemukan.", dbName);
        send(socket, reason, MAX_INFORMATION_LENGTH, 0);
        return;
    }
    if(isRoot(authenticatedUser)) {
        useDatabaseSuccess(socket, dbName);
        return;
    }
    chdir(dbName);
    FILE * accessTable = fopen(accessDBFileName, "r");
    char authorizedUser[MAX_CREDENTIALS_LENGTH];
    while(fscanf(accessTable, "%s", authorizedUser) != EOF) {
        if(strcmp(authorizedUser, authenticatedUser) == 0) {
            useDatabaseSuccess(socket, dbName);
            chdir("../");
            return;
        }
    }
    send(socket, failMsg, FAIL_OR_SUCCESS_LENGTH, 0);
    char reason[MAX_INFORMATION_LENGTH];
    sprintf(reason, "Anda tidak berhak mengakses DB %s", dbName);
    send(socket, reason, MAX_INFORMATION_LENGTH, 0);
    fclose(accessTable);
    chdir("../");
}

void grantDatabaseToUser(int socket, char authenticatedUser[]) {
    char dbName[MAX_CREDENTIALS_LENGTH], username[MAX_CREDENTIALS_LENGTH];
    memset(dbName, 0, MAX_CREDENTIALS_LENGTH);
    read(socket, dbName, MAX_CREDENTIALS_LENGTH);
    memset(username, 0, MAX_CREDENTIALS_LENGTH);
    read(socket, username, MAX_CREDENTIALS_LENGTH);
    char command[1000];
    sprintf(command, "GRANT PERMISSION %s INTO %s", dbName, username);
    logging(authenticatedUser, command);
    if(!isRoot(authenticatedUser)) {
        send(socket, failMsg, FAIL_OR_SUCCESS_LENGTH, 0);
        char reason[MAX_INFORMATION_LENGTH];
        sprintf(reason, "Non-root tidak berhak memberikan akses database.");
        send(socket, reason, MAX_INFORMATION_LENGTH, 0);
        return;
    }
    if(!isFolderExists(dbName)) {
        send(socket, failMsg, FAIL_OR_SUCCESS_LENGTH, 0);
        char reason[MAX_INFORMATION_LENGTH];
        sprintf(reason, "DB %s tidak ditemukan.", dbName);
        send(socket, reason, MAX_INFORMATION_LENGTH, 0);
        return;
    }
    if(!isUserExists(username)) {
        send(socket, failMsg, FAIL_OR_SUCCESS_LENGTH, 0);
        char reason[MAX_INFORMATION_LENGTH];
        sprintf(reason, "User %s tidak ditemukan.", username);
        send(socket, reason, MAX_INFORMATION_LENGTH, 0);
        return;
    }
    chdir(dbName);
    getcwd(currentPath, sizeof currentPath);
    puts(currentPath);
    FILE * accessTable = fopen(accessDBFileName, "a");
    fprintf(accessTable, "%s\n", username);
    fclose(accessTable);
    send(socket, successMsg, FAIL_OR_SUCCESS_LENGTH, 0);
    chdir("../");
}

void createDatabase(int socket, char authenticatedUser[]) {
    char dbName[MAX_CREDENTIALS_LENGTH];
    memset(dbName, 0, MAX_CREDENTIALS_LENGTH);
    read(socket, dbName, MAX_CREDENTIALS_LENGTH);
    char command[1000];
    sprintf(command, "CREATE DATABASE %s", dbName);
    logging(authenticatedUser, command);
    if(isFolderExists(dbName)) {
        send(socket, failMsg, FAIL_OR_SUCCESS_LENGTH, 0);
        char reason[MAX_INFORMATION_LENGTH];
        sprintf(reason, "DB %s sudah ada.", dbName);
        send(socket, reason, MAX_INFORMATION_LENGTH, 0);
        return;
    }
    mkdir(dbName, S_IRWXU);
    chdir(dbName);
    FILE * accessTable = fopen(accessDBFileName, "w");
    if(isRoot(authenticatedUser)) {
        send(socket, successMsg, FAIL_OR_SUCCESS_LENGTH, 0);
        fclose(accessTable);
        chdir("../");
        return;
    }
    fprintf(accessTable, "%s\n", authenticatedUser);
    send(socket, successMsg, FAIL_OR_SUCCESS_LENGTH, 0);
    fclose(accessTable);
    chdir("../");
}

int remove_directory(const char *path) {
   DIR *d = opendir(path);
   size_t path_len = strlen(path);
   int r = -1;

   if (d) {
      struct dirent *p;

      r = 0;
      while (!r && (p=readdir(d))) {
          int r2 = -1;
          char *buf;
          size_t len;

          /* Skip the names "." and ".." as we don't want to recurse on them. */
          if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
             continue;

          len = path_len + strlen(p->d_name) + 2; 
          buf = malloc(len);

          if (buf) {
             struct stat statbuf;

             snprintf(buf, len, "%s/%s", path, p->d_name);
             if (!stat(buf, &statbuf)) {
                if (S_ISDIR(statbuf.st_mode))
                   r2 = remove_directory(buf);
                else
                   r2 = unlink(buf);
             }
             free(buf);
          }
          r = r2;
      }
      closedir(d);
   }

   if (!r)
      r = rmdir(path);

   return r;
}

void dropDatabase(int socket, char authenticatedUser[]) {
    char dbName[MAX_CREDENTIALS_LENGTH];
    memset(dbName, 0, MAX_CREDENTIALS_LENGTH);
    read(socket, dbName, MAX_CREDENTIALS_LENGTH);
    char command[1000];
    sprintf(command, "DROP DATABASE %s", dbName);
    logging(authenticatedUser, command);
    if(!isFolderExists(dbName)) {
        send(socket, failMsg, FAIL_OR_SUCCESS_LENGTH, 0);
        char reason[MAX_INFORMATION_LENGTH];
        sprintf(reason, "DB %s tidak ada.", dbName);
        send(socket, reason, MAX_INFORMATION_LENGTH, 0);
        return;
    }
    if(isRoot(authenticatedUser)) {
        remove_directory(dbName);
        send(socket, successMsg, FAIL_OR_SUCCESS_LENGTH, 0);
        return;
    }
    chdir(dbName);
    FILE * accessTable = fopen(accessDBFileName, "r");
    char authorizedUser[MAX_CREDENTIALS_LENGTH];
    while(fscanf(accessTable, "%s", authorizedUser) != EOF) {
        if(strcmp(authorizedUser, authenticatedUser) == 0) {
            fclose(accessTable);
            chdir("../");
            remove_directory(dbName);
            send(socket, successMsg, FAIL_OR_SUCCESS_LENGTH, 0);
            return;
        }
    }
    send(socket, failMsg, FAIL_OR_SUCCESS_LENGTH, 0);
    char reason[MAX_INFORMATION_LENGTH];
    sprintf(reason, "Anda tidak berhak menghapus DB %s", dbName);
    send(socket, reason, MAX_INFORMATION_LENGTH, 0);
    fclose(accessTable);
    chdir("../");
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
        else if(strcmp(action, "useDatabase") == 0)
            useDatabase(socket, authenticatedUser);
        else if(strcmp(action, "grantDatabaseToUser") == 0)
            grantDatabaseToUser(socket, authenticatedUser);
        else if(strcmp(action, "createDatabase") == 0)
            createDatabase(socket, authenticatedUser);
        else if(strcmp(action, "dropDatabase") == 0)
            dropDatabase(socket, authenticatedUser);
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
    if(!isFileExists(accessDBFileName)) createFile(accessDBFileName);
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