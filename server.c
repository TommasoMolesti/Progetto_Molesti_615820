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
#include "./utils/include/server_utils.h"

extern const char* THEMES[N_THEMES];
extern int server_sock;
extern int players_count;
extern Player *players;
extern Theme QUIZ[N_THEMES];

void handle_player(Player* p, fd_set* readfds) {
    char buffer[BUFFER_SIZE];

    reset(buffer);
    recv_msg(p->sock, buffer);

    if(strcmp(buffer, ENDQUIZ) == 0) {
        reset(buffer);
        strcpy(buffer, ENDQUIZ);
        send_msg(p->sock, buffer);
        endquiz(p->username);
        close(p->sock);
        FD_CLR(p->sock, readfds);
        remove_player(p->sock);
        show_results();
        return;
    }

    if(strcmp(buffer, QUIT) == 0) {
        printf("\n\nUn client ha terminato la connessione.\n\n");
        endquiz(p->username);
        close(p->sock);
        FD_CLR(p->sock, readfds);
        remove_player(p->sock);
        show_results();
        return;
    }

    // gestisco il caso in cui mi abbia mandato il nickname
    if(strcmp(p->username, "") == 0) {
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

    // Caso in cui ha scelto ha scelto un tema
    // - non ci sono temi in corso
    // - il valore rientra nell'intervallo
    if(
        !is_some_theme_pending(p) &&
        atoi(buffer) > 0 &&
        atoi(buffer) < (N_THEMES + 1)
    ) {        
        int theme = atoi(buffer);
        theme--;

        if(!theme_already_completed(p, theme)) {
            p->current_theme = theme;
            if(!p->games[theme].started)
                p->games[theme].started = true;
        
            show_results();
            
            // devo inviare la domanda corrente del tema theme
            Theme *t = &QUIZ[theme];
            Question *q = &t->questions[p->games[theme].current_question];
            sprintf(buffer, "\nQuiz %s\n", t->label);
            strcat(buffer, SEPARATOR);
            strcat(buffer, q->text);
            strcat(buffer, NEW_LINE);
            send_msg(p->sock, buffer);
            return;
        } else {
            // Giocatore ha completato tutti i temi disponibili
            if(is_game_ended(p)) {
                strcpy(buffer, "\nHai completato il gioco !\n");
                strcat(buffer, SEPARATOR);
                send_msg(p->sock, buffer);
                FD_CLR(p->sock, readfds);
                show_results();
                return;
            } else {
                strcpy(buffer, "\nHai già giocato a questo tema!\nScegline un altro.\n");
                get_quiz(buffer);
                send_msg(p->sock, buffer);
                return;
            }
        }
    }


    // caso opzione non valida
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


    // caso in cui manda la risposta ad una domanda
    if(p->current_theme != -1) {        
        // controllo se il comando mandato è show score
        if(strcmp(buffer, SHOW_SCORE) == 0) {
            show_score(p);
            return;
        }

        Theme *t = &QUIZ[p->current_theme];

        if(verify_answer(t, p->games[p->current_theme].current_question, buffer)) {
            strcpy(buffer, "\nRisposta corretta!\n");
            p->games[p->current_theme].score++;
            p->games[p->current_theme].current_question++;
            if(p->games[p->current_theme].current_question == N_QUEST) {
                // era l'ultima domanda
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
        Question *q = &t->questions[p->games[p->current_theme].current_question];
        strcat(buffer, q->text);
        strcat(buffer, NEW_LINE);
        send_msg(p->sock, buffer);
        return;
    }

}

void handler(int sig) {
    printf("Disconnessione del server\n");

    close(server_sock);
}

void handle_client(int server_sock, fd_set* readfds, int* max_sd) {
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

    FD_SET(client_sock, readfds);
    if(client_sock > *max_sd)
        *max_sd = client_sock;

    char msg[BUFFER_SIZE];
    snprintf(msg, sizeof(msg), "%s%sScegli un nickname (deve essere univoco):\n", TITLE, SEPARATOR);
    send_msg(client_sock, msg);

    return;
}

int main() {
    signal(SIGHUP, handler);
    signal(SIGINT, handler);

    struct sockaddr_in address;

    init_game();
    get_quiz_database();

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
    int max_sd, activity;

    FD_ZERO(&readfds);
    FD_SET(server_sock, &readfds);
    max_sd = server_sock;

    while(1) {
        fd_set master = readfds;

        activity = select(max_sd+1, &master, NULL, NULL, NULL);
        if((activity < 0) && (errno != EINTR)) {
            perror("Err : select()\n");
            break;
        }

        if(FD_ISSET(server_sock, &master)) {
            handle_client(server_sock, &readfds, &max_sd);
        }

        // Scorro i client connessi e registrati e controllo se ci sono dati da leggere
        Player *current_player = players;
        Player *next_player;
        while(current_player != NULL) {
            // Mi salvo next_player prima di gestire
            // Previene accessi a memoria deallocata se current_player viene rimosso.
            next_player = current_player->next;

            if(FD_ISSET(current_player->sock, &master)) {
                handle_player(current_player, &readfds);
            }
            current_player = next_player;
        }
    }
    
    close(server_sock);
    return 0;
}