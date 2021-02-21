#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CLIENTS_NB 65535           /* Nombre de réservations pouvant etre traitées simulatanément */
#define    MAX_TCP_SEG_SIZE 65535         /* Taille max du paquet TCP autorisée  */
#define    MAX_UDP_DGM_SIZE 65535         /* Taille max du paquet UDP autorisée  */

void usage(char *argv0) {
    fprintf(stderr, "usage: %s port [livre_1 livre_2 ...]\n\n", argv0);
    exit(1);
}

void raler(char *msg) {
    perror(msg);
    exit(1);
}

// Pour chercher un livre dans le stock disponible
int search_library(char *book, char *stock[], int stock_size);

// Pour chercher un livre dans le stock disponible (et le suprimer si présent)
int search_library_and_delete(char *book, char *stock[], int *stock_size);

// Contruction d'un segment TCP
int make_reply_segment(uint8_t *const replies[], int replies_size,
                       uint8_t *segment);

// Communication avec le serveur Nil en UDP
void talk_to_nil(int sock, char *stock[], int stock_size);

// Communication avec le client en TCP
void talk_to_client(int in, char *stock[], int *stock_size);


int main(int argc, char *argv[]) {

    printf("\n------ LIBRAIRIE ------\n");

    int my_tcp_sock, dedi_sock, r, my_udp_sock;
    struct sockaddr_in6 monadr, sonadr;
    socklen_t salong = sizeof sonadr;
    int port, val;
    char padr[INET6_ADDRSTRLEN];

    // Test des arguments
    if (argc >= 2) {
        port = atoi(argv[1]);
    } else
        usage(argv[0]);

    // Création du stock de livres disponibles dans cette libraririe
    int stock_size = argc - 2;
    char *stock[stock_size];
    for (int i = 0; i < stock_size; ++i)
        stock[i] = argv[i + 2];

    /* Création du socket UDP IPv6 (pouvant travailler en Ipv4) pour communiquer avec Nil */
    my_udp_sock = socket(PF_INET6, SOCK_DGRAM, 0);
    if (my_udp_sock == -1) raler("socket");

    val = 0;
    r = setsockopt(my_udp_sock, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof val);
    if (r == -1) raler("setsockopt");
    int opt = 1;
    r = setsockopt(my_udp_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
    if (r == -1) raler("setsockopt");

    // Bind du socket a cette drresse
    memset(&monadr, 0, sizeof monadr);
    monadr.sin6_family = AF_INET6;
    monadr.sin6_port = htons(port);
    monadr.sin6_addr = in6addr_any;
    r = bind(my_udp_sock, (struct sockaddr *) &monadr, sizeof monadr);
    if (r == -1) raler("bind");

    /* Création du socket TCP IPv6 (pouvant travailler en Ipv4) pour communiquer avec un client */
    my_tcp_sock = socket(PF_INET6, SOCK_STREAM, 0);
    if (my_tcp_sock == -1) raler("socket");

    val = 0;
    r = setsockopt(my_tcp_sock, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof val);
    if (r == -1) raler("setsockopt");
    opt = 1;
    r = setsockopt(my_tcp_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
    if (r == -1) raler("setsockopt");

    // Bind du socket a cette adrresse
    memset(&monadr, 0, sizeof monadr);
    monadr.sin6_family = AF_INET6;
    monadr.sin6_port = htons(port);
    monadr.sin6_addr = in6addr_any;
    r = bind(my_tcp_sock, (struct sockaddr *) &monadr, sizeof monadr);
    if (r == -1) raler("bind");

    r = listen(my_tcp_sock, MAX_CLIENTS_NB);
    if (r == -1) raler("listen");

    for (;;) {

        // Ajout de descriteurs d'écoute (pas deboisn d'écouter sur celui du client dédié)
        fd_set readfds;
        FD_ZERO (&readfds);
        FD_SET (my_udp_sock, &readfds);
        FD_SET (my_tcp_sock, &readfds);
        int max = my_tcp_sock > my_udp_sock ? my_tcp_sock : my_udp_sock;

        // Ecoute d'une écriture
        if (select(max + 1, &readfds, NULL, NULL, NULL) == -1)
            raler("select");

        // Communication avec le serveur UDP
        if (FD_ISSET (my_udp_sock, &readfds))
            talk_to_nil(my_udp_sock, stock, stock_size);

        // Accepation de la connection TCP et communication avec le client
        if (FD_ISSET (my_tcp_sock, &readfds)) {
            dedi_sock = accept(my_tcp_sock, (struct sockaddr *) &sonadr,
                               &salong);
            if (dedi_sock == -1) raler("accept");

            if (inet_ntop(AF_INET6, &sonadr.sin6_addr, padr, sizeof padr) ==
                NULL)
                raler("inet_ntop");
            printf("\nCommande recue de %s/%d pour: ", padr, ntohs(sonadr.sin6_port));

            talk_to_client(dedi_sock, stock, &stock_size);
            close(dedi_sock);
        }
    }

    close(my_tcp_sock);
    close(my_udp_sock);

    printf("\n\n");

    exit(0);
}

int search_library(char *book, char *stock[], int stock_size) {
    int index = 0;
    while (index < stock_size && strcmp(book, stock[index]) != 0)
        index++;

    if (index == stock_size) // Livre non trouvé
        return 0;
    else                     // Livre trouvé
        return 1;
}

int search_library_and_delete(char *book, char *stock[], int *stock_size) {
    int index = 0, current_stock_size = *stock_size;
    while (index < current_stock_size && strcmp(book, stock[index]) != 0)
        index++;

    if (index == current_stock_size)
        return 0;
    else {
        // Décalage des livres de la liste
        for (int j = index; j < current_stock_size - 1; ++j)
            stock[j] = stock[j + 1];

        stock[current_stock_size - 1] = NULL;
        *stock_size = current_stock_size - 1;

        return 1;
    }
}

int make_reply_segment(uint8_t *const replies[], int replies_size,
                       uint8_t *segment) {
    uint16_t nb = replies_size;
    memcpy(segment, (uint8_t *) &nb, sizeof(uint16_t));

    int segment_size = sizeof(uint16_t);

    for (int i = 0; i < replies_size; ++i) {
        memcpy(segment + segment_size, replies[i], 10 + 1);
        segment_size += 10 + 1;
    }

    return segment_size;
}

void talk_to_nil(int sock, char *stock[], int stock_size) {
    struct sockaddr_storage sonadr;
    socklen_t salong;
    uint16_t port;
    int request_size, reply_size, af;
    void *nadr;                    /* addresse au format network */
    char padr[INET6_ADDRSTRLEN];    /* addresse au format presentation */
    uint8_t request_buf[MAX_UDP_DGM_SIZE], reply_buf[MAX_UDP_DGM_SIZE];

    /* Lecture du datagramme */
    salong = sizeof sonadr;
    request_size = recvfrom(sock, request_buf, MAX_UDP_DGM_SIZE, 0,
                            (struct sockaddr *) &sonadr, &salong);
    if (request_size < 6) raler("Lecture message Nil");

    af = ((struct sockaddr *) &sonadr)->sa_family;
    switch (af) {
        case AF_INET :
            nadr = &((struct sockaddr_in *) &sonadr)->sin_addr;
            break;
        case AF_INET6 :
            nadr = &((struct sockaddr_in6 *) &sonadr)->sin6_addr;
            break;
    }
    inet_ntop(af, nadr, padr, sizeof padr);

    uint32_t req_id = *(uint32_t *) request_buf;
    memcpy(reply_buf, request_buf, 4);

    uint16_t req_nb, rep_nb = 0;
    memcpy(&req_nb, request_buf + 4, 2);

    port = ntohs(((struct sockaddr_in *) &sonadr)->sin_port);
    printf("\nRequète [%d] recu de %s/%d \n", req_id, padr, port);
    printf("-- Recieved %d bytes \n", request_size);

    /* Envoie du datagramme retour */
    printf("Envoi réponse [%d] : ", req_id);

    uint8_t book[11];
    int status;
    book[10] = '\0';

    for (int i = 0; i < req_nb; ++i) {
        memcpy(book, request_buf + 4 + 2 + i * 10, 10);
        status = search_library((char *) book, stock, stock_size);
        if (status == 0)
            continue;
        else { // status == 1
            printf("%s ", (char *) book);
            memcpy(reply_buf + 4 + 2 + rep_nb * 10, book, 10);

            rep_nb += 1;
        }
    }

    memcpy(reply_buf + 4, &rep_nb, 2);

    reply_size = sendto(sock, reply_buf, rep_nb * 10 + 6, 0,
                        (struct sockaddr *) &sonadr, salong);
    if (reply_size < rep_nb * 10 + 6) raler("Ecriture message Nil");

    printf("\n-- Sent %d bytes \n", reply_size);
}

void talk_to_client(int in, char *stock[], int *stock_size) {
    int request_seg_size, reply_seg_size;
    uint8_t request_buf[MAX_TCP_SEG_SIZE], reply_buf[MAX_TCP_SEG_SIZE];
    char book[10 + 1];
    int r, status;
    uint8_t *p;

    // Lecture du segment TCP
    request_seg_size = read(in, request_buf, MAX_TCP_SEG_SIZE);
    if (request_seg_size <= 2) raler("Lecture message client");

    uint16_t nb = *(uint16_t *) (request_buf);
    int size_of_nb = sizeof(uint16_t);

    uint8_t *replies[nb];

    for (int i = 0; i < nb; ++i) {
        memcpy(book, request_buf + size_of_nb + i * 10, 10);
        book[10] = '\0';
        printf("%s ", book);

        // On pourrait mettre un blocaqge à ce niveau !!
        status = search_library_and_delete(book, stock, stock_size);

        p = (uint8_t *) malloc(10 + 2);
        if (p == NULL) raler("Malloc");
        replies[i] = p;

        memcpy(replies[i], book, 10);
        memcpy(&replies[i][10], (uint8_t *) &status, 1);
    }

    reply_seg_size = make_reply_segment(replies, nb, reply_buf);
    if (reply_seg_size < 2) raler("Ecriture message client");

    // Envoi de la reponse au client
    r = write(in, reply_buf, reply_seg_size);
    if (r < 0) raler("Envoi message au client");
    for (int i = 0; i < nb; ++i)
        free(replies[i]);

    printf("\n-- Recieved %d bytes", request_seg_size);
    printf("\n-- Sent %d bytes", reply_seg_size);

    // Affichage des elements restants
    if (*stock_size <= 0)
        printf("\nLibrairie Vide !");
    else {
        printf("\nStock restant: ");
        for (int i = 0; i < *stock_size; ++i)
            printf("%s ", stock[i]);
    }
    printf("\n");
    fflush(stdout);
}
