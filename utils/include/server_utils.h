#ifndef SERVER_UTILS_H
#define SERVER_UTILS_H

#include "constants.h"

extern const char* THEMES[N_THEMES];
extern struct desc_player *players;
extern int players_count;
extern Tema QUIZ[N_THEMES];
extern int server_sock;

void init_game();
void get_quiz_disponibili(char* buffer);
void add_player(struct desc_player **head, int sock);
int check_username(char *username);
void remove_player(struct desc_player **head, int sock);
void show_results();
void send_msg(int sd, char* buffer);
void recv_msg(int sd, char* buffer);
void split_risposte(Domanda *d, char *linea);
int carica_tema_da_file(Tema *tema, const char *filename, const char *nome_tema);
void carica_database();
void endquiz(const char* username);
int is_some_theme_pending(struct desc_player* p);
bool theme_already_completed(struct desc_player* p, int theme);
void show_score(struct desc_player *p);
int verifica_risposta(Tema *tema, int domanda_idx, const char *risposta_client);
void reset(char* buffer);
void theme_list();

#endif