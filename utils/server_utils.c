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

#include "./include/server_utils.h"
#include "./include/constants.h"

const char* THEMES[N_THEMES] = {"Geografia", "Sport", "Storia", "Tech"}; // Le label dei temi
Player *players = NULL; // Lista dei giocatori
int players_count = 0;
Theme QUIZ[N_THEMES]; // "Database" che contiene domande e risposte
int server_sock;

// Struttura di appoggio per ordinare in modo più facile i giocatori in ordine decrescente
typedef struct PlayerScoreNode {
    char username[MAX_USERNAME];
    int score;
    struct PlayerScoreNode *next;
} PlayerScoreNode;

void init_game() {
    players = NULL;
    players_count = 0;
}

void add_player(int sock) {
    Player *new_player = (Player *)malloc(sizeof(Player));
    if (!new_player) {
        perror("Err: impossibile aggiungere il giocatore\n");
        return;
    }

    new_player->sock = sock;
    new_player->next = players;
    new_player->current_theme = -1;
    strcpy(new_player->username, ""); // All'inizio il suo username sarà vuoto

    for (int i = 0; i < N_THEMES; i++) {
        new_player->games[i].score = 0;
        new_player->games[i].started = false;
        new_player->games[i].ended = false;
        new_player->games[i].current_question = 0;
    }

    players_count++;

    players = new_player;
}

void remove_player(int sock) {
    Player *current = players;
    Player *prev = NULL;

    // Rimozione di un elemento da una lista
    while (current != NULL && current->sock != sock) {
        prev = current;
        current = current->next;
    }

    if (current == NULL) {
        perror("Err : giocatore non trovato\n");
        return;
    }

    if (prev == NULL) {
        players = current->next;
    } else {
        prev->next = current->next;
    }
    free(current);
    players_count--;
}

// Per verificare se l'username è ok: cioè se non ci sono omonimi già registrati
bool verify_username(char *username) {
    Player *p = players;
    while(p != NULL) {
        if(strcmp(p->username, username) == 0) {
            return false;
        }
        p = p->next;
    }
    return true;
}

// Stampa semplicemente i quiz disponibili
void get_quiz(char* buffer) {
    strcat(buffer, "\nQuiz disponibili\n");
    for(int i = 0; i < N_THEMES; i++) {
        char temp[MAX_LEN];
        snprintf(temp, sizeof(temp), "%d - %s\n", i + 1, THEMES[i]);
        strncat(buffer, temp, BUFFER_SIZE - strlen(buffer) - 1);
    }
    strcat(buffer, "La tua scelta:\n");
}

// Per pulire la lista dei PlayerScoreNode
void reset_player_score_list(PlayerScoreNode *head) {
    PlayerScoreNode *current = head;
    PlayerScoreNode *next_node;
    while (current != NULL) {
        next_node = current->next;
        free(current);
        current = next_node;
    }
}

// Inserisco già nella lista PlayerScoreNode in modo ordinato, così non devo ordinare dopo
void insert_sorted(PlayerScoreNode **head, const char *username, int score) {
    PlayerScoreNode *new_node = (PlayerScoreNode *)malloc(sizeof(PlayerScoreNode));
    if (!new_node) {
        perror("Err: malloc per PlayerScoreNode");
        return;
    }
    strcpy(new_node->username, username);
    new_node->score = score;
    new_node->next = NULL;

    if (*head == NULL || new_node->score >= (*head)->score) {
        new_node->next = *head;
        *head = new_node;
    } else {
        PlayerScoreNode *current = *head;
        while (current->next != NULL && new_node->score < current->next->score) {
            current = current->next;
        }
        new_node->next = current->next;
        current->next = new_node;
    }
}

void show_results() {
    printf("Partecipanti (%d)\n", players_count);
    Player *p = players;
    
    while (p != NULL) {
        printf("-%s\n", p->username);
        p = p->next;
    }

    printf(NEW_LINE);

    for (int j = 0; j < N_THEMES; j++) {
        PlayerScoreNode *theme_scores_head = NULL;

        Player *current_p = players;
        while (current_p != NULL) {
            Game g = current_p->games[j];
            /*
                Se effettivamente questo giocatore ha giocato o sta giocando al tema
                allora devo inserirlo nella lista "theme_scores_head", per poi ordinarlo
                e mostrarlo
            */
            if (g.started) {
                insert_sorted(&theme_scores_head, current_p->username, g.score);
            }
            current_p = current_p->next;
        }

        printf("Punteggio tema %d\n", j + 1);

        PlayerScoreNode *current_score_node = theme_scores_head;
        while (current_score_node != NULL) {
            printf("-%s %d\n", current_score_node->username, current_score_node->score);
            current_score_node = current_score_node->next;
        }
        printf(NEW_LINE);

        reset_player_score_list(theme_scores_head); // Pulizia della lista di appoggio
    }

    for (int j = 0; j < N_THEMES; j++) {
        printf("Quiz tema %d completato\n", j + 1);
        p = players;
        while (p != NULL) {
            Game g = p->games[j];
            if (g.ended) {
                printf("-%s\n", p->username);
            }
            p = p->next;
        }
        printf(NEW_LINE);
    }
    printf(SEPARATOR);
}

// Funzione di utilità per mandare un messaggio al client
void sendmsg(int sd, char* buffer) {
    int ret;
    int message_len = strlen(buffer);
    int effective_message_len = htonl(message_len);
    ret = send(sd, &effective_message_len, sizeof(effective_message_len), 0);
    if(ret == -1) {
        perror("Err: send() effective_message_len\n");
        exit(1);
    }

    ret = send(sd, buffer, message_len, 0);
    if(ret == -1) {
        perror("Err: send() buffer\n");
        exit(1);
    }
}

// Funzione di utilità per ricevere un messaggio dal client
void recvmsg(int sd, char* buffer) {
    int bytes_read;
    int effective_message_len;
    int message_len;
    bytes_read = recv(sd, &effective_message_len, sizeof(effective_message_len), 0);
    if(bytes_read == -1) {
        printf("Client disconnesso\n");
        close(sd);
        return;
    }

    message_len = ntohl(effective_message_len);
    bytes_read = recv(sd, buffer, message_len, 0);
    if(bytes_read == -1) {
        printf("Client disconnesso\n");
        close(sd);
        return;
    }

    buffer[bytes_read] = '\0';
}

/*
    Ogni risposta è divisa dall'altra da un ";".
    Questa funzione prende una stringa "line", la splitta e riempie
    q->answers con i vari elementi che trova splittando
*/
void split_answers(Question *q, char *line) {
    char *token = strtok(line, ";"); // Prendo la stringa prima di ";"
    q->answers_count = 0;
    // Se ci sono più risposte, tronco a MAX_RESP
    while (token && q->answers_count < MAX_RESP) {
        strncpy(q->answers[q->answers_count], token, MAX_LEN);
        token = strtok(NULL, ";"); // Cerca il prossimo delimitatore, partendo dalla stringa di adesso
        q->answers_count++;
    }
}

/*
    Rispetto ad un tema, prende i due file (domande e risposte) e carica le strutture dati con le informazioni
*/
int get_theme_from_file(Theme *t, const char *questions_filename, const char *answers_filename, const char *theme_name) {
    // Apro i file in lettura
    FILE *fq = fopen(questions_filename, "r");
    FILE *fa = fopen(answers_filename, "r");

    if (!fq || !fa) {
        if (fq)
            fclose(fq);
        if (fa)
            fclose(fa);
        perror("Err : apertura dei file del quiz\n");
        return -1;
    }

    strcpy(t->label, theme_name);

    char question_line[QUESTION_LENGHT];
    char answer_line[QUESTION_LENGHT];
    int count = 0;

    // Scorro i file in parallelo (le righe sono associate tra di loro)
    while (fgets(question_line, sizeof(question_line), fq) && fgets(answer_line, sizeof(answer_line), fa)) {
        if (count >= N_QUEST) { // Tronco comunque ad un numero massimo N_QUEST
            break;
        }

        // Rimuovo il carattere di newline
        question_line[strcspn(question_line, "\r\n")] = '\0';
        answer_line[strcspn(answer_line, "\r\n")] = '\0';

        // Carico le domande
        Question *q = &t->questions[count];
        strncpy(q->text, question_line, MAX_LEN - 1);
        q->text[MAX_LEN - 1] = '\0';

        // Carico le risposte
        split_answers(q, answer_line);

        count++;
    }

    fclose(fq);
    fclose(fa);
    return 0;
}

/*
    Funzione che scorre i temi e va a pescare i corretti file per passarli alla funzione "get_theme_from_file"
*/
void get_quiz_database() {
    for(int i = 0; i < N_THEMES; i++) {
        char questions_path[MAX_LEN];
        char answers_path[MAX_LEN];
        
        sprintf(questions_path, "./quiz/%d_domande.txt", i + 1);
        sprintf(answers_path, "./quiz/%d_risposte.txt", i + 1);
        
        if (get_theme_from_file(&QUIZ[i], questions_path, answers_path, THEMES[i]) != 0) {
            fprintf(stderr, "Errore nel caricamento del tema %s dai file: %s e %s\n", THEMES[i], questions_path, answers_path);
        }
    }
}

// Cerco se il giocatore ha un tema in corso a cui sta giocando
bool is_some_theme_pending(Player* p) {
    for (int i = 0; i < N_THEMES; i++) {
        Game *g = &p->games[i];
        if (g->started && !g->ended) {
            return true;
        }
    }
    return false;
}

// Controllo se il tema è già stato completato
bool theme_already_completed(Player* p, int theme) {
    Game *g = &p->games[theme];
    return g->ended;
}

/*
    Mostro i punteggi al client dopo che ha inviato "show score".
    Praticamente uguale a show_results, solo che concatena il risultato nel buffer e poi spedisce al client.
    Alla fine poi mostra al client il continuo del gioco
*/
void show_score(Player *p) {
    char buffer[BUFFER_SIZE];
    reset(buffer);
    strcpy(buffer, NEW_LINE);
    strcat(buffer, "Show score\n");
    strcat(buffer, SEPARATOR);

    char temp[64];
    for (int j = 0; j < N_THEMES; j++) {
        PlayerScoreNode *theme_scores_head = NULL;

        Player *current_p = players;
        while (current_p != NULL) {
            Game g = current_p->games[j];
            if (g.started) {
                insert_sorted(&theme_scores_head, current_p->username, g.score);
            }
            current_p = current_p->next;
        }

        strcat(buffer, "Punteggio tema ");
        snprintf(temp, sizeof(temp), "%d\n", j + 1);
        strcat(buffer, temp);

        PlayerScoreNode *current_score_node = theme_scores_head;
        while (current_score_node != NULL) {
            strcat(buffer, "-");
            strcat(buffer, current_score_node->username);
            strcat(buffer, " ");
            snprintf(temp, sizeof(temp), "%d\n", current_score_node->score);
            strcat(buffer, temp);
            current_score_node = current_score_node->next;
        }
        strcat(buffer, NEW_LINE);

        reset_player_score_list(theme_scores_head);
    }

    strcat(buffer, SEPARATOR);


    if(p->current_theme < 0) {
        // Nessun tema in corso e gioco ancora non finito
        get_quiz(buffer);
    } else {
        // Recupero il tema corrente
        Theme *t = &QUIZ[p->current_theme];
    
        // Recupero il game corrente
        Game *g = &p->games[p->current_theme];

        Question *q = &t->questions[g->current_question];
        strcat(buffer, q->text);
        strcat(buffer, NEW_LINE);
    }
    
    sendmsg(p->sock, buffer);

}

// Controlla se la risposta data dal client è giusta, avendo il tema e l'index della risposta
bool verify_answer(Theme *t, int answ_index, const char *client_answ) {
    Question *q = &t->questions[answ_index];
    for (int i = 0; i < q->answers_count; i++) {
        if (strcasecmp(q->answers[i], client_answ) == 0) { // Controllo in tutte le risposte possibili
            return true;
        }
    }
    return false;
}

// Per resettare il buffer
void reset(char* buffer) {
    memset(buffer, '\0', BUFFER_SIZE);
}

// Semplice stampa dei temi
void theme_list() {
    printf("Temi:\n");
    for(int i=0; i < 4; i++) {
        printf("%d - %s\n", i+1, THEMES[i]);
    }
}

// Controllo se il giocatore ha giocato e finito tutti i temi, quindi se ha finito il gioco
bool is_game_ended(Player* p) {
    for (int i = 0; i < N_THEMES; i++) {
        Game *g = &p->games[i];
        if (!g->started || (g->started && !g->ended)) {
            return false;
        }
    }
    return true;
}