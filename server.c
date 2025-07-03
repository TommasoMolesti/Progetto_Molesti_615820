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
extern struct desc_player *players;
extern Tema QUIZ[N_THEMES];

void handle_player(struct desc_player* p, fd_set* readfds) {
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
        players_count--;
        show_results();
        return;
    }

    if(strcmp(buffer, QUIT) == 0) {
        printf("Un client ha terminato la connessione.\n");
        remove_player(&players, p->sock);
        endquiz(p->username);
        close(p->sock);
        FD_CLR(p->sock, readfds);
        players_count--;
        show_results();
        return;
    }

    // gestisco il caso in cui mi abbia mandato il nickname
    if(strcmp(p->username, "") == 0) {
        int ret = check_username(buffer);
        if(ret == 1) {
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
        get_quiz_disponibili(buffer);
        send_msg(p->sock, buffer);
        show_results();

        return;
    }

    // Caso in cui ha scelto showscore oppure ha scelto un tema
    if(
        is_some_theme_pending(p) < 0 &&
        (
            (strcmp(buffer, "1") != 0) ||
            (strcmp(buffer, "2") != 0) ||
            (strcmp(buffer, "3") != 0) ||
            (strcmp(buffer, "4") != 0) ||
            (strcmp(buffer, SHOW_SCORE) != 0)
        )
    ) {
        if(strcmp(buffer, SHOW_SCORE) == 0) {
            show_score(p);
            return;
        }

        int theme = atoi(buffer);
        theme--;

        if(!theme_already_completed(p, theme)) {
            p->current_theme = theme;
            if(!p->games[theme].started)
                p->games[theme].started = true;
        
            // devo inviare la domanda corrente del tema theme
            show_results();

            Tema *t = &QUIZ[theme];
            Domanda *d = &t->domande[p->games[theme].current_question];
            sprintf(buffer, "\nQuiz %s\n", t->label);
            strcat(buffer, SEPARATOR);
            strcat(buffer, d->testo);
            strcat(buffer, NEW_LINE);
            send_msg(p->sock, buffer);
            return;
        } else {
            strcpy(buffer, "\nHai già giocato a questo tema!\n");
            get_quiz_disponibili(buffer);
            send_msg(p->sock, buffer);
            return;
        }
    }


    // caso opzione non valida
    if(
        (strcmp(buffer, "1") != 0) &&
        (strcmp(buffer, "2") != 0) &&
        (strcmp(buffer, "3") != 0) &&
        (strcmp(buffer, "4") != 0) &&
        (strcmp(buffer, SHOW_SCORE) != 0) &&
        is_some_theme_pending(p) < 0
    ) {
        strcpy(buffer, "\nScelta del quiz non valida, riprova!\n");
        get_quiz_disponibili(buffer);
        send_msg(p->sock, buffer);
        return;
    }


    // caso in cui manda la risposta ad una domanda
    if(p->current_theme != -1) {        
        if(strcmp(buffer, SHOW_SCORE) == 0) {
            show_score(p);
            return;
        }

        Tema *t = &QUIZ[p->current_theme];

        if(verifica_risposta(t, p->games[p->current_theme].current_question, buffer)) {
            strcpy(buffer, "\nGiusto!\n");
            p->games[p->current_theme].score++;
            if(p->games[p->current_theme].current_question == N_THEMES - 1) {
                // era l'ultima domanda
                p->games[p->current_theme].ended = true;
                p->current_theme = -1;
                show_results();
                strcat(buffer, "\nHai completato il quiz, puoi sceglierne un altro!\n");
                get_quiz_disponibili(buffer);
                send_msg(p->sock, buffer);
                return;
            }
            p->games[p->current_theme].current_question++;
            show_results();
        } else {
            strcpy(buffer, "\nSbagliato, riprova.\n");
        }
        Domanda *d = &t->domande[p->games[p->current_theme].current_question];
        strcat(buffer, d->testo);
        strcat(buffer, NEW_LINE);
        send_msg(p->sock, buffer);
        return;
    }
}

void handler(int sig) {
    printf("Disconnessione del server\n");

    close(server_sock);
}

void handle_new_connection(int server_sock, fd_set* readfds, int* max_sd) {
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

    add_player(&players, client_sock);

    FD_SET(client_sock, readfds);
    if(client_sock > *max_sd)
        *max_sd = client_sock;

    players_count++;

    char msg[BUFFER_SIZE];
    strcpy(msg, TITLE);
    strcat(msg, SEPARATOR);
    strcat(msg, "Scegli un nickname (deve essere univoco):\n");
    send_msg(client_sock, msg);

    return;
}

int main() {
    signal(SIGHUP, handler);
    signal(SIGINT, handler);

    struct sockaddr_in address;

    init_game();
    carica_database();

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
            handle_new_connection(server_sock, &readfds, &max_sd);
        }

        // Scorro i client connessi e registrati e controllo se ci sono dati da leggere
        struct desc_player *p = players;
        while(p != NULL) {
            if(FD_ISSET(p->sock, &master)) {
                handle_player(p, &readfds);
            }
            p = p->next;
        }
    }
    
    close(server_sock);
    return 0;
}