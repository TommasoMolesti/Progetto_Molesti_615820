#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <stdbool.h>

#include "./utils/include/constants.h"
#include "./utils/include/server_utils.h"

extern const char* THEMES[N_THEMES];
extern int server_sock;
extern int players_count;
extern Player *players;
extern Theme QUIZ[N_THEMES];

void handler(int sig) {
    printf("\nServer disconnesso\n");
    close(server_sock);
    exit(0);
}

void handle_player(Player* p, fd_set* readfds) {
    char buffer[BUFFER_SIZE];

    reset(buffer);
    recv_msg(p->sock, buffer);

    // Caso in cui mi abbia mandato il nickname e non ha chiuso il terminale
    if(strcmp(p->username, "") == 0 && strcmp(buffer, EXIT) != 0) {
        if(!verify_username(buffer)) {
            // username non valido
            char msg[BUFFER_SIZE];
            snprintf(msg, sizeof(msg), "\nUsername non disponibile\nTrivia Quiz\n");
            strcat(msg, SEPARATOR);
            strcat(msg, "Scegli un username (univoco)\n");
            send_msg(p->sock, msg);
            return;
        }

        strcpy(p->username, buffer);
        reset(buffer);
        get_quiz(buffer);
        send_msg(p->sock, buffer);
        show_results();

        return;
    }

    // Caso in cui il client chiude il terminale
    if(strcmp(buffer, EXIT) == 0) {
        // Altrimenti è brutto mostrare il messaggio se il giocatore sta entrando ma non ha ancora un nome
        if(strcmp(p->username, "") != 0) {
            printf("\n\nUn giocatore ha terminato la connessione.\n\n");
        }
        close(p->sock);
        FD_CLR(p->sock, readfds); // Rimuovo il socket dal sets
        remove_player(p->sock);
        show_results();
        return;
    }

    // Caso in cui il client manda "endquiz"
    if(strcmp(buffer, ENDQUIZ) == 0) {
        reset(buffer);
        strcpy(buffer, ENDQUIZ);
        send_msg(p->sock, buffer);
        close(p->sock);
        FD_CLR(p->sock, readfds); // Rimuovo il socket dal sets
        remove_player(p->sock);
        show_results();
        return;
    }

    // Caso in cui il client manda "show score"
    if(strcmp(buffer, SHOW_SCORE) == 0) {
        show_score(p);
        return;
    }

    // Caso in cui ha scelto ha scelto un tema
    // - non ci sono temi in corso
    // - il valore mandato rientra nell'intervallo dei temi
    if(
        !is_some_theme_pending(p) &&
        atoi(buffer) > 0 &&
        atoi(buffer) < (N_THEMES + 1)
    ) {        
        int theme = atoi(buffer);
        theme--; // Per farlo coincidere con gli indici dell'array (nel menù si parte da 1 invece che da 0)

        if(!theme_already_completed(p, theme)) {
            // Giocatore ha scelto un tema a cui non ha ancora giocato
            p->current_theme = theme;
            if(!p->games[theme].started) {
                p->games[theme].started = true;
            }
        
            show_results();
            
            // Mando la domanda corrente del tema theme
            // Di default si parte dalla prima
            Theme *t = &QUIZ[theme];
            Question *q = &t->questions[p->games[theme].current_question];
            sprintf(buffer, "\nQuiz %s\n", t->label);
            strcat(buffer, SEPARATOR);
            strcat(buffer, q->text);
            strcat(buffer, NEW_LINE);
            send_msg(p->sock, buffer);
            return;
        } else if(is_game_ended(p)) {
            // Giocatore ha completato tutti i temi disponibili
            strcpy(buffer, FINISHED);
            send_msg(p->sock, buffer);
            FD_CLR(p->sock, readfds);
            show_results();
            return;
        } else {
            // Giocatore ha già giocato al tema selezionato, ma ci sono altri temi a cui può ancora giocare
            strcpy(buffer, "\nHai già giocato a questo tema!\nScegline un altro.\n");
            get_quiz(buffer);
            send_msg(p->sock, buffer);
            return;
        }
    }


    // Caso opzione non valida
    // - non ci sono temi in corso
    // - l'opzione non rietra nel range dei temi
    if(
        !(atoi(buffer) > 0) &&
        !(atoi(buffer) < N_THEMES) &&
        !is_some_theme_pending(p)
    ) {
        strcpy(buffer, "\nScelta del quiz non valida, riprova!\n");
        get_quiz(buffer);
        send_msg(p->sock, buffer);
        return;
    }


    // Caso in cui manda la risposta ad una domanda
    if(p->current_theme != -1) {        
        Theme *t = &QUIZ[p->current_theme];

        if(verify_answer(t, p->games[p->current_theme].current_question, buffer)) {
            strcpy(buffer, "\nRisposta corretta!\n");
            p->games[p->current_theme].score++;
            p->games[p->current_theme].current_question++; // Avanzo con l'indice della domanda
            if(p->games[p->current_theme].current_question == N_QUEST) {
                // Era l'ultima domanda
                printf("Finito \n");
                p->games[p->current_theme].ended = true;
                p->current_theme = -1;
                show_results();
                strcat(buffer, "\n\nHai completato il quiz, puoi sceglierne un altro!\n\n");
                get_quiz(buffer);
                send_msg(p->sock, buffer);
                return;
            }
            show_results();
        } else {
            strcpy(buffer, "\nRisposta errata, riprova.\n");
        }
        // Prendo la prossima domanda in sequenza e la mostro
        Question *q = &t->questions[p->games[p->current_theme].current_question];
        strcat(buffer, q->text);
        strcat(buffer, NEW_LINE);
        send_msg(p->sock, buffer);
        return;
    }
}

void handle_new_client(int server_sock, fd_set* readfds, int* max_sd) {
    int client_sock;
    struct sockaddr_in cl_addr;
    int len = sizeof(cl_addr);

    if((client_sock = accept(server_sock, (struct sockaddr*)&cl_addr, (socklen_t*)&len)) < 0) {
        perror("Err: accept()\n");
        return;
    }

    if(players_count >= MAX_PLAYERS) {
        char msg[BUFFER_SIZE];
        strcpy(msg, "Capacità massima del server raggiunta, riprova tra poco.\n");
        send_msg(client_sock, msg);
        close(client_sock);
        return;
    }

    add_player(client_sock);

    // Aggiungo il descrittore del client all'insieme
    FD_SET(client_sock, readfds);
    if(client_sock > *max_sd) // Tengo di conto del max descrittore per la select
        *max_sd = client_sock;

    char msg[BUFFER_SIZE];
    snprintf(msg, sizeof(msg), "%s%sScegli un nickname (deve essere univoco):\n", TITLE, SEPARATOR);
    send_msg(client_sock, msg);

    return;
}

int main() {
    /*
        Gestione segnali:
        - SIGHUP: chiusura del terminale
        - SIGINT: Ctrl+C
    */
    signal(SIGHUP, handler);
    signal(SIGINT, handler);

    struct sockaddr_in address;

    init_game(); // Inizializzo le strutture dati
    get_quiz_database(); // Carico le domande dai file nelle strutture dati

    // Creazione del socket TCP
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Err: socket()\n");
        exit(EXIT_FAILURE);
    }

    // Assegnazione indirizzo e porta
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = INADDR_ANY;

    // Binding del socket
    if (bind(server_sock, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Err: bind()\n");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    // Ascolta le connessioni
    if (listen(server_sock, MAX_PLAYERS) < 0) {
        perror("Err: listen()\n");
        exit(EXIT_FAILURE);
    }

    printf(TITLE);
    printf(SEPARATOR);
    theme_list();
    printf(SEPARATOR);
    printf(NEW_LINE);
    
    show_results();

    fd_set readfds;
    int max_sd;

    FD_ZERO(&readfds);
    FD_SET(server_sock, &readfds);
    max_sd = server_sock;

    while(1) {
        fd_set master = readfds;

        if(select(max_sd+1, &master, NULL, NULL, NULL) < 0) {
            perror("Err : select()\n");
            break;
        }

        // Gestione di una richiesta mandata da un nuovo client
        if(FD_ISSET(server_sock, &master)) {
            handle_new_client(server_sock, &readfds, &max_sd);
        }

        // Scorro i client connessi e registrati e controllo se ci sono dati da leggere
        Player *current_player = players;
        Player *next_player;
        while(current_player != NULL) {
            // Mi salvo next_player prima di gestire
            // Previene accessi a memoria deallocata se current_player viene rimosso.
            next_player = current_player->next;

            // Gestione di una richiesta mandata da un giocatore già registrato
            if(FD_ISSET(current_player->sock, &master)) {
                handle_player(current_player, &readfds);
            }
            current_player = next_player;
        }
    }
    
    close(server_sock);
    return 0;
}