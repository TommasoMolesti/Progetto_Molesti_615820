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

#include "./include/server_utils.h"
#include "./include/constants.h"

const char* THEMES[N_THEMES] = {"Geografia", "Sport", "Storia", "Tech"};
Player *players = NULL;
int players_count = 0;
Theme QUIZ[N_THEMES];
int server_sock;

void init_game() {
    players = NULL;
    players_count = 0;
}

void add_player(int sock) {
    Player *new_player = (Player *)malloc(sizeof(Player));
    if (!new_player) {
        perror("Impossibile aggiungere il giocatore\n");
        return;
    }

    new_player->sock = sock;
    new_player->next = players;
    new_player->current_theme = -1;
    strcpy(new_player->username, "");

    for (int i = 0; i < N_THEMES; i++) {
        new_player->games[i].score = 0;
        new_player->games[i].theme = i;
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

    while (current != NULL && current->sock != sock) {
        prev = current;
        current = current->next;
    }

    if (current == NULL) {
        perror("Giocatore non trovato\n");
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

void get_quiz(char* buffer) {
    strcat(buffer, "\nQuiz disponibili\n");
    for(int i = 0; i < N_THEMES; i++) {
        char temp[MAX_LEN];
        snprintf(temp, sizeof(temp), "%d - %s\n", i + 1, THEMES[i]);
        strncat(buffer, temp, BUFFER_SIZE - strlen(buffer) - 1);
    }
    strcat(buffer, "La tua scelta:\n");
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
        printf("Punteggio tema %d\n", j + 1);
        p = players;
        while (p != NULL) {
            Game g = p->games[j];
            if (g.started) {
                printf("-%s %d\n", p->username, g.score);
            }
            p = p->next;
        }
        printf(NEW_LINE);
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

void send_msg(int sd, char* buffer) {
    int ret;
    int message_len = strlen(buffer);
    int effective_message_len = htonl(message_len);
    ret = send(sd, &effective_message_len, sizeof(effective_message_len), 0);
    if(ret == -1) {
        perror("Err: send()\n");
        exit(1);
    }

    ret = send(sd, buffer, message_len, 0);
    if(ret == -1) {
        perror("Err: send()\n");
        exit(1);
    }
}

void recv_msg(int sd, char* buffer) {
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

void split_answers(Question *q, char *line) {
    char *token = strtok(line, ";");
    q->answers_count = 0;
    while (token && q->answers_count < MAX_RESP) {
        strncpy(q->answers[q->answers_count], token, MAX_LEN);
        token = strtok(NULL, ";");
        q->answers_count++;
    }
}

int get_theme_from_file(Theme *t, const char *questions_filename, const char *answers_filename, const char *theme_name) {
    FILE *fq = fopen(questions_filename, "r");
    FILE *fa = fopen(answers_filename, "r");

    if (!fq || !fa) {
        if (fq) fclose(fq);
        if (fa) fclose(fa);
        perror("Errore nell'apertura dei file del quiz");
        return -1;
    }

    strcpy(t->label, theme_name);

    char question_line[QUESTION_LENGHT];
    char answer_line[QUESTION_LENGHT];
    int count = 0;

    while (fgets(question_line, sizeof(question_line), fq) && fgets(answer_line, sizeof(answer_line), fa)) {
        if (count >= N_QUEST) {
            break;
        }

        // Rimuovi il carattere di newline
        question_line[strcspn(question_line, "\r\n")] = '\0';
        answer_line[strcspn(answer_line, "\r\n")] = '\0';

        Question *q = &t->questions[count];
        strncpy(q->text, question_line, MAX_LEN - 1);
        q->text[MAX_LEN - 1] = '\0';

        split_answers(q, answer_line);

        count++;
    }

    fclose(fq);
    fclose(fa);
    return 0;
}

void get_quiz_database() {
    for(int i = 0; i < N_THEMES; i++) {
        char questions_path[256];
        char answers_path[256];
        
        sprintf(questions_path, "./quiz/%d_domande.txt", i + 1);
        sprintf(answers_path, "./quiz/%d_risposte.txt", i + 1);
        
        if (get_theme_from_file(&QUIZ[i], questions_path, answers_path, THEMES[i]) != 0) {
            fprintf(stderr, "Errore nel caricamento del tema %s dai file: %s e %s\n", THEMES[i], questions_path, answers_path);
        }
    }
}

void endquiz(const char* username) {
    Player *curr = players;

    while (curr != NULL) {
        if (strcmp(curr->username, username) == 0) {
            break;
        }
        curr = curr->next;
    }
}

bool is_some_theme_pending(Player* p) {
    for (int i = 0; i < N_THEMES; i++) {
        Game *g = &p->games[i];
        if (g->started && !g->ended) {
            return true;
        }
    }
    return false;
}

bool theme_already_completed(Player* p, int theme) {
    Game *g = &p->games[theme];
    return g->ended;
}

void show_score(Player *p) {
    char buffer[BUFFER_SIZE];

    char temp[64];
    for (int j = 0; j < N_THEMES; j++) {
        strcpy(buffer, "Punteggio tema ");
        snprintf(temp, sizeof(temp), "%d\n", j + 1);
        strcat(buffer, temp);
        p = players;
        while (p != NULL) {
            Game g = p->games[j];
            if (g.started) {
                strcat(buffer, "-");
                strcat(buffer, p->username);
                strcat(buffer, " ");
                snprintf(temp, sizeof(temp), "%d\n", g.score);
                strcat(buffer, temp);
            }
            p = p->next;
        }
        strcat(buffer, NEW_LINE);
    }

    for (int j = 0; j < N_THEMES; j++) {
        strcat(buffer, "Quiz tema ");
        snprintf(temp, sizeof(temp), "%d completato\n", j + 1);
        strcat(buffer, temp);
        p = players;
        while (p != NULL) {
            Game g = p->games[j];
            if (g.ended) {
                strcat(buffer, "-");
                strcat(buffer, p->username);
                strcat(buffer, NEW_LINE);
            }
            p = p->next;
        }
        strcat(buffer, NEW_LINE);
    }
    strcat(buffer, SEPARATOR);

    int theme_index = p->current_theme;

    if(theme_index < 0) {
        // nessun tema in corso e gioco ancora non finito
        get_quiz(buffer);
    } else {
        // tema in corso
        printf("Quiz ancora in corso: %d\n", theme_index);
        // Recupero il tema corrente
        Theme *t = &QUIZ[theme_index];
    
        // Recupero il game corrente
        Game *g = &p->games[theme_index];

        Question *q = &t->questions[g->current_question];
        strcat(buffer, q->text);
        strcat(buffer, NEW_LINE);
    }
    
    send_msg(p->sock, buffer);

}

int verify_answer(Theme *t, int answ_index, const char *client_answ) {
    Question *q = &t->questions[answ_index];
    for (int i = 0; i < q->answers_count; i++) {
        if (strcasecmp(q->answers[i], client_answ) == 0) {
            return 1;
        }
    }
    return 0;
}

void reset(char* buffer) {
    memset(buffer, '\0', BUFFER_SIZE);
}

void theme_list() {
    printf("Temi:\n");
    for(int i=0; i < 4; i++) {
        printf("%d - %s\n", i+1, THEMES[i]);
    }
}

bool is_game_ended(Player* p) {
    for (int i = 0; i < N_THEMES; i++) {
        Game *g = &p->games[i];
        if (!g->started || (g->started && !g->ended)) {
            return false;
        }
    }
    return true;
}