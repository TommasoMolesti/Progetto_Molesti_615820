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

#define PORT 1234
#define MAX_PLAYERS 10
#define BUFFER_SIZE 1024
#define N_QUESTIONS 5
#define N_THEMES 4
#define QUESTION_LENGHT 512

const char* THEMES[N_THEMES] = {"Geografia", "Sport", "Storia", "Tech"};

int server_sock, players_count;

struct desc_game {
    int score;
    int theme; // Indice del tema relativo a THEMES
    bool started;
    bool ended;
    int current_question;
};

struct desc_player {
    char username[20];
    int sock;
    // Per ogni tema c'è un game in corso
    struct desc_game games[N_THEMES];
    struct desc_player *next;
};

struct desc_player *players = NULL;

void add_player(struct desc_player **head, const char *username, int sock) {
    struct desc_player *new_player = (struct desc_player *)malloc(sizeof(struct desc_player));
    if (!new_player) {
        perror("Failed to add player");
        return;
    }

    // da gestire univocità tra giocatori

    strncpy(new_player->username, username, sizeof(new_player->username) - 1);
    new_player->sock = sock;
    new_player->next = *head;
    
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
    
    prev->next = temp->next;
    free(temp);
}

// struct desc_player* find_player(struct desc_player *head, int sock) {
//     struct desc_player *current = head;
//     while (current != NULL) {
//         if (current->sock == sock)
//             return current;
//         current = current->next;
//     }
//     return NULL;
// }


void handler(int sig) {
    printf("Disconnessione del server\n");

    close(server_sock);
}

void init_game() {
    players = NULL;
    players_count = 0;
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
    printf("------\n");
}

int main() {
    signal(SIGHUP, handler);
    signal(SIGINT, handler);

    struct sockaddr_in address;

    // Creazione del socket TCP
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Err: socket() fallita");
        exit(EXIT_FAILURE);
    }

    // Assegnazione indirizzo e porta
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);

    // Binding del socket
    if (bind(server_sock, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Err: bind() fallito");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    // Ascolta le connessioni
    if (listen(server_sock, 10) < 0) {
        perror("Err: listen() fallito");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Trivia Quiz\n");
    printf("++++++++++++++++++++++++++++\n");
    printf("Temi:\n");
    for(int i=0; i < 4; i++) {
        printf("%d - %s\n", i+1, THEMES[i]);
    }
    printf("++++++++++++++++++++++++++++\n");
    printf("\n");
    
    init_game();
    
    
    show_results();
    
    close(server_sock);
    return 0;
}