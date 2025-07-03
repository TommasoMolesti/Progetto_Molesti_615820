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

int check_username(char *username) {
    Player *p = players;
    while(p != NULL) {
        if(strcmp(p->username, username) == 0) {
            return 1;
        }
        p = p->next;
    }
    return 0;
}

void get_quiz(char* buffer) {
    strcpy(buffer, "\nQuiz disponibili\n");
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

int get_theme_from_file(Theme *t, const char *filename, const char *theme_name) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return -1;

    strcpy(t->label, theme_name);

    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "D:", 2) == 0) {
            if (count >= MAX_QUEST)
                break;

            Question *q = &t->questions[count];
            strncpy(q->text, line + 2, MAX_LEN);

            if (fgets(line, sizeof(line), fp)) {
                line[strcspn(line, "\r\n")] = '\0';
                if (strncmp(line, "R:", 2) == 0) {
                    split_answers(q, line + 2);
                    count++;
                }
            }
        }
    }

    fclose(fp);
    return 0;
}

void get_quiz_database() {
    for(int i=0; i < N_THEMES; i++) {
        char path[256];
        sprintf(path, "./quiz/%d.txt", i+1);
        if (get_theme_from_file(&QUIZ[i], path, THEMES[i]) != 0)
            fprintf(stderr, "Errore nel caricamento del tema %s\n", THEMES[i]);
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

int is_some_theme_pending(Player* p) {
    for (int i = 0; i < N_THEMES; i++) {
        Game *g = &p->games[i];
        if (g->started && !g->ended) {
            return i;
        }
    }
    return -1;
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

    int theme_index = is_some_theme_pending(p);

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