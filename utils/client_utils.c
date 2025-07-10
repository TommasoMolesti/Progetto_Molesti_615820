#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "./include/client_utils.h"
#include "./include/constants.h"

// Funzione di utilità per mandare un messaggio al server
void sendmsg(int sd, char* buffer) {
    int ret;

    int message_len = strlen(buffer);
    int effective_message_len = htonl(message_len);
    ret = send(sd, &effective_message_len, sizeof(effective_message_len), 0);
    if(ret == -1) {
        printf("Disconnesso, gioco finito.\n");
        close(sd);
        exit(1);
    }

    ret = send(sd, buffer, message_len, 0);
    if(ret == -1) {
        printf("Disconnesso, gioco finito\n");
        close(sd);
        exit(1);
    }
}

// Funzione di utilità per ricevere un messaggio dal server
void recvmsg(int sd, char* buffer) {
    int len;
    int message_len;

    int bytes_read = recv(sd, &message_len, sizeof(message_len), 0);
    if(bytes_read == -1) {
        printf("Disconnesso, gioco finito\n");
        close(sd);
        exit(1);
    }

    len = ntohl(message_len);
    bytes_read = recv(sd, buffer, len, 0);
    if(bytes_read == -1) {
        printf("Disconnesso, gioco finito\n");
        close(sd);
        exit(1);
    }
    buffer[bytes_read] = '\0';
}

// Per resettare il buffer
void reset(char* buffer) {
    memset(buffer, '\0', BUFFER_SIZE);
}