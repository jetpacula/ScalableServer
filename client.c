#define  _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>
#include <time.h>
#include <strings.h>

#include "include/common.h"


pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;



void printMessage( Message *printableMsg ) {
    printf("{%s} [%s] %s\n", printableMsg->time, printableMsg->nick, printableMsg->buffer);
}

int readerLoop(void * socketFD){
    uint32_t socketToUse = * (uint32_t *) socketFD;
    while(1) {
        Message MsgIn = {NULL,NULL,NULL};
        int res = getMessageFromServer(socketToUse,&MsgIn);
        if (res){
            return EXIT_FAILURE;
        }

        if(MsgIn.buffer == NULL){
            continue;
        }
        printMessage(&MsgIn);
    }
}



int writerLoop(void * socketFD, char * nickName) {
    uint32_t socketToUse = * (uint32_t *) socketFD;
    uint32_t nicknameSize = strlen(nickName);
    char * nickNameToUse = malloc(nicknameSize);
    nickNameToUse = nickName;

    size_t linecap = 0;
    while(1) {
        size_t len = 0;
        char * mbuf = NULL;
        if( getline(&mbuf, &len, stdin) == -1 ) {
            perror("error reading from console");
            return EXIT_FAILURE;
        }
        if( strcmp(mbuf, "m\n") == 0 ) {
            Message MsgOut = {nickNameToUse,NULL,NULL};
            if( feof(stdin) ){
                free(MsgOut.buffer);
                free(MsgOut.time);
                break;
            }
            uint32_t* linelen = malloc(sizeof(uint32_t));
            *linelen = getline(&MsgOut.buffer, &linecap , stdin);
            if( feof(stdin) ){
                free(MsgOut.buffer);
                free(MsgOut.time);
                break;
            }
            MsgOut.buffer[*linelen-1] = '\0';
            MsgOut.time = getMsgTime();

            int res = sendMessageToServer(socketToUse,&MsgOut);
            if (res) {
                return EXIT_FAILURE;
            }
            free(MsgOut.buffer);
            MsgOut.buffer = NULL;
            free(MsgOut.time);
            MsgOut.time = NULL;
            MsgOut.nick = NULL;
            free (linelen);
            mbuf = NULL;}
    }
    free(nickNameToUse);
    return EXIT_FAILURE;
}

uint32_t connectToServer (uint32_t IPAddress, uint16_t portNumber){
    struct sockaddr_in serv_addr;

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = IPAddress;
    serv_addr.sin_port = htons(portNumber);
    int socketNumber = socket(AF_INET, SOCK_STREAM, 0);

    if (socketNumber < 0){
        perror("Socket failure\n");
        return 0;
    }
    if (connect(socketNumber, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
        perror("Connection failed ");
        return 0;
    }
    else{
        return socketNumber;
    }
}

int main(int argc, char** argv) {

    if (argc < 4) {
        fprintf(stderr, "usage %s hostname port\n", argv[0]);
        return EXIT_FAILURE;
    }

    uint32_t ipaddr = inet_addr(argv[1]);

    uint16_t portNumber;
    portNumber = (uint32_t) strtol(argv[2], (char **) NULL, 10);

    char *nickName = argv[3];
    pthread_t tid;
    uint32_t socketNo = connectToServer(ipaddr, portNumber);
    if (socketNo == 0) {
        return EXIT_FAILURE;
    }
    pthread_create(&tid, NULL, (void *(*)(void *)) &readerLoop, (void *) &socketNo);
    if (writerLoop((void *) &socketNo, nickName)) {
        return EXIT_FAILURE;
    }
}

