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

void init_game() {
    players = NULL;
    players_count = 0;
}

void add_player(struct desc_player **head, int sock) {
    struct desc_player *new_player = (struct desc_player *)malloc(sizeof(struct desc_player));
    if (!new_player) {
        perror("Failed to add player");
        return;
    }

    new_player->sock = sock;
    new_player->next = *head;
    new_player->current_theme = -1;
    strcpy(new_player->username, "");

    for (int i = 0; i < N_THEMES; i++) {
        new_player->games[i].score = 0;
        new_player->games[i].theme = i;
        new_player->games[i].started = false;
        new_player->games[i].ended = false;
        new_player->games[i].current_question = 0;
    }

    *head = new_player;
}

void remove_player(struct desc_player **head, int sock) {
    struct desc_player *temp = *head, *prev = NULL;

    if (temp != NULL && temp->sock == sock) {
        *head = temp->next;
        free(temp);
        return;
    }

    while (temp != NULL && temp->sock != sock) {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL)
        return;

    if (prev != NULL) { // Ensure prev is not NULL before dereferencing
        prev->next = temp->next;
    }
    free(temp);
}

int check_username(char *username) {
    struct desc_player *p = players;
    while(p != NULL) {
        if(strcmp(p->username, username) == 0) {
            return 1;
        }
        p = p->next;
    }
    return 0;
}

void get_quiz_disponibili(char* buffer) {
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
    struct desc_player *p = players;
    
    while (p != NULL) {
        printf("-%s\n", p->username);
        p = p->next;
    }

    printf("\n");

    for (int j = 0; j < N_THEMES; j++) {
        printf("Punteggio tema %d\n", j + 1);
        p = players;
        while (p != NULL) {
            struct desc_game g = p->games[j];
            if (g.started) {
                printf("-%s %d\n", p->username, g.score);
            }
            p = p->next;
        }
        printf("\n");
    }

    for (int j = 0; j < N_THEMES; j++) {
        printf("Quiz tema %d completato\n", j + 1);
        p = players;
        while (p != NULL) {
            struct desc_game g = p->games[j];
            if (g.ended) {
                printf("-%s\n", p->username);
            }
            p = p->next;
        }
        printf("\n");
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

void split_risposte(Domanda *d, char *linea) {
    char *token = strtok(linea, ";");
    d->num_risposte = 0;
    while (token && d->num_risposte < MAX_RESP) {
        strncpy(d->risposte[d->num_risposte], token, MAX_LEN);
        token = strtok(NULL, ";");
        d->num_risposte++;
    }
}

int carica_tema_da_file(Tema *tema, const char *filename, const char *nome_tema) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return -1;

    strcpy(tema->label, nome_tema);

    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "D:", 2) == 0) {
            if (count >= MAX_QUEST) break;

            Domanda *d = &tema->domande[count];
            strncpy(d->testo, line + 2, MAX_LEN);

            if (fgets(line, sizeof(line), fp)) {
                line[strcspn(line, "\r\n")] = '\0';
                if (strncmp(line, "R:", 2) == 0) {
                    split_risposte(d, line + 2);
                    count++;
                }
            }
        }
    }

    fclose(fp);
    return 0;
}

void carica_database() {
    for(int i=0; i < N_THEMES; i++) {
        char path[256];
        sprintf(path, "./quiz/%d.txt", i+1);
        if (carica_tema_da_file(&QUIZ[i], path, THEMES[i]) != 0)
            fprintf(stderr, "Errore nel caricamento del tema %s\n", THEMES[i]);
    }
}

void endquiz(const char* username) {
    struct desc_player *curr = players;

    while (curr != NULL) {
        if (strcmp(curr->username, username) == 0) {
            break;
        }
        curr = curr->next;
    }
}

int is_some_theme_pending(struct desc_player* p) {
    for (int i = 0; i < N_THEMES; i++) {
        struct desc_game *g = &p->games[i];
        if (g->started && !g->ended) {
            return i;
        }
    }
    return -1;
}

bool theme_already_completed(struct desc_player* p, int theme) {
    struct desc_game *g = &p->games[theme];
    return g->ended;
}

void show_score(struct desc_player *p) {
    char buffer[BUFFER_SIZE];

    char temp[64];
    for (int j = 0; j < N_THEMES; j++) {
        strcpy(buffer, "Punteggio tema ");
        snprintf(temp, sizeof(temp), "%d\n", j + 1);
        strcat(buffer, temp);
        p = players;
        while (p != NULL) {
            struct desc_game g = p->games[j];
            if (g.started) {
                strcat(buffer, "-");
                strcat(buffer, p->username);
                strcat(buffer, " ");
                snprintf(temp, sizeof(temp), "%d\n", g.score);
                strcat(buffer, temp);
            }
            p = p->next;
        }
        strcat(buffer, "\n");
    }

    for (int j = 0; j < N_THEMES; j++) {
        strcat(buffer, "Quiz tema ");
        snprintf(temp, sizeof(temp), "%d completato\n", j + 1);
        strcat(buffer, temp);
        p = players;
        while (p != NULL) {
            struct desc_game g = p->games[j];
            if (g.ended) {
                strcat(buffer, "-");
                strcat(buffer, p->username);
                strcat(buffer, "\n");
            }
            p = p->next;
        }
        strcat(buffer, "\n");
    }
    strcat(buffer, SEPARATOR);

    int theme_index = is_some_theme_pending(p);

    if(theme_index < 0) {
        // nessun tema in corso e gioco ancora non finito
        get_quiz_disponibili(buffer);
    } else {
        // tema in corso
        printf("Quiz ancora in corso: %d\n", theme_index);
        // Recupero il tema corrente
        Tema *tema = &QUIZ[theme_index];
    
        // Recupero il game corrente
        struct desc_game *g = &p->games[theme_index];

        Domanda *domanda = &tema->domande[g->current_question];
        strcat(buffer, domanda->testo);
        strcat(buffer, "\n");
    }
    
    send_msg(p->sock, buffer);

}

int verifica_risposta(Tema *tema, int domanda_idx, const char *risposta_client) {
    Domanda *d = &tema->domande[domanda_idx];
    for (int i = 0; i < d->num_risposte; i++) {
        if (strcasecmp(d->risposte[i], risposta_client) == 0) {
            return 1;
        }
    }
    return 0;
}