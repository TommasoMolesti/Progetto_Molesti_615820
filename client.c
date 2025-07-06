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

#include "./utils/include/constants.h"
#include "./utils/include/client_utils.h"

int sock;

void handle_sigint(int sig) {
    printf("\nGiocatore ha chiuso la partita da terminale\n");
    char buffer[BUFFER_SIZE] = QUIT;
    send_msg(sock, buffer);

    close(sock);
    exit(0);
}

int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGHUP, handle_sigint);
    signal(SIGINT, handle_sigint);


    int port = 1234;
    int choice;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE]={0};
    char answer[BUFFER_SIZE];

    if(argc > 1)
        port = atoi(argv[1]);


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

        printf("Trivia quiz\n");
        printf(SEPARATOR);
        printf("Menù:\n");
        printf("1-Comincia una sessione di Trivia\n");
        printf("2-Esci\n");
        printf(SEPARATOR);
        printf("La tua scelta:\n");
        if(scanf("%d", &choice) != 1) {
            printf("Scelta non valida\n");
            return 1;
        }

        if(choice == 2) {
            // Ha scelto 2-Esci
            strcpy(answer, QUIT);
            send_msg(sock, answer);
            close(sock);
            return 0;
        } else if(choice > 2 || choice <= 0) {
            printf("Scelta non valida\n");
            return 1;
        } 

        // Ha scelto 1
        // Connessione al server
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            printf("Connessione fallita\n");
            return -1;
        }

        while(1) {
            memset(buffer, '\0', sizeof(buffer));
            recv_msg(sock, buffer);

            if(strcmp(buffer, ENDQUIZ) == 0) {
                close(sock);
                break;
            }

            if(strcmp(buffer, "Completati") == 0) {
                printf("Completati entrambi i quiz\n");
                close(sock);
                return 0;
            }

            printf("%s", buffer);
            memset(answer, '\0', BUFFER_SIZE);

            do {
                memset(answer, '\0', BUFFER_SIZE);
                fgets(answer, BUFFER_SIZE, stdin);
            } while(strcmp(NEW_LINE, answer) == 0);

            answer[strcspn(answer, NEW_LINE)] = '\0';

            send_msg(sock, answer);
        }
    
    }


    close(sock);
    return 0;
}