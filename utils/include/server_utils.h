#ifndef SERVER_UTILS_H
#define SERVER_UTILS_H

#include "constants.h"

extern const char* THEMES[N_THEMES];
extern Player *players;
extern int players_count;
extern Theme QUIZ[N_THEMES];
extern int server_sock;

void init_game();
void get_quiz(char* buffer);
void add_player(int sock);
bool verify_username(char *username);
void remove_player(int sock);
void show_results();
void send_msg(int sd, char* buffer);
void recvmsg(int sd, char* buffer);
void split_answers(Question *q, char *line);
int get_theme_from_file(Theme *t, const char *questions_filename, const char *answers_filename, const char *theme_name);
void get_quiz_database();
bool is_some_theme_pending(Player* p);
bool theme_already_completed(Player* p, int theme);
void show_score(Player *p);
bool verify_answer(Theme *t, int answ_index, const char *client_answ);
void reset(char* buffer);
void theme_list();
bool is_game_ended(Player* p);

#endif