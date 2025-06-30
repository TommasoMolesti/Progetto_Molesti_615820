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
#define N_THEMES 4
#define QUESTION_LENGHT 512
#define MAX_RESP 3
#define MAX_QUEST 5
#define MAX_LEN 128

int message_len, effective_message_len;

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

typedef struct {
    char testo[MAX_LEN];
    char risposte[MAX_RESP][MAX_LEN];
    int num_risposte;
} Domanda;

typedef struct {
    char label[MAX_LEN];
    Domanda domande[MAX_QUEST];
} Tema;

Tema QUIZ[N_THEMES];

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

int verifica_risposta(Tema *tema, int domanda_idx, const char *risposta_client) {
    Domanda *d = &tema->domande[domanda_idx];
    for (int i = 0; i < d->num_risposte; i++) {
        if (strcasecmp(d->risposte[i], risposta_client) == 0) {
            return 1;
        }
    }
    return 0;
}

void add_player(struct desc_player **head, int sock) {
    struct desc_player *new_player = (struct desc_player *)malloc(sizeof(struct desc_player));
    if (!new_player) {
        perror("Failed to add player");
        return;
    }

    // da gestire univocità tra giocatori

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

void stampa_quiz() {
    printf("Database del Quiz:\n");
    printf("====================\n");

    // Ciclo attraverso i temi
    for (int i = 0; i < N_THEMES; i++) {
        Tema *tema = &QUIZ[i];  // Prendo il tema corrente
        printf("Tema %d: %s\n", i + 1, tema->label);
        
        // Ciclo attraverso le domande del tema
        for (int j = 0; j < MAX_QUEST; j++) {
            Domanda *domanda = &tema->domande[j];
            if (strlen(domanda->testo) == 0) continue;
            printf("\tDomanda %d: %s\n", j + 1, domanda->testo);
            
            // Ciclo attraverso le risposte
            printf("\t\tRisposte: ");
            for (int k = 0; k < domanda->num_risposte; k++) {
                printf("%s", domanda->risposte[k]);
                if (k < domanda->num_risposte - 1) {
                    printf(", ");
                }
            }
            printf("\n");
        }
        printf("\n");
    }
}

void send_msg(int sd, char* buffer) {
    int ret;
    message_len = strlen(buffer);
    effective_message_len = htonl(message_len); // conversione a network
    // Invio del numero di byte
    ret = send(sd, &effective_message_len, sizeof(effective_message_len), 0);
    if(ret == -1) {
        perror("Err: send()\n");
        exit(1);
    }

    // Invio del messaggio
    ret = send(sd, buffer, message_len, 0);
    if(ret == -1) {
        perror("Err: send()\n");
        exit(1);
    }
}

void recv_msg(int sd, char* buffer) {
    int bytes_read = recv(sd, &effective_message_len, sizeof(effective_message_len), 0);
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

void handle_new_connection(int server_sock, fd_set* readfds, int* max_sd) {
    int client_sock;
    struct sockaddr_in cl_addr;
    int len = sizeof(cl_addr);

    if((client_sock = accept(server_sock, (struct sockaddr*)&cl_addr, (socklen_t*)&len)) < 0) {
        perror("Err: accept() fallito\n");
        return;
    }

    if(players_count >= MAX_PLAYERS) {
        char msg[BUFFER_SIZE];
        strcpy(msg, "Capacità massima del server raggiunta, riprova tra poco.\n");
        send_msg(client_sock, msg);
        close(client_sock);
        return;
    }

    add_player(&players, client_sock);

    FD_SET(client_sock, readfds);
    if(client_sock > *max_sd)
        *max_sd = client_sock;

    players_count++;

    char msg[BUFFER_SIZE];
    strcpy(msg, "Trivia Quiz\n");
    strcat(msg, "+++++++++++++++++++\n");
    strcat(msg, "Scegli un nickname (deve essere univoco):\n");
    send_msg(client_sock, msg);

    return;
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

int game_completed(struct desc_player* p) {
    for (int i = 0; i < N_THEMES; i++) {
        struct desc_game *g = &p->games[i];
        if ((g->started && !g->ended) || !g->started) {
            return i;
        }
    }
    return -1;
}

void show_score(struct desc_player *p) {
    char risultati[BUFFER_SIZE];

    char temp[64];
    for (int j = 0; j < N_THEMES; j++) {
        strcpy(risultati, "Punteggio tema ");
        snprintf(temp, sizeof(temp), "%d\n", j + 1);
        strcat(risultati, temp);
        p = players;
        while (p != NULL) {
            struct desc_game g = p->games[j];
            if (g.started) {
                strcat(risultati, "-");
                strcat(risultati, p->username);
                strcat(risultati, " ");
                snprintf(temp, sizeof(temp), "%d\n", g.score);
                strcat(risultati, temp);
            }
            p = p->next;
        }
        strcat(risultati, "\n");
    }

    for (int j = 0; j < N_THEMES; j++) {
        strcat(risultati, "Quiz tema ");
        snprintf(temp, sizeof(temp), "%d completato\n", j + 1);
        strcat(risultati, temp);
        p = players;
        while (p != NULL) {
            struct desc_game g = p->games[j];
            if (g.ended) {
                strcat(risultati, "-");
                strcat(risultati, p->username);
                strcat(risultati, "\n");
            }
            p = p->next;
        }
        strcat(risultati, "\n");
    }
    strcat(risultati, "------\n");

    int theme_index = is_some_theme_pending(p);
    int game_index = game_completed(p);

    if(theme_index < 0 && game_index < 0) {
        // nessun tema in corso e gioco ancora non finito
        strcpy(risultati, "Quiz disponibili\n");
        for(int i=0; i < 4; i++) {
            char temp[20];
            snprintf(temp, sizeof(temp), "%d - %s\n", i + 1, THEMES[i]);
        }
        strcat(risultati, "La tua scelta:\n");
    } else {
        // tema in corso
        
        // Recupero il tema corrente
        // Tema *tema = &QUIZ[theme_index];
    
        // Recupero il game corrente
        // struct desc_game g = p->games[game_index];

        // Domanda *domanda = &tema->domande[g->current_question];
        // strcat(risultati, domanda->testo);
        // strcat(risultati, "\n");
    }
    
    send_msg(p->sock, risultati);

}


void handle_player(struct desc_player* p, fd_set* readfds) {
    char buffer[BUFFER_SIZE];
    char risultato[BUFFER_SIZE];

    memset(buffer, '\0', sizeof(buffer));
    recv_msg(p->sock, buffer);


    if(strcmp(buffer, "endquiz") == 0) {
        strcpy(risultato, "endquiz");
        send_msg(p->sock, risultato);
        endquiz(p->username);
        close(p->sock);
        FD_CLR(p->sock, readfds);
        players_count--;
        show_results();
        return;
    }

    if(strcmp(buffer, "quit") == 0) {
        printf("Un client ha terminato la connessione.\n");
        endquiz(p->username);
        close(p->sock);
        FD_CLR(p->sock, readfds);
        players_count--;
        show_results();
        return;
    }

    // gestisco il caso in cui mi abbia mandato il nickname
    if(strcmp(p->username, "") == 0) {
        int ret = check_username(buffer);
        if(ret == 1) {
            // username non valido
            char msg[BUFFER_SIZE];
            snprintf(msg, sizeof(msg), "\nUsername non disponibile\nTrivia Quiz\n++++++++++++++++++++++++++++\nScegli un username (univoco)\n");
            send_msg(p->sock, msg);
            return;
        }

        strcpy(p->username, buffer);
        memset(buffer, '\0', sizeof(buffer));
        strcpy(buffer, "\nQuiz disponibili\n");
        for(int i=0; i < 4; i++) {
            char temp[20];
            snprintf(temp, sizeof(temp), "%d - %s\n", i + 1, THEMES[i]);
            strncat(buffer, temp, sizeof(buffer) - strlen(buffer) - 1);
        }
        strcat(buffer, "La tua scelta:\n");
        send_msg(p->sock, buffer);
        show_results();

        return;
    }

    if(
        is_some_theme_pending(p) < 0 &&
        (
            (strcmp(buffer, "1") != 0) ||
            (strcmp(buffer, "2") != 0) ||
            (strcmp(buffer, "3") != 0) ||
            (strcmp(buffer, "4") != 0) ||
            (strcmp(buffer, "show score") != 0)
        )
    ) {
        if(strcmp(buffer, "show score") == 0) {
            show_score(p);
            return;
        }

        if(game_completed(p) > -1) {

        }
    }


    // caso opzione non valida
    if(
        (strcmp(buffer, "1") != 0) &&
        (strcmp(buffer, "2") != 0) &&
        (strcmp(buffer, "3") != 0) &&
        (strcmp(buffer, "4") != 0) &&
        (strcmp(buffer, "show score") != 0) &&
        is_some_theme_pending(p) < 0
    ) {
        strcpy(risultato, "\nScelta del quiz non valida, riprova!\n");
        strcpy(risultato, "Quiz disponibili\n");
        for(int i=0; i < 4; i++) {
            char temp[20];
            snprintf(temp, sizeof(temp), "%d - %s\n", i + 1, THEMES[i]);
            strncat(buffer, temp, sizeof(buffer) - strlen(buffer) - 1);
        }
        strcat(buffer, "La tua scelta:\n");
        send_msg(p->sock, risultato);
        return;
    }
}

int main() {
    signal(SIGHUP, handler);
    signal(SIGINT, handler);

    struct sockaddr_in address;

    init_game();
    carica_database();

    // Creazione del socket TCP
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Err: socket() fallita");
        exit(EXIT_FAILURE);
    }

    // Assegnazione indirizzo e porta
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = INADDR_ANY;

    // Binding del socket
    if (bind(server_sock, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Err: bind() fallito");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    // Ascolta le connessioni
    if (listen(server_sock, MAX_PLAYERS) < 0) {
        perror("Err: listen() fallito");
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
    
    show_results();
    // stampa_quiz();

    fd_set readfds;
    int max_sd, activity;

    FD_ZERO(&readfds);
    FD_SET(server_sock, &readfds);
    max_sd = server_sock;

    while(1) {
        fd_set master = readfds;

        activity = select(max_sd+1, &master, NULL, NULL, NULL);
        if((activity < 0) && (errno != EINTR)) {
            perror("Err : select() fallito\n");
            break;
        }

        if(FD_ISSET(server_sock, &master)) {
            handle_new_connection(server_sock, &readfds, &max_sd);
        }

        // Scorro i client connessi e registrati e controllo se ci sono dati da leggere
        struct desc_player *p = players;
        while(p != NULL) {
            if(FD_ISSET(p->sock, &master)) {
                handle_player(p, &readfds);
            }
            p = p->next;
        }
    }
    
    close(server_sock);
    return 0;
}