#ifndef JETPACULA_COMMON_H
#define JETPACULA_COMMON_H
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int CheckSocket( int socket ) {
    int error = 0;
    socklen_t len = sizeof(error);
    int retval = getsockopt((int) socket, SOL_SOCKET, SO_ERROR, &error, &len);

    if (retval != 0) {
        fprintf(stderr, "error getting socket error code: %s\n", strerror(retval));
        return EXIT_FAILURE;
    }
    if (error != 0) {
        fprintf(stderr, "socket error: %s\n", strerror(error));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

typedef struct {
    char *nick;
    char *buffer;
    char *time;
} Message;


int writeToSocket(int socketFD, char * buffer, uint32_t bytesToWrite){

    if (CheckSocket(socketFD)) {
        return EXIT_FAILURE;
    };
    uint32_t offset = 0;
    uint16_t tries = 7;
    while(bytesToWrite != 0){
        int writtenBytes = write(socketFD, buffer + offset, bytesToWrite);
        if( writtenBytes < 0 ) {
            perror("failed to write to socket\n");
            return EXIT_FAILURE;
        }
        if (writtenBytes == 0){
            --tries;
            if (tries ==0){
                return EXIT_FAILURE;
            }
        }
        bytesToWrite -= writtenBytes;
        offset += writtenBytes;
    }
    return EXIT_SUCCESS;
}

int writeBufferToSocket( uint32_t socket, char *buffer ) {
    if( buffer == NULL) {
        return EXIT_SUCCESS;
    }
    uint32_t bufferSize = strlen(buffer);
    bufferSize = htonl(bufferSize);

    if( writeToSocket(socket, (char *) &bufferSize, sizeof(bufferSize))) {
        return EXIT_FAILURE;
    }
    bufferSize = ntohl(bufferSize);
    if( bufferSize == 0 ) {
        return EXIT_SUCCESS;
    }
    if( writeToSocket(socket, buffer, bufferSize)) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int readFromSocket(uint32_t socketFD, char * buffer, uint32_t bytesToRead){
    if (CheckSocket(socketFD)) {
        return EXIT_FAILURE;
    };
    uint32_t offset = 0;
    uint16_t tries = 7;

    while(bytesToRead != 0){

        int gotBytes = read(socketFD, buffer + offset, bytesToRead);
        if( gotBytes < 0 ) {
            perror("failed to read from socket\n");
            return EXIT_FAILURE;
        }
        if (gotBytes == 0 ){
            sleep(1);
            --tries;
            if (tries ==0){
                return EXIT_FAILURE;
            }
        }
        bytesToRead -= gotBytes;
        offset += gotBytes;
    }
    return EXIT_SUCCESS;
}

int readBufferFromSocket( uint32_t socket, char **buffer ) {
    uint32_t bufferSize = 0;
    if( readFromSocket(socket, (char *) &bufferSize, sizeof(bufferSize))) {
        return EXIT_FAILURE;
    }
    bufferSize = ntohl(bufferSize);
    if( bufferSize == 0 ) {
        *buffer = (char *) malloc(1);
        (*buffer)[0] = '\0';
        return EXIT_SUCCESS;
    }
    *buffer = (char *) malloc(bufferSize);
    if( readFromSocket(socket, *buffer, bufferSize) != 0) {
        return EXIT_FAILURE;
    }
    if((*buffer)[bufferSize - 1] != '\0' ) {
        *buffer = realloc(*buffer, bufferSize + 1);
        (*buffer)[bufferSize] = '\0';
    }
    return EXIT_SUCCESS;
}

int getMessageFromServer(uint32_t socketFD, Message * MsgOut){
    if(readBufferFromSocket(socketFD,&(MsgOut->nick)) ||
       readBufferFromSocket(socketFD,&(MsgOut->buffer)) ||
       readBufferFromSocket(socketFD,&(MsgOut->time))){
        return EXIT_FAILURE;
    }
    else return EXIT_SUCCESS;
}

char * getMsgTime() {
    time_t rawtime;
    struct tm *info;
    size_t bufSize = 80;
    char *buffer = (char *) malloc(bufSize);
    time(&rawtime);
    info = localtime(&rawtime);
    int res = strftime(buffer, bufSize, "%H:%M", info);
    while (!res){
        free (buffer);
        bufSize = bufSize *2;
        char *buffer = (char *) malloc(bufSize);
        strftime(buffer, bufSize, "%H:%M", info);
    }
    return buffer;
}

int getMessageFromClient(int socketFD, Message * MsgOut){
    //  MsgOut->time = getMsgTime();
    if(readBufferFromSocket(socketFD,&(MsgOut->nick)) ||
       readBufferFromSocket(socketFD,&(MsgOut->buffer))
        //    readBufferFromSocket(socketFD,&(MsgOut->time))
            )
    {
        return EXIT_FAILURE;
    }
    else
        //  MsgOut->time = getMsgTime();
        return EXIT_SUCCESS;
}

int sendMessageToClient(int socketFD, Message * MsgOut){
    if(writeBufferToSocket(socketFD,MsgOut->nick) ||
       writeBufferToSocket(socketFD,MsgOut->buffer) ||
       writeBufferToSocket(socketFD,MsgOut->time)){
        return EXIT_FAILURE;
    }
    else return EXIT_SUCCESS;
}


int sendMessageToServer(int socketFD, Message * MsgOut){
    if(writeBufferToSocket(socketFD,MsgOut->nick) ||
       writeBufferToSocket(socketFD,MsgOut->buffer)
        //   writeBufferToSocket(socketFD,MsgOut->time)
            )
    {
        return EXIT_FAILURE;
    }
    else return EXIT_SUCCESS;
}

#endif //JETPACULA_COMMON_H

