#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdbool.h>
#include <libgen.h>
#define PORT 1111
#define FAIL_OR_SUCCESS_LENGTH 10
#define MAX_INFORMATION_LENGTH 200
#define MAX_CREDENTIALS_LENGTH 100
const char failMsg[] = "false";
const char successMsg[] = "true";
char username[MAX_CREDENTIALS_LENGTH], password[MAX_CREDENTIALS_LENGTH];

bool setupClient(int * sock) {
    struct sockaddr_in address;
    struct sockaddr_in serv_addr;
    if ((*sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return false;
    }
  
    memset(&serv_addr, '0', sizeof(serv_addr));
  
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
      
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) {
        printf("\nInvalid address/ Address not supported \n");
        return false;
    }
  
    if (connect(*sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return false;
    }
    return true;
}

bool authentication(int sock) {
    if(strcmp(username, "root") == 0) {
        printf("Login menggunakan root hanya dapat dilakukan dengan sudo tanpa memberikan username dan password\n");
        return false;
    }
    if(getuid() == 0) strcpy(username, "root");
    if(strcmp(username, "") == 0 && strcmp(password, "") == 0) {
        printf("Masukkan username dan password\n");
        return false;
    }
    printf("Username: %s\n", username);
    printf("Password: %s\n", password);
    send(sock, username, MAX_CREDENTIALS_LENGTH, 0);
    send(sock, password, MAX_CREDENTIALS_LENGTH, 0);
    char message[FAIL_OR_SUCCESS_LENGTH];
    memset(message, 0, FAIL_OR_SUCCESS_LENGTH);
    read(sock, message, FAIL_OR_SUCCESS_LENGTH);
    if(strcmp(failMsg, message) == 0) {
        printf("Username atau password salah\n");
        return false;
    }
    return true;
}

bool parseArgs(int argc, char const *argv[]) {
    int i=1;
    for(; i<argc; i += 2) {
        if(strcmp(argv[i], "-u") == 0 && i + 1 < argc) {
            strcpy(username, argv[i + 1]);
            continue;
        }
        if(strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            strcpy(password, argv[i + 1]);
            continue;
        }
        printf("Argumen tidak diformat dengan benar");
    }
}

void printSemicolonError() {
    printf("Syntax error, missing semicolon at the end of query.\n");
}

void createUser(int socket, char fullQuery[]) {
    if(strcmp(username, "root") != 0) {
        printf("Non-root user tidak berhak membuat user baru.\n");
        return;
    }
    const char command[] = "createUser";
    char newUser[MAX_CREDENTIALS_LENGTH];
    char newPassword[MAX_CREDENTIALS_LENGTH];
    char response[FAIL_OR_SUCCESS_LENGTH];
    sscanf(fullQuery, "%*s %*s %s %*s %*s %s", newUser, newPassword);
    if(newPassword[strlen(newPassword) - 1] != ';') {
        printSemicolonError();
        return;
    }
    newPassword[strlen(newPassword) - 1] = '\0';
    send(socket, command, MAX_INFORMATION_LENGTH, 0);
    send(socket, newUser, MAX_CREDENTIALS_LENGTH, 0);
    send(socket, newPassword, MAX_CREDENTIALS_LENGTH, 0);
    read(socket, response, FAIL_OR_SUCCESS_LENGTH);
    if(strcmp(response, successMsg) == 0) printf("Berhasil membuat user baru.\n");
    else printf("Gagal membuat user baru.\n");
}

void useDatabase(int socket, char fullQuery[]) {
    char dbName[MAX_CREDENTIALS_LENGTH];
    sscanf(fullQuery, "%*s %s", dbName);
    if(dbName[strlen(dbName) - 1] != ';') {
        printSemicolonError();
        return;
    }
    dbName[strlen(dbName) - 1] = '\0';
    const char command[] = "useDatabase";
    char response[FAIL_OR_SUCCESS_LENGTH];
    send(socket, command, MAX_INFORMATION_LENGTH, 0);
    send(socket, dbName, MAX_CREDENTIALS_LENGTH, 0);
    read(socket, response, FAIL_OR_SUCCESS_LENGTH);
    if(strcmp(response, successMsg) == 0) {
        printf("Tersambung ke database %s\n", dbName);
        return;
    }
    char reason[MAX_INFORMATION_LENGTH];
    read(socket, reason, MAX_INFORMATION_LENGTH);
    printf("%s\n", reason);
}

void grantDatabaseToUser(int socket, char fullQuery[]) {
    char dbName[MAX_CREDENTIALS_LENGTH], grantedTo[MAX_CREDENTIALS_LENGTH];
    sscanf(fullQuery, "%*s %*s %s %*s %s", dbName, grantedTo);
    if(grantedTo[strlen(grantedTo) - 1] != ';') {
        printSemicolonError();
        return;
    }
    grantedTo[strlen(grantedTo) - 1] = '\0';
    const char command[] = "grantDatabaseToUser";
    char response[FAIL_OR_SUCCESS_LENGTH];
    send(socket, command, MAX_INFORMATION_LENGTH, 0);
    send(socket, dbName, MAX_CREDENTIALS_LENGTH, 0);
    send(socket, grantedTo, MAX_CREDENTIALS_LENGTH, 0);
    read(socket, response, FAIL_OR_SUCCESS_LENGTH);
    if(strcmp(response, successMsg) == 0) {
        printf("Akses database %s diberikan ke %s\n", dbName, grantedTo);
        return;
    }
    char reason[MAX_INFORMATION_LENGTH];
    read(socket, reason, MAX_INFORMATION_LENGTH);
    printf("%s\n", reason);
}

void createDatabase(int socket, char fullQuery[]) {
    char dbName[MAX_CREDENTIALS_LENGTH];
    sscanf(fullQuery, "%*s %*s %s", dbName);
    if(dbName[strlen(dbName) - 1] != ';') {
        printSemicolonError();
        return;
    }
    dbName[strlen(dbName) - 1] = '\0';
    const char command[] = "createDatabase";
    char response[FAIL_OR_SUCCESS_LENGTH];
    send(socket, command, MAX_INFORMATION_LENGTH, 0);
    send(socket, dbName, MAX_CREDENTIALS_LENGTH, 0);
    read(socket, response, FAIL_OR_SUCCESS_LENGTH);
    if(strcmp(response, successMsg) == 0) {
        printf("Database %s berhasil dibuat\n", dbName);
        return;
    }
    char reason[MAX_INFORMATION_LENGTH];
    read(socket, reason, MAX_INFORMATION_LENGTH);
    printf("%s\n", reason);
}

void dropDatabase(int socket, char fullQuery[]) {
    char dbName[MAX_CREDENTIALS_LENGTH];
    sscanf(fullQuery, "%*s %*s %s", dbName);
    if(dbName[strlen(dbName) - 1] != ';') {
        printSemicolonError();
        return;
    }
    dbName[strlen(dbName) - 1] = '\0';
    const char command[] = "dropDatabase";
    char response[FAIL_OR_SUCCESS_LENGTH];
    send(socket, command, MAX_INFORMATION_LENGTH, 0);
    send(socket, dbName, MAX_CREDENTIALS_LENGTH, 0);
    read(socket, response, FAIL_OR_SUCCESS_LENGTH);
    if(strcmp(response, successMsg) == 0) {
        printf("Database %s berhasil dihapus\n", dbName);
        return;
    }
    char reason[MAX_INFORMATION_LENGTH];
    read(socket, reason, MAX_INFORMATION_LENGTH);
    printf("%s\n", reason);
}

void executePrompt(int socket, char fullQuery[]) {
    if(strstr(fullQuery, "CREATE USER") != NULL)
        createUser(socket, fullQuery);
    else if(strstr(fullQuery, "USE") != NULL)
        useDatabase(socket, fullQuery);
    else if(strstr(fullQuery, "GRANT PERMISSION") != NULL)
        grantDatabaseToUser(socket, fullQuery);
    else if(strstr(fullQuery, "CREATE DATABASE") != NULL)
        createDatabase(socket, fullQuery);
    else if(strstr(fullQuery, "DROP DATABASE") != NULL)
        dropDatabase(socket, fullQuery);
    else if(strstr(fullQuery, "EXIT") != NULL)
        exit(EXIT_SUCCESS);
}

int main(int argc, char const *argv[]) {
    int sock = 0, valread;
    if(!setupClient(&sock)) {
        printf("Gagal inisiasi client\n");
        exit(EXIT_FAILURE);
    }

    parseArgs(argc, argv);
    
    puts("Menunggu hingga koneksi diterima.");
    char acceptedConnection[FAIL_OR_SUCCESS_LENGTH];
    memset(acceptedConnection, 0, FAIL_OR_SUCCESS_LENGTH);
    read(sock, acceptedConnection, FAIL_OR_SUCCESS_LENGTH);
    if(strcmp(acceptedConnection, successMsg) == 0) {
        puts("Koneksi diterima.");
    } else {
        puts("Koneksi gagal.");
        exit(EXIT_FAILURE);
    }

    if(!authentication(sock)) {
        exit(EXIT_FAILURE);
    }

    printf("User authenticated.\n");
    while(true) {
        char query[MAX_INFORMATION_LENGTH];
        memset(query, 0, MAX_INFORMATION_LENGTH);
        fgets(query, MAX_INFORMATION_LENGTH, stdin);
        if(query[strlen(query) - 1] == '\n') query[strlen(query) - 1] = '\0';
        executePrompt(sock, query);
    }
    return 0;
}