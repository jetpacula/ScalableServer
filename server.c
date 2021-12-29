//
// Created by jetpacula on 26.12.2021.
//
#include <json-c/json.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <hiredis/hiredis.h>
#include <arpa/inet.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "./common.h"
//#include "include/common.h"
#define __STDC_LIMIT_MACROS
#include <assert.h>


#define MAXWAITINGLISTEN 5
#define MAXCLIENTS 100


pthread_mutex_t clt_mutex = PTHREAD_MUTEX_INITIALIZER;


//int clientQty;

typedef struct{
    int sockfd;
    struct client_t * nextclt;
}   client_t;

char * strdelch(char *str, char ch)
{
    char *current = str;
    char *tail = str;

    while(*tail)
    {
        if(*tail == ch)
        {
            tail++;
        }
        else
        {
            *current++ = *tail++;
        }
    }
    *current = 0;
    return str;
}


client_t * headClient = NULL;
int clientQty = 0;

int sendMessages(Message * MsgToAll){
    pthread_mutex_lock(&clt_mutex);

    for( client_t * tempclient = headClient; tempclient != NULL;tempclient = (client_t *) tempclient->nextclt ) {
        if (sendMessageToClient(tempclient->sockfd, MsgToAll)!=0){
            continue;
            //return EXIT_FAILURE;
        };
    }
    pthread_mutex_unlock(&clt_mutex);
    return EXIT_SUCCESS;
}

client_t * addClient (int  socketFD) { //add client

    client_t * newClient = malloc(sizeof (client_t));
    newClient->sockfd = socketFD;
    newClient->nextclt = NULL;
    return  newClient;
}

client_t * addClientAtHead (client_t ** head, client_t * clientToInsert) {
    clientToInsert->nextclt = (struct client_t *)*head;
    *head = clientToInsert;
    return clientToInsert;
}

void removeClient(client_t** head_ref, client_t *clientInstance){

    client_t *temp = *head_ref;
    if (temp != NULL && temp->sockfd == clientInstance->sockfd) {
        free(*head_ref);
        *head_ref = (client_t*)temp->nextclt; // Changed head
//        free(*head_ref);
        return;
    }
    client_t * prev = NULL;
    while (temp != NULL && temp->sockfd != clientInstance->sockfd) {
        prev = temp;
        temp = (client_t*) temp->nextclt;
    }
    prev->nextclt = temp->nextclt;
    //   free(temp);
    free(clientInstance);
}

int WriteToDB (Message MsgToDB) {
    char ipaddr [] = "127.0.0.1";
    redisContext *c = redisConnect(ipaddr, 6379);
    if (c != NULL && c->err) {
        printf("Error: %s\n", c->errstr);
// handle error
    } else {
        printf("Connected to Redis\n");
    }
    redisReply *reply;
    pid_t process_id;
    process_id = getpid();
    char * mypid = malloc(6);   // ex. 34567
    sprintf(mypid, "%d", process_id);


    json_object * RedisKeyJson = json_object_new_object();
    json_object *jstringBuf = json_object_new_string(MsgToDB.buffer);
    json_object *jstringNick = json_object_new_string(MsgToDB.nick);

    json_object_object_add(RedisKeyJson,"message", jstringBuf);
    json_object_object_add(RedisKeyJson,"nickname", jstringNick);

    char * RedisKeyString = json_object_to_json_string_ext(RedisKeyJson,  JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY);
/////

    json_object * array = json_object_new_array();
    json_object  * RedisValJson = json_object_new_object();
    // Fill the array
    json_object * jsonPID = json_object_new_string(mypid);
    json_object_array_add(array, jsonPID);
    json_object_object_add(RedisValJson,"PIDs", array);

    char * RedisValString = json_object_to_json_string_ext(RedisValJson,  JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY);


    reply = redisCommand(c,"SET %s %s",RedisKeyString,RedisValString);
    free(mypid);

    freeReplyObject(reply);
    redisFree(c);
}

void ReadFromDB (Message * MsgFromDB) {

    char ipaddr [] = "127.0.0.1";
    redisContext *c = redisConnect(ipaddr, 6379);
    if (c != NULL && c->err) {
        printf("Error: %s\n", c->errstr);
// handle error
    } else {
        printf("Connected to Redis\n");
    }
    redisReply *reply;

    pid_t process_id;
    process_id = getpid();

    char * mypid = malloc(6);   // ex. 34567
    sprintf(mypid, "%d", process_id);

   // printf("The process id: %d\n",process_id);
    reply = redisCommand(c,"KEYS *");


    redisReply * ReplyForEachKey;
    for (ssize_t i = 0; i < reply->elements; i++){

    ReplyForEachKey = redisCommand(c,"GET %s",(reply->element[i])->str);
	        if (strstr(ReplyForEachKey->str,mypid)){
            freeReplyObject(ReplyForEachKey);
            continue;
        }
    json_object * jobj = json_tokener_parse((reply->element[i])->str);
    json_object * jobjMessage;
    json_object * jobjNickname;
    json_object_object_get_ex(jobj, "message", &jobjMessage);
    json_object_object_get_ex(jobj, "nickname", &jobjNickname);
    

    char * MessageFromDBStack = json_object_to_json_string_ext(jobjMessage,JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY);
    char * strippedMsg = strdelch(MessageFromDBStack, '"');

char *MessageFromDB = malloc(strlen(strippedMsg));
MessageFromDB = strippedMsg ;

    char * NicknameFromDBStack = json_object_to_json_string_ext(jobjNickname,JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY);
    char * strippedNickname = strdelch(NicknameFromDBStack,'"');

char *NicknameFromDB = malloc(strlen(strippedMsg));
 NicknameFromDB = strippedNickname ;

//// messing with PIDs
        json_object * array = json_object_new_array();
        json_object * tmp = json_tokener_parse(ReplyForEachKey->str);
        json_object_object_get_ex(tmp, "PIDs", &array);

    char * pidprnt = json_object_to_json_string_ext(array ,JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY);

    json_object * tmpNew = json_object_new_string(mypid);
        json_object_array_add(array, tmpNew);
        json_object * NewValForRedis =  json_object_new_object();
        json_object_object_add(NewValForRedis,"PIDs", array);
        char * forRedis = json_object_to_json_string_ext(NewValForRedis,JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY);
    freeReplyObject(ReplyForEachKey);
    ReplyForEachKey= redisCommand(c,"SET %s %s",(reply->element[i])->str,forRedis);
    freeReplyObject(ReplyForEachKey);
    MsgFromDB->buffer = MessageFromDB;
    MsgFromDB->nick = NicknameFromDB;
    MsgFromDB->time = getMsgTime();
    sendMessages(MsgFromDB);
    free(MsgFromDB->buffer);
    free(MsgFromDB->nick) ;
    free(MsgFromDB->time);
 } 
redisFree(c);

}

void * client_handler(void * clientArg){

    client_t *clientInstance = (client_t *)clientArg;

    while (1) {
        Message Msg = {NULL,NULL,NULL};
        int res = getMessageFromClient(clientInstance->sockfd,&Msg);
        if( res) {
            break;
        }
        Msg.time = getMsgTime();

        sendMessages(&Msg); //,clientInstance->uid
        WriteToDB(Msg);
        free(Msg.buffer);
        free(Msg.time);
        free(Msg.nick);
        ReadFromDB(&Msg);
//        free(Msg.buffer);
//        free(Msg.time);
//        free(Msg.nick);
    }
    pthread_mutex_lock(&clt_mutex);
    close(clientInstance->sockfd);
    removeClient(&headClient, clientInstance);
    pthread_mutex_unlock(&clt_mutex);
    pthread_exit(NULL);
}

uint32_t checkPort(char * portNumberPtr){
    for( size_t i = 0; i < strlen(portNumberPtr); ++i ) {
        if( !isdigit(portNumberPtr[i])) {
            fprintf(stderr, "Port %s is not allowed.\n", portNumberPtr);
            exit(0);
        }
    }
    uint32_t portToCheck = strtoll(portNumberPtr,(char **)NULL, 10);
    if(65535 <= portToCheck ||
       1024>=portToCheck) {
        fprintf(stderr, "Port must be between 1024 and 65535\n");
        exit(0);
    }

    return portToCheck;
}




int listenForConnections (uint32_t portNumber){


    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    int listenfd;

    uint32_t ipaddr = inet_addr("0.0.0.0");

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = ipaddr;
    serv_addr.sin_port = htons(portNumber);

    //  signal(SIGPIPE, SIG_IGN);
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int));

    if (bind(listenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR: Socket binding failed");
        return EXIT_FAILURE;
    }
    if (listen(listenfd, MAXWAITINGLISTEN) < 0) {
        perror("ERROR: Socket listening failed");
        return EXIT_FAILURE;
    }
    while (1) {
        socklen_t clilen = sizeof(cli_addr);
        //     int* new_fd = malloc(sizeof (int));

        int new_fd = accept(listenfd, (struct sockaddr *) &cli_addr, &clilen);
        if (new_fd == -1){
            perror("accept");
            continue;
        }

        if (clientQty < MAXCLIENTS){
            client_t * newClient = addClient(new_fd);
            addClientAtHead(&headClient,newClient);

            pthread_attr_t  tattr;
            pthread_attr_init (&tattr);
            pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
            pthread_t tid;
            pthread_create(&tid,&tattr,client_handler, newClient);
        }
        else {
            close(new_fd);
            //  free(new_fd);
            printf("client cap reached");
            sleep(5);
            continue;
        }

    }

}



int main (int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage %s port\n", argv[0]);
        return EXIT_FAILURE;
    }
    uint32_t port = checkPort(argv[1]);
//    char * prt = '5001';
 //   uint32_t port = checkPort(prt);
 //   uint32_t port = 5001;


    listenForConnections(port);
    return EXIT_SUCCESS;
}



