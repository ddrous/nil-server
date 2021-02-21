#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define INIT_MAX_CLIENTS_NB 32          /* Nombre maximal de clients pour Nil au lancement */
#define MAX_CLIENTS_NB 4294967295       /* Nombre maximal possible de clients simultané pour Nil */
#define    MAX_TCP_SEG_SIZE 65535       /* Taille du segment TCP par défaut  */
#define    MAX_UDP_DGM_SIZE 65535       /* Taille du datagramme TCP par défaut  */

void usage(char *argv0) {
    fprintf(stderr,
            "usage: %s port delai [librarie_1 port_1 librarie_2 port_2 ...]\n\n",
            argv0);
    exit(1);
}

void raler(char *msg) {
    perror(msg);
    exit(1);
}

// Lecture de la demande du client
int
read_request_from_client(int client_sock, uint32_t client_id, uint8_t **buf);

// Envoi de la demande aux libraries
void
send_request_to_libraries(int *sock, struct addrinfo *addr_info[], int nb_libs,
                          uint8_t **buf, int buf_size);

// Lecture de la réponse de la librarie (retourne le client concerné par la réponse)
uint32_t read_reply_from_library(int sock, uint8_t **buf,
                                 struct sockaddr_storage *recv_addr);

// Mise a jour de la réponse à renvoyer à un client
void update_client_reply(uint8_t **reply, int *reply_size,
                         struct sockaddr_storage *recv_addr, uint8_t **buf);

// Envoi de la réponse (complète ou non) au client
void send_reply_to_client(int sock, uint8_t **reply, int reply_size);

// Reallocation de memoire en cas de plusieurs clients simultanés
void increase_allocated_memory(uint32_t new_size, uint8_t ***replies,
                               int **reply_sizes, int **rel_count,
                               int **sockets, int **active,
                               struct sockaddr_in6 **addr,
                               socklen_t **addr_len);

int main(int argc, char *argv[]) {

    printf("\n------ SERVEUR NIL ------\n");

    // Test préliminaire des premiers arguments
    if (argc < 3 || argc % 2 == 0)
        usage(argv[0]);

    /************************************************************************
    // Creation du socket TCP pour les connexions avec les clients (Ouvertures passives)
    // Les quantité ici sont allouées dynamiquement (et reallouées plus tard si besoin)
    *************************************************************************/

    // Tableau de sockets et des clients actifs (i.e pour lesquels une requete a encore été recue mais aucune réponse n'a été renvoyée)
    int *clients_sock, *active_clients;
    clients_sock = malloc(INIT_MAX_CLIENTS_NB * sizeof(int));
    if (clients_sock == NULL) raler("Malloc int");

    active_clients = malloc(INIT_MAX_CLIENTS_NB * sizeof(int));
    if (active_clients == NULL) raler("Malloc int");
    for (int i = 0; i < INIT_MAX_CLIENTS_NB; ++i)
        active_clients[i] = 0;

    // Addresses IP des clients
    struct sockaddr_in6 my_addr, *clients_addr;
    clients_addr = malloc(INIT_MAX_CLIENTS_NB * sizeof(struct sockaddr_in6));
    if (clients_addr == NULL) raler("Malloc sockaddr_in6");

    socklen_t *clients_addr_len;
    clients_addr_len = malloc(INIT_MAX_CLIENTS_NB * sizeof(socklen_t));
    if (clients_addr_len == NULL) raler("Malloc socket address length");

    int max_sock_id, request_seg_size;

    // Tableau comptant le nombre de fois qu'un client est concerné par une réponse de librarie: -> rel_count
    // Tableau enrégistrant la taille actuelle d'une réponse au client pour chaque client: -> reply_sizes
    int *reply_sizes, *rel_count;
    reply_sizes = (int *) malloc(INIT_MAX_CLIENTS_NB * sizeof(int));
    if (reply_sizes == NULL) raler("Malloc int");

    rel_count = (int *) malloc(INIT_MAX_CLIENTS_NB * sizeof(int));
    if (rel_count == NULL) raler("Malloc int");
    for (int i = 0; i < INIT_MAX_CLIENTS_NB; ++i) {
        reply_sizes[i] = 2;
        rel_count[i] = 0;
    }

    // Un tenseur pour contenir les réponses pour chaque client actif
    uint8_t **replies = malloc(INIT_MAX_CLIENTS_NB * sizeof(uint8_t *));
    if (replies == NULL) raler("Malloc");
    for (int i = 0; i < INIT_MAX_CLIENTS_NB; ++i) {
        replies[i] = malloc(MAX_TCP_SEG_SIZE * sizeof(uint8_t));
        if (replies[i] == NULL) raler("Malloc reply struct");
    }

    // Des buffer pour contenir les requetes, et les réponses temporaires
    uint8_t *request_buf, *reply_buf;
    request_buf = (uint8_t *) malloc(MAX_TCP_SEG_SIZE);
    if (request_buf == NULL) raler("Malloc");
    reply_buf = (uint8_t *) malloc(MAX_UDP_DGM_SIZE);
    if (reply_buf == NULL) raler("Malloc");

    int my_port, my_tcp_sock, r;
    char pres_addr[INET6_ADDRSTRLEN];
    uint16_t recv_port;

    // Identifiant des clients connectés, et du client concerné par une réponse
    uint32_t next_id = 0, max_id = 0, rc_id;
    uint32_t curr_max_clients_nb = INIT_MAX_CLIENTS_NB;

    // Temps d'attente maximal
    time_t timeout_sec = atoi(argv[2]);

    // Port TCP pour la communication avec les clients
    my_port = atoi(argv[1]);

    // Création du socket TCP IPv6 (supportant IPv4)
    my_tcp_sock = socket(PF_INET6, SOCK_STREAM, 0);
    if (my_tcp_sock == -1) raler("socket");

    int deactivate_ipv4 = 0;
    r = setsockopt(my_tcp_sock, IPPROTO_IPV6, IPV6_V6ONLY, &deactivate_ipv4,
                   sizeof deactivate_ipv4);
    if (r == -1) raler("setsockopt");

    int optimize_address_reuse = 1;
    r = setsockopt(my_tcp_sock, SOL_SOCKET, SO_REUSEADDR,
                   &optimize_address_reuse, sizeof optimize_address_reuse);
    if (r == -1) raler("setsockopt");

    // Bind du socket à l'addresse de Nil
    memset(&my_addr, 0, sizeof my_addr);
    my_addr.sin6_family = AF_INET6;
    my_addr.sin6_port = htons(my_port);
    my_addr.sin6_addr = in6addr_any;
    r = bind(my_tcp_sock, (struct sockaddr *) &my_addr, sizeof my_addr);
    if (r == -1) raler("TCP bind");

    // Ouverture passive de connexion
    r = listen(my_tcp_sock, INIT_MAX_CLIENTS_NB);
    if (r == -1) raler("Listen error");


    /*************************************************************************
    // Creation de sockets UDP pour les connexions avec les libraries (Ouvertures actives)
    // Les quatitées sont allouées de facon statique, vu que le nombre de libraries est donnu
    *************************************************************************/

    // Nombre de librarie a creer
    int nb_libs = (argc - 3) / 2;

    char *libs_addr[nb_libs], *libs_port[nb_libs];
    char *err_cause;
    int libs_sock[nb_libs];
    struct timeval timeout;
    struct addrinfo hints, *res, *res0[nb_libs], *libs_addr_info[nb_libs];
    struct sockaddr_storage recv_addr;

    // Création de la liste des addresses de bibliotheques
    for (int i = 0; i < nb_libs; ++i) {
        libs_addr[i] = argv[i * 2 + 3];
        libs_port[i] = argv[i * 2 + 4];
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = PF_UNSPEC;       // IPv6 ou IPv4
    hints.ai_socktype = SOCK_DGRAM;    // UDP

    // Récupération des addresses IP de librairies
    for (int i = 0; i < nb_libs; ++i) {
        r = getaddrinfo(libs_addr[i], libs_port[i], &hints, &res0[i]);
        if (r != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(r));
            exit(1);
        }
    }

    // Création de sockets pour les libraries (#### On Peut Faire Mieux ! ####)
    for (int i = 0; i < nb_libs; ++i) {
        libs_sock[i] = -1;
        for (res = res0[i]; res != NULL; res = res->ai_next) {
            libs_sock[i] = socket(res->ai_family, res->ai_socktype,
                                  res->ai_protocol);
            if (libs_sock[i] == -1)
                err_cause = "cannot create socket";
            else {
                if (res->ai_family == AF_INET6) {
                    deactivate_ipv4 = 0;
                    r = setsockopt(libs_sock[i], IPPROTO_IPV6, IPV6_V6ONLY,
                                   &deactivate_ipv4, sizeof deactivate_ipv4);
                    if (r == -1) raler("UDP setsockopt use IPv4");
                } else
                    my_addr.sin6_family = AF_INET;

                optimize_address_reuse = 1;
                r = setsockopt(libs_sock[i], SOL_SOCKET, SO_REUSEADDR,
                               &optimize_address_reuse,
                               sizeof optimize_address_reuse);
                if (r == -1) raler("UDP setsockopt reuse addr");

                /*********** LE BIND EST IMPORTANT, MAIS NE FONCTIONNNE QU'EN IPv4 ************/
                // r = bind(libs_sock[i], (struct sockaddr *) &my_addr, sizeof my_addr) ;
                // if (r == -1) raler("UDP bind") ;

                libs_addr_info[i] = res;
                break;
            }
        }
        if (libs_sock[i] == -1) raler(err_cause);
    }


    /************************************************************************
    // Boucle infinie pour attendre simulatanément les connexions des clients et les reponses des libraries
    // - Si le select est intérrompu par un fils entrant, envoyer sa demande et le classer comme actif
    // - Si le select est intérrompu par une réponse de librarie, la rajouter au fils actif concerné
    // - Si le select s'acheve après un timeout, alors le serveur répond a tous les fils encore actifs
    ************************************************************************/


    for (;;) {

        // Rajouter le socket TCP et les sockets UDP à l'écoute
        fd_set readfds;
        FD_ZERO (&readfds);
        FD_SET (my_tcp_sock, &readfds);
        max_sock_id = my_tcp_sock;
        for (int i = 0; i < nb_libs; i++) {
            FD_SET (libs_sock[i], &readfds);
            if (libs_sock[i] > max_sock_id)
                max_sock_id = libs_sock[i];
        }

        // Définition du timeout (et redefinition pour tout appel à select)
        timeout.tv_sec = timeout_sec;
        timeout.tv_usec = 0;
        r = select(max_sock_id + 1, &readfds, NULL, NULL, &timeout);

        if (r == -1) {                         // Erreur -> Raler
            raler("Select");
        } else if (r == 0) {                   // Timeout -> Répondre à tous les clients actifs
            for (uint32_t j = 0; j < max_id + 1 ; ++j) {
                switch (active_clients[j]) {
                    case 1:     // Pour l'implémentation de la tolérance aux pannes
                        // Recupérer la demande de chaque client actif
                        // Renvoyer la demande à la librarie non répondante
                        // active_clients[j] += 1;
                        break;
                    case 2:
                        printf("\nEnvoie réponse au client [%d]\n", j);

                        send_reply_to_client(clients_sock[j], &(replies[j]),
                                             reply_sizes[j]);
                        reply_sizes[j] = 2;
                        active_clients[j] = 0;
                        rel_count[j] = 0;

                        if (j < next_id) next_id = j;     // Reutilisation de l'id j
                        break;
                }
            }
        } else {
            if (FD_ISSET (my_tcp_sock, &readfds)) {
                // Ativation de cet id
                active_clients[next_id] = 2;
                if (next_id > max_id) max_id = next_id;

                // Création d'un socket dédié au client
                clients_addr_len[next_id] = sizeof clients_addr[next_id];
                clients_sock[next_id] = accept(my_tcp_sock,
                                          (struct sockaddr *) &clients_addr[next_id],
                                          &clients_addr_len[next_id]);
                if (clients_sock[next_id] == -1) raler("accept");

                if (inet_ntop(AF_INET6, &clients_addr[next_id].sin6_addr, pres_addr,
                              sizeof pres_addr) == NULL)
                    raler("inet_ntop");

                recv_port = ntohs(clients_addr[next_id].sin6_port);
                printf("\nRequète recue de %s/%d pour: ", pres_addr, recv_port);

                // Lecture de la requete du nouveau client
                request_seg_size = read_request_from_client(clients_sock[next_id],
                                                            next_id,
                                                            &request_buf);

                // Envoie à toutes les libraries
                send_request_to_libraries(libs_sock, libs_addr_info, nb_libs,
                                          &request_buf, request_seg_size);

                // Choix par defaut du prochain id
                next_id = max_id + 1;

                if (next_id != 0 && next_id % INIT_MAX_CLIENTS_NB == 0) {
                    curr_max_clients_nb = (1 + (next_id / INIT_MAX_CLIENTS_NB)) *
                                        INIT_MAX_CLIENTS_NB;
                    increase_allocated_memory(curr_max_clients_nb, &replies, &reply_sizes,
                                              &rel_count, &clients_sock,
                                              &active_clients,
                                              &clients_addr, &clients_addr_len);
                }
                if (next_id == MAX_CLIENTS_NB)
                    raler("Nombre maximal de connexions atteint");

            } else {
                for (int i = 0; i < nb_libs; i++) {
                    if (FD_ISSET (libs_sock[i], &readfds)) {
                        // Initialisation de la connection du ce port (intéressant si on veut utiliser un seul socket)
                        r = connect(libs_sock[i], libs_addr_info[i]->ai_addr,
                                    libs_addr_info[i]->ai_addrlen);
                        if (r == -1) raler("Cannot connect to known library");

                        // Lecture du datagramme et identification du client concerné
                        rc_id = read_reply_from_library(libs_sock[i],
                                                             &reply_buf,
                                                             &recv_addr);

                        // Incrémentation du nombre de réponses pour le client concerné
                        rel_count[rc_id] += 1;

                        // Mise a jour de la réponse pour le client converné
                        update_client_reply(&(replies[rc_id]),
                                            &(reply_sizes[rc_id]),
                                            &recv_addr, &reply_buf);

                        // Vérification si le client concerné a suffisament de réponses
                        if (rel_count[rc_id] == nb_libs) {
                            printf("Envoi réponse au client [%d]\n",
                                   rc_id);

                            send_reply_to_client(clients_sock[rc_id],
                                                 &(replies[rc_id]),
                                                 reply_sizes[rc_id]);
                            reply_sizes[rc_id] = 2;
                            active_clients[rc_id] = 0;
                            rel_count[rc_id] = 0;

                            if (rc_id < next_id) next_id = rc_id;
                        }
                    }
                }
            }
        }
    }

    // Fermeture des sockets, et libération des espaces mémoire
    for (int i = 0; i < nb_libs; ++i) {
        close(libs_sock[i]);
        freeaddrinfo(res0[i]);
    }

    free(replies);
    for (uint32_t i = 0; i < curr_max_clients_nb; ++i) {
        free(replies[i]);
    }
    free(reply_sizes);
    free(rel_count);
    free(clients_sock);
    free(active_clients);
    free(clients_addr);
    free(clients_addr_len);

    close(my_tcp_sock);

    printf("\n\n");

    exit(0);
}


int
read_request_from_client(int client_sock, uint32_t client_id, uint8_t **buf) {
    int request_seg_size = 0;

    request_seg_size = read(client_sock, *buf + 4, MAX_TCP_SEG_SIZE - 4);
    if (request_seg_size <= 2) raler("Lecture message client");

    char book[11];
    book[10] = '\0';
    uint16_t nb = *(uint16_t *) (*buf + 4);

    // Affichage des livres
    for (int i = 0; i < nb; ++i) {
        memcpy(book, *buf + 6 + i * 10, 10);
        printf(" %s ", book);
    }

    printf("\n-- Recieved %d bytes \n", request_seg_size);

    // Ajout de l'id du client au début de la requète à tranmettre à la librairie
    uint32_t id = client_id;
    memcpy(*buf, &id, 4);

    return request_seg_size + 4;
}


void
send_request_to_libraries(int *sock, struct addrinfo *addr_info[], int nb_libs,
                          uint8_t **buf, int buf_size) {
    int r, addr_fam, port;
    void *network_addr;
    char pres_addr[INET6_ADDRSTRLEN];

    for (int i = 0; i < nb_libs; ++i) {

        addr_fam = ((struct sockaddr *) addr_info[i]->ai_addr)->sa_family;
        switch (addr_fam) {
            case AF_INET :
                network_addr = &((struct sockaddr_in *) addr_info[i]->ai_addr)->sin_addr;
                port = ntohs(
                        ((struct sockaddr_in *) addr_info[i]->ai_addr)->sin_port);
                break;
            case AF_INET6 :
                network_addr = &((struct sockaddr_in6 *) addr_info[i]->ai_addr)->sin6_addr;
                port = ntohs(
                        ((struct sockaddr_in *) addr_info[i]->ai_addr)->sin_port);
                break;
            default:
                raler("Protocole non supporté");
        }
        inet_ntop(addr_fam, network_addr, pres_addr, sizeof pres_addr);
        printf("Envoi requète à la librairie %s/%d \n", pres_addr, port);

        r = sendto(sock[i], *buf, buf_size, 0, addr_info[i]->ai_addr,
                   addr_info[i]->ai_addrlen);
        if (r < buf_size) raler("Envoi message aux Libraries");
        printf("-- Sent %d bytes \n", r);

    }
}


uint32_t read_reply_from_library(int sock, uint8_t **buf,
                                 struct sockaddr_storage *recv_addr) {
    int addr_fam, reply_seg_size;
    uint32_t rel_client;
    uint16_t port;
    void *network_addr;
    char pres_addr[INET6_ADDRSTRLEN];

    socklen_t recv_addr_len = sizeof *(recv_addr);

    reply_seg_size = recvfrom(sock, *buf, MAX_UDP_DGM_SIZE, 0,
                              (struct sockaddr *) recv_addr, &recv_addr_len);
    if (reply_seg_size < 6) raler("Reception message de librarie");

    addr_fam = ((struct sockaddr *) recv_addr)->sa_family;

    switch (addr_fam) {
        case AF_INET :
            network_addr = &((struct sockaddr_in *) recv_addr)->sin_addr;
            break;
        case AF_INET6 :
            network_addr = &((struct sockaddr_in6 *) recv_addr)->sin6_addr;
            break;
        default:
            raler("Protocole non supporté");
    }

    inet_ntop(addr_fam, network_addr, pres_addr, sizeof pres_addr);
    port = ntohs(((struct sockaddr_in *) recv_addr)->sin_port);

    rel_client = *((uint32_t *) *buf);

    printf("Réponse [%d] recue de %s/%d \n", rel_client, pres_addr, port);
    printf("-- Recieved %d bytes \n", reply_seg_size);

    return rel_client;
}


void update_client_reply(uint8_t **reply, int *reply_size,
                         struct sockaddr_storage *recv_addr, uint8_t **buf) {
    int current_size = *reply_size;

    struct sockaddr_in *sadr4;
    struct sockaddr_in6 *sadr6;

    uint8_t ipver;
    uint16_t port;

    int fam = ((struct sockaddr *) recv_addr)->sa_family;
    uint16_t nb = *(uint16_t *) (*buf + 4);

    for (int i = 0; i < nb; ++i) {
        memcpy((*reply) + current_size, *buf + 6 + 10 * i, 10);

        switch (fam) {
            case AF_INET6:
                ipver = 6;
                sadr6 = (struct sockaddr_in6 *) recv_addr;
                port = sadr6->sin6_port;

                memcpy((*reply) + current_size + 10, &ipver, 1);
                memcpy((*reply) + current_size + 11, &sadr6->sin6_addr, 16);
                memcpy((*reply) + current_size + 27, &port, 2);
                break;
            case AF_INET:
                ipver = 4;
                sadr4 = (struct sockaddr_in *) recv_addr;
                port = sadr4->sin_port;

                memcpy((*reply) + current_size + 10, &ipver, 1);
                memcpy((*reply) + current_size + 11, &sadr4->sin_addr, 4);
                memcpy((*reply) + current_size + 27, &port, 2);
                break;
            default:
                raler("Famille d'addresses non supportée");
        }
        current_size += 29;
    }
    *reply_size = current_size;
}


void send_reply_to_client(int sock, uint8_t **reply, int reply_size) {
    int r;

    uint16_t nb = (reply_size - 2) / 29;
    memcpy(*reply, &nb, sizeof(uint16_t));

    r = write(sock, *reply, reply_size);
    if (r < reply_size) raler("Renvoi message au client");
    printf("-- Sent %d bytes \n", reply_size);
}


void increase_allocated_memory(uint32_t new_size, uint8_t ***replies,
                               int **reply_sizes, int **rel_count,
                               int **sockets, int **active,
                               struct sockaddr_in6 **addr,
                               socklen_t **addr_len) {

    // printf("\n### Realocating memory ####\n");

    *replies = realloc(*replies, new_size * sizeof(uint8_t *));
    if (*replies == NULL) raler("Realloc");
    for (uint64_t i = new_size - INIT_MAX_CLIENTS_NB; i < new_size; ++i) {
        (*replies)[i] = malloc(MAX_TCP_SEG_SIZE * sizeof(uint8_t));
        if ((*replies)[i] == NULL) raler("Malloc reply struct");
    }

    *reply_sizes = (int *) realloc(*reply_sizes, new_size * sizeof(int));
    if (*reply_sizes == NULL) raler("Realloc");
    *rel_count = (int *) realloc(*rel_count,
                                 new_size * sizeof(int));
    if (*rel_count == NULL) raler("Realloc int");
    for (uint64_t i = new_size - INIT_MAX_CLIENTS_NB; i < new_size; ++i) {
        (*reply_sizes)[i] = 2;
        (*rel_count)[i] = 0;
    }

    *sockets = realloc(*sockets, new_size * sizeof(int));
    if (*sockets == NULL) raler("Realloc int");

    *active = realloc(*active, new_size * sizeof(int));
    if (*active == NULL) raler("Realloc int");
    for (uint32_t i = new_size - INIT_MAX_CLIENTS_NB; i < new_size; ++i)
        (*active)[i] = 0;

    *addr = realloc(*addr, new_size * sizeof(struct sockaddr_in6));
    if (*addr == NULL) raler("Realloc sockaddr_in6");

    *addr_len = malloc(new_size * sizeof(socklen_t));
    if (*addr_len == NULL) raler("Realloc socket address length");
}
