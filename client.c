#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024

int message_len, effective_message_len, sock;

void send_msg(int sd, char* buffer) {
    int ret;

    message_len = strlen(buffer);
    effective_message_len = htonl(message_len);
    ret = send(sd, &effective_message_len, sizeof(effective_message_len), 0);
    if(ret == -1) {
        printf("Server disconnesso, gioco terminato\n");
        close(sd);
        exit(1);
    }

    ret = send(sd, buffer, message_len, 0);
    if(ret == -1) {
        printf("Server disconnesso, gioco terminato\n");
        close(sd);
        exit(1);
    }
}

void recv_msg(int sd, char* buffer) {
    int len;

    int bytes_read = recv(sd, &message_len, sizeof(message_len), 0);
    if(bytes_read == -1) {
        printf("Server disconnesso, gioco terminato\n");
        close(sd);
        exit(1);
    }

    len = ntohl(message_len);
    bytes_read = recv(sd, buffer, len, 0);
    if(bytes_read == -1) {
        printf("Server disconnesso, gioco terminato\n");
        close(sd);
        exit(1);
    }
    buffer[bytes_read] = '\0';
}

void handle_sigint(int sig) {
    printf("\nGiocatore ha chiuso la partita da terminale\n");
    char buffer[BUFFER_SIZE] = "quit";
    send_msg(sock, buffer);

    close(sock);
    exit(0);
}

int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGHUP, handle_sigint);
    signal(SIGINT, handle_sigint);


    int port = 1234;
    int numero;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE]={0};
    char risposta[BUFFER_SIZE];

    if(argc > 1) {
        port = atoi(argv[1]);
    }


    while(1) {
        // Creazione del socket TCP
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1) {
            printf("Errore nella creazione del socket\n");
            return -1;
        }
    
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
    
        // Converte l'indirizzo IP
        if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
            printf("Indirizzo non valido\n");
            close(sock);
            return -1;
        }

        // Connessione al server
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            printf("Connessione fallita\n");
            return -1;
        }

        printf("Trivia quiz\n");
        printf("+++++++++++++++++++\n");
        printf("Menù:\n");
        printf("1-Comincia una sessione di Trivia\n");
        printf("2-Esci\n");
        printf("+++++++++++++++++++\n");
        printf("La tua scelta:\n");
        if(scanf("%d", &numero) != 1) {
            printf("Scelta non valida\n");
            return 1;
        }

        if(numero == 2) {
            close(sock);
            return 0;
        } else if(numero > 2 || numero <= 0) {
            printf("Scelta non valida\n");
            return 1;
        }

        while(1) {
            memset(buffer, '\0', sizeof(buffer));
            recv_msg(sock, buffer);

            if(strcmp(buffer, "endquiz") == 0) {
                close(sock);
                break;
            }

            if(strcmp(buffer, "Completati") == 0) {
                printf("Completati entrambi i quiz\n");
                close(sock);
                return 0;
            }

            printf("%s", buffer);
            memset(risposta, '\0', BUFFER_SIZE);

            do {
                memset(risposta, '\0', BUFFER_SIZE);
                fgets(risposta, BUFFER_SIZE, stdin);
            } while(strcmp("\n", risposta) == 0);

            risposta[strcspn(risposta, "\n")] = '\0';

            send_msg(sock, risposta);
        }
    
    }


    close(sock);
    return 0;
}