#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <stdbool.h>

#include "./utils/include/constants.h"
#include "./utils/include/client_utils.h"

int sock;

void handler(int sig) {
    printf("\nIl giocatore ha chiuso il gioco.\n");
    send_msg(sock, EXIT);
    close(sock);
    exit(0);
}

int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGHUP, handler);
    signal(SIGINT, handler);


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

        bool valid_main_menu_choice = false;
        do {
            int read_status;
            printf("Trivia quiz\n");
            printf(SEPARATOR);
            printf("Menù:\n");
            printf("1-Comincia una sessione di Trivia\n");
            printf("2-Esci\n");
            printf(SEPARATOR);
            printf("La tua scelta:\n");

            read_status = scanf("%d", &choice);

            /*
                Pulizia del buffer di input : consuma i caratteri rimanenti nel
                buffer di input fino al newline o EOF.
            */
            int c;
            while ((c = getchar()) != '\n' && c != EOF);

            if (read_status != 1) {
                printf("\nScelta non valida. Inserisci un'opzione tra quelle nella lista.\n\n");
                valid_main_menu_choice = false;
            } else if (choice == 2) {
                // Ha scelto 2-Esci
                printf("Uscita dal gioco.\n");
                close(sock);
                return 0;
            } else if (choice == 1) {
                valid_main_menu_choice = true;
            } else {
                printf("\nScelta non valida. Inserisci un'opzione tra quelle nella lista.\n\n");
                valid_main_menu_choice = false;
            }
        } while (!valid_main_menu_choice);


        // Ha scelto 1
        // Connessione al server
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            printf("Connessione fallita\n");
            return -1;
        }

        while(1) {
            reset(buffer);
            recv_msg(sock, buffer);

            if(strcmp(buffer, ENDQUIZ) == 0) {
                close(sock);
                break;
            }

            if(strcmp(buffer, FINISHED) == 0) {
                printf("Hai completato tutti i quiz!\n");
                printf(SEPARATOR);
                close(sock);
                return 0;
            }

            printf("%s", buffer);
            reset(answer);

            do {
                reset(answer);
                fgets(answer, BUFFER_SIZE, stdin);
            } while(strcmp(NEW_LINE, answer) == 0);

            answer[strcspn(answer, NEW_LINE)] = '\0';

            send_msg(sock, answer);
        }
    
    }


    close(sock);
    return 0;
}