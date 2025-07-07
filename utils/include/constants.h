#include <stdbool.h>

#ifndef CONSTANTS
#define CONSTANTS

#define PORT 1234
#define MAX_PLAYERS 10
#define BUFFER_SIZE 1024
#define N_THEMES 4
#define QUESTION_LENGHT 512
#define MAX_RESP 3
#define N_QUEST 5
#define MAX_LEN 128
#define MAX_USERNAME 20

#define TITLE "Trivia Quiz\n"
#define SEPARATOR "++++++++++++++++++++++++++++\n"
#define ENDQUIZ "endquiz"
#define SHOW_SCORE "show score"
#define EXIT "exit"
#define FINISHED "finished"
#define NEW_LINE "\n"

typedef struct {
    int score;
    int theme;
    bool started;
    bool ended;
    int current_question;
} Game;

typedef struct desc_player {
    char username[MAX_USERNAME];
    int sock;
    Game games[N_THEMES];
    struct desc_player *next;
    int current_theme;
} Player;

typedef struct {
    char text[MAX_LEN];
    char answers[MAX_RESP][MAX_LEN];
    int answers_count;
} Question;

typedef struct {
    char label[MAX_LEN];
    Question questions[N_QUEST];
} Theme;

#endif