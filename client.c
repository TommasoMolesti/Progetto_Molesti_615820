#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>


int main(int argc, char **argv) {
    int sock, port;
    struct sockaddr_in serv_addr;

    if(argc != 2) {
        printf("Numero di argomento errato\n");
    }

    port = atoi(argv[1]);

    // Creazione del socket TCP
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Errore nella creazione del socket\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Converte l'indirizzo IP
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("Indirizzo non valido\n");
        return -1;
    }

    // Connessione al server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connessione fallita\n");
        return -1;
    }

    printf("Connessione stabilita correttamente con il server\n");

    close(sock);
    return 0;
}