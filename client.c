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
    sendmsg(sock, EXIT);
    close(sock);
    exit(0);
}

int main(int argc, char **argv) {
    /*
        Gestione segnali:
        - SIGPIPE: evita crash se scrivo su un socket chiuso (lo ignoro)
        - SIGHUP: chiusura del terminale
        - SIGINT: Ctrl+C
    */
    signal(SIGPIPE, SIG_IGN);
    signal(SIGHUP, handler);
    signal(SIGINT, handler);


    int port = PORT; // Porta di default
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    // Se viene passata una porta come parametro si usa quella
    if(argc > 1) {
        port = atoi(argv[1]);
    }


    while(1) {
        bool valid_choice = false;
        // Creazione del socket TCP
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1) {
            printf("Errore nella creazione del socket\n");
            return -1;
        }
    
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
    
        // Converto l'indirizzo IP del server
        if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
            printf("Indirizzo non valido\n");
            close(sock);
            return -1;
        }

        // Gestione del menù di scelta
        // Se la scelta non è valida per qualche motivo, si ripresenta il menù
        do {
            int read_status;
            int choice;

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
                // Non è un numero
                printf("\nScelta non valida. Inserisci un'opzione tra quelle nella lista.\n\n");
                valid_choice = false;
            } else if (choice == 2) {
                printf("Uscita dal gioco.\n");
                close(sock);
                return 0;
            } else if (choice == 1) {
                valid_choice = true;
            } else {
                // E' un numero ma fuori dal range di scelte
                printf("\nScelta non valida. Inserisci un'opzione tra quelle nella lista.\n\n");
                valid_choice = false;
            }
        } while (!valid_choice);


        // Connessione al server
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            printf("Connessione fallita\n");
            return -1;
        }

        while(1) {
            reset(buffer);
            recvmsg(sock, buffer);

            // Se il client ha mandato endquiz e il server ha risposto con endquiz
            // Chiudo la connessione e ricomincio dal menù principale
            if(strcmp(buffer, ENDQUIZ) == 0) {
                close(sock);
                break;
            }

            // Controllo se il server ha detto che il gioco è stato terminato
            if(strcmp(buffer, FINISHED) == 0) {
                printf("Hai completato tutti i quiz!\n");
                printf(SEPARATOR);
                close(sock);
                return 0;
            }

            printf("%s", buffer);

            // Si cicla finché la stringa non è vuota
            // Uso lo stesso buffer, ma stavolta per mandare il messaggio al server
            do {
                reset(buffer);
                fgets(buffer, BUFFER_SIZE, stdin);
            } while(strcmp(NEW_LINE, buffer) == 0);

            buffer[strcspn(buffer, NEW_LINE)] = '\0';

            sendmsg(sock, buffer);
        }
    
    }

    close(sock);
    return 0;
}