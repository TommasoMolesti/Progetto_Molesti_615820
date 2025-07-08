#include <stdbool.h>

#ifndef CONSTANTS
#define CONSTANTS

#define PORT 1234 // Porta di default
#define MAX_PLAYERS 10
#define BUFFER_SIZE 1024
#define N_THEMES 4
#define QUESTION_LENGHT 512
#define MAX_RESP 3 // Massimo numero di risposte per ogni domanda
#define N_QUEST 5
#define MAX_LEN 128
#define MAX_USERNAME 20 // Lunghezza max

// Stringhe varie
#define TITLE "Trivia Quiz\n"
#define SEPARATOR "++++++++++++++++++++++++++++\n"
#define ENDQUIZ "endquiz"
#define SHOW_SCORE "show score"
#define EXIT "exit"
#define FINISHED "finished"
#define NEW_LINE "\n"

// Definisce un "gioco", cioè una sessione di gioco relativa ad un tema
typedef struct {
    int score;
    bool started;
    bool ended;
    int current_question; // Per vedere a che punto sono
} Game;

typedef struct desc_player {
    char username[MAX_USERNAME];
    int sock;
    Game games[N_THEMES]; // Array di giochi, uno per ogni tema
    struct desc_player *next;
    int current_theme; // Può giocare ad un tema alla volta
} Player;

typedef struct {
    char text[MAX_LEN];
    char answers[MAX_RESP][MAX_LEN]; // Massimo MAX_RESP risposte per ogni tema
    int answers_count;
} Question;

typedef struct {
    char label[MAX_LEN];
    Question questions[N_QUEST];
} Theme;

#endif