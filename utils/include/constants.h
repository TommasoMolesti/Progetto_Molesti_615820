#include <stdbool.h>

#ifndef CONSTANTS
#define CONSTANTS

#define PORT 1234
#define MAX_PLAYERS 10
#define BUFFER_SIZE 1024
#define N_THEMES 4
#define QUESTION_LENGHT 512
#define MAX_RESP 3
#define MAX_QUEST 5
#define MAX_LEN 128

#define TITLE "Trivia Quiz\n"
#define SEPARATOR "++++++++++++++++++++++++++++\n"
#define ENDQUIZ "endquiz"
#define SHOW_SCORE "show score"
#define QUIT "quit"
#define NEW_LINE "\n"

typedef struct {
    int score;
    int theme;
    bool started;
    bool ended;
    int current_question;
} Game;

typedef struct desc_player {
    char username[20];
    int sock;
    Game games[N_THEMES];
    struct desc_player *next;
    int current_theme;
} Player;

typedef struct {
    char testo[MAX_LEN];
    char risposte[MAX_RESP][MAX_LEN];
    int num_risposte;
} Domanda;

typedef struct {
    char label[MAX_LEN];
    Domanda domande[MAX_QUEST];
} Tema;

#endif