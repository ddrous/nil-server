#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BOOK_NAME_SIZE 10                   /* Taille maximale d'un nom de livre */
#define    MAX_TCP_SEG_SIZE 65535              /* Taille maximale du seqment TCP autorisé */

void usage(char *argv0) {
    fprintf(stderr, "usage: %s serveur port livre_1 [livre_2 ...]\n\n", argv0);
    exit(1);
}

void raler(char *msg) {
    perror(msg);
    fprintf(stderr, "\n");
    exit(1);
}

void pad_book_name(const char *inName, char *outName);

int make_request_segment(char *const books[], const int books_size,
                         uint8_t *segment);

void write_to_server(int s, char *const books[], int books_size);

void read_from_server(int s, char *const books[], int books_size);

int main(int argc, char *argv[]) {

    printf("\n------ CLIENT ------\n");

    char *host = NULL, *service = NULL;
    char *cause;
    int s, r;
    struct addrinfo hints, *res, *res0;

    // Test des arguments
    if (argc >= 4) {
        host = argv[1];
        service = argv[2];
    } else
        usage(argv[0]);

    for (int i = 3; i < argc; ++i) {
        if (strlen(argv[i]) > 10) {
            fprintf(stderr,
                    "\nTaille de la reférence bibliographique %d trop longue\n\n",
                    i - 2);
            exit(1);
        }
    }

    // Récupération des addresses IP du serveur (librairie ou Nil)
    memset(&hints, 0, sizeof hints);
    hints.ai_family = PF_UNSPEC;       // IPv6 ou IPv4
    hints.ai_socktype = SOCK_STREAM;   // TCP
    r = getaddrinfo(host, service, &hints, &res0);
    if (r != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(r));
        exit(1);
    }

    // Connection au serveur
    s = -1;
    for (res = res0; res != NULL; res = res->ai_next) {
        s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (s == -1)
            cause = "socket";
        else {
            r = connect(s, res->ai_addr, res->ai_addrlen);
            if (r == -1) {
                cause = "connect";
                close(s);
                s = -1;
            } else break;
        }
    }
    if (s == -1) raler(cause);
    freeaddrinfo(res0);

    // Création du stock de livres à envoyer
    int requested_books_size = argc - 3;
    char *requested_books[requested_books_size];
    for (int i = 3; i < argc; ++i)
        requested_books[i - 3] = argv[i];

    // Envoi des données (indifféremment à la librarie comme au serveur)
    printf("\nEnvoi requête à %s/%s", argv[1], argv[2]);
    write_to_server(s, requested_books, requested_books_size);

    /* Boucle d'ettente de la réponse du serveur */
    for (;;) {

        fd_set readfds;
        FD_ZERO (&readfds);
        FD_SET (s, &readfds);

        if (select(s + 1, &readfds, NULL, NULL, NULL) == -1)
            raler("select");

        if (FD_ISSET (s, &readfds)) {
            read_from_server(s, requested_books, requested_books_size);
            close(s);
        }

        break;
    }

    exit(0);
}


// Construction d'une reference bibliograpgique conforme
void pad_book_name(const char *inName, char *outName) {
    size_t len = strlen(inName);
    strcpy(outName, inName);
    for (int i = len; i <= BOOK_NAME_SIZE; ++i)
        outName[i] = '\0';
}

// Condtruction d'un segment TCP du client vers une librairie, ou vers Nil
int make_request_segment(char *const books[], const int books_size,
                         uint8_t *segment) {
    uint16_t nb = books_size;
    memcpy(segment, &nb, sizeof(uint16_t));

    int segment_size = sizeof(uint16_t);
    char book[BOOK_NAME_SIZE + 1];
    for (int i = 0; i < books_size; ++i) {
        pad_book_name(books[i], book);
        memcpy(segment + segment_size, book, BOOK_NAME_SIZE);
        segment_size += BOOK_NAME_SIZE;
    }

    return segment_size;
}

// Envoie des données au serveur
void write_to_server(int s, char *const books[], int books_size) {
    uint8_t request_buf[MAX_TCP_SEG_SIZE];
    int r;

    int segment_size = make_request_segment(books, books_size, request_buf);
    if (segment_size < 2) raler("Erreur création de segment TCP");

    r = write(s, request_buf, segment_size);
    if (r < 2) raler("Envoi segment au serveur");
    printf("\n-- Sent %d bytes", r);
}

// Recupération de l'id d'un des livres
int get_requested_book_index(char *book, char *const books[], int size) {
    int index = 0;

    while (index < size && strcmp(book, books[index]) != 0) index++;

    // Livre non trouvé
    if (index == size) return -1;
    else return index;
}

// Lecture de la réponse du serveur
void read_from_server(int s, char *const books[], int books_size) {
    uint8_t reply_buf[MAX_TCP_SEG_SIZE];
    int size_read;
    int size_of_nb = sizeof(uint16_t);
    char book[BOOK_NAME_SIZE + 1];
    book[10] = '\0';
    uint8_t status_or_ipver, status, ipver;

    size_read = read(s, reply_buf, MAX_TCP_SEG_SIZE);
    if (size_read < 2) raler("Erreur reception message Nil");

    printf("\n\n-- Recieved %d bytes", size_read);

    // Identification de la nature de la source du paquet: (Librarie ou Nil)
    memcpy(&status_or_ipver, &reply_buf[size_of_nb + BOOK_NAME_SIZE], 1);

    uint16_t nb = *(uint16_t *) (reply_buf);
    int available_indices[nb];
    for (int i = 0; i < nb; ++i) available_indices[i] = -1;

    if (size_read > 2) {
        if (status_or_ipver == 0 || status_or_ipver == 1) {
            uint8_t reply[BOOK_NAME_SIZE + 1];
            for (int i = 0; i < nb; ++i) {
                memcpy(reply, reply_buf + size_of_nb + i * (BOOK_NAME_SIZE + 1),
                       BOOK_NAME_SIZE + 1);
                memcpy(book, reply, BOOK_NAME_SIZE);
                printf("\n%s : ", book);

                memcpy(&status, &reply[BOOK_NAME_SIZE], 1);
                switch (status) {
                    case 0:
                        printf("Pas disponible");
                        break;
                    case 1:
                        printf("Commande validée!");
                        break;
                    default:
                        raler("Segment TCP envoyé par la librairie pas conforme");
                }
            }
        } else if (status_or_ipver == 4 || status_or_ipver == 6) { // Nil
            char netw_addr[16], pres_adr[INET6_ADDRSTRLEN];
            uint16_t port;

            for (int i = 0; i < nb; ++i) {
                memcpy(book, reply_buf + 2 + i * 29, 10);
                memcpy(&ipver, reply_buf + 2 + i * 29 + 10, 1);
                memcpy(netw_addr, reply_buf + 2 + i * 29 + 11, 16);
                memcpy(&port, reply_buf + 2 + i * 29 + 27, 2);

                available_indices[i] = get_requested_book_index(book, books,
                                                                books_size);
                printf("\n%s : ", book);

                switch (ipver) {
                    case 4:
                        inet_ntop(AF_INET, netw_addr, pres_adr,
                                  sizeof pres_adr);
                        printf("disponible sur %s/%d", pres_adr, ntohs(port));
                        break;
                    case 6:
                        inet_ntop(AF_INET6, netw_addr, pres_adr,
                                  sizeof pres_adr);
                        printf("disponible sur %s/%d", pres_adr, htons(port));
                        break;
                    default:
                        raler("Segment TCP envoyé par le serveur Nil pas conforme");
                }
            }
            // Afficher les livres non disponibles
            printf("\n");
            for (int i = 0; i < books_size; ++i) {
                int j = 0;
                while (j < nb && i != available_indices[j]) j++;

                if (j == nb)
                    printf("\n%s : pas disponible", books[i]);
            }
        } else
            raler("Format du segment TCP pas conforme");
    } else if (size_read == 2)
        printf("\nAucun de ces livres n'est disponible!");


    printf("\n\n");
}
