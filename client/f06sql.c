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
    printf("Username: %s\n", username);
    printf("Password: %s\n", password);
    send(sock, username, MAX_CREDENTIALS_LENGTH, 0);
    send(sock, password, MAX_CREDENTIALS_LENGTH, 0);
    char message[FAIL_OR_SUCCESS_LENGTH];
    memset(message, 0, FAIL_OR_SUCCESS_LENGTH);
    read(sock, message, FAIL_OR_SUCCESS_LENGTH);
    return strcmp(successMsg, message) == 0;
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
        printf("Username atau password salah\n");
        exit(EXIT_FAILURE);
    }

    printf("User authenticated.\n");
    // while(true) {
    //     char action[MAX_INFORMATION_LENGTH];
    //     memset(action, 0, MAX_INFORMATION_LENGTH);
    //     printPrompt();
    //     scanf("%s", action);
    //     getchar();
    //     executePrompt(sock, action);
    // }
    return 0;
}