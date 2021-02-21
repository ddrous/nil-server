#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define	MAXLEN	1024
#define	SERVICE	"9000"		/* ou un nom dans /etc/services */
#define	MAXSOCK	32

void raler (char *msg)
{
    perror (msg) ;
    exit (1) ;
}

void usage (char *argv0)
{
    fprintf (stderr, "usage: %s [port]\n", argv0) ;
    exit (1) ;
}

void lire_message (int s)
{
    struct sockaddr_storage sonadr ;
    socklen_t salong ;
    int r, af ;
    void *nadr ;			/* au format network */
    char padr [INET6_ADDRSTRLEN] ;	/* au format presentation */
    char buf [MAXLEN] ;

    salong = sizeof sonadr ;
    r = recvfrom (s, buf, MAXLEN, 0, (struct sockaddr *) &sonadr, &salong) ;	// On ne peut pas modfier le comportemetn de recvfrom, salong est de taille maximale
    af = ((struct sockaddr *) &sonadr)->sa_family ;
    switch (af)		// EN fonctiond de la famille dáddrese, on recupere l'addrese IPv4
    {
	case AF_INET :		// 2 ==AF_INET
	    nadr = & ((struct sockaddr_in *) &sonadr)->sin_addr ;
	    break ;
	case AF_INET6 :
	    nadr = & ((struct sockaddr_in6 *) &sonadr)->sin6_addr ;
	    break ;
    }
    inet_ntop (af, nadr, padr, sizeof padr) ;		// convertit l'addrr en chaine de char affichable
    printf ("%s: nb d'octets lus = %d\n", padr, r) ;
}

int main (int argc, char *argv [])
{
    int s [MAXSOCK], nsock, r ;					// MAXSOCK ==>on s'attend a ouvrir plusierus sockets pour IPv4 et v6
    struct addrinfo hints, *res, *res0 ;
    char *cause ;
    char *serv = NULL ;

    switch (argc)
    {
	case 1 : serv = SERVICE ; break ;
	case 2 : serv = argv [1] ; break ;
	default : usage (argv [0]) ; break ;
    }

    memset (&hints, 0, sizeof hints) ;
    hints.ai_family = PF_UNSPEC ;
    hints.ai_socktype = SOCK_DGRAM ;	
    hints.ai_flags = AI_PASSIVE ;		// OUverture faicle, pas de serveur a mettre
    r = getaddrinfo (NULL, serv,  &hints, &res0) ;		// retourne des sockets
    if (r != 0)
    {
	fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (r)) ;		// Primitive susteme gai_strerror
	exit (1) ;
    }

    nsock = 0 ;
    for (res = res0; res && nsock < MAXSOCK; res = res->ai_next)	// Parcours de nos sockets
    {
		s [nsock] = socket (res->ai_family, res->ai_socktype, res->ai_protocol) ;		// utilse, ai_family 10 = ipv6
		if (s [nsock] == -1)
		    cause = "socket" ;
		else
		{
		    int o = 1 ;		/* pour Linux */
		    setsockopt (s [nsock], IPPROTO_IPV6, IPV6_V6ONLY, &o, sizeof o) ;		// Option particulière de LINUX qui ne gene pas pour les autres systemes

		    r = bind (s [nsock], res->ai_addr, res->ai_addrlen) ;		// Je m'attache a tel port, on bind au port reoutné par getaddrinfo
		    if (r == -1)
		    {
			cause = "bind" ;
			close (s [nsock]) ;
		    }
		    else nsock++ ;
		}
    }
    if (nsock == 0) raler (cause) ;			// Si on n'a pas touvé on rale
    freeaddrinfo (res0) ;		// On libere les champs de noms qui ont été transofmé en addresses 

    for (;;)			// boucle infinie pour attendre des connexions
    {
		fd_set readfds ;
		int i, max = 0 ;

		FD_ZERO (&readfds) ;			// On s'enfout des descripteurs 0,1 ou 2 aussi !!!
		for (i = 0 ; i < nsock ; i++)
		{
		    FD_SET (s [i], &readfds) ;		/// Cette boucle permet de mettre les bits à 1
		    if (s [i] > max)
			max = s [i] ;
		}
		// Select permet d'ecouter sur toutes les addresses possibles pour un clients (autant de sockets que necesaire sont ete crees)
		// 1er NULL: on n'ecoute pas les ecritures; 2eme NULL: on n'ecoute pas les evenement exeptionnels; 3eme NULL; on ne borne pas le temps 
		if (select (max+1, &readfds, NULL, NULL, NULL) == -1)		// Qui n'est pas specifique pour socket. Pour ne pas manquer une connexsion sur le 2eme socket alors qu'on listen sur le 1er. select prend une liste de descripteur
		    raler ("select") ;			// max+1 donne les plus grand nombre de bits qu'on veut surveiller et donc parcourir

		for (i = 0 ; i < nsock ; i++)
		    if (FD_ISSET (s [i], &readfds))			// SI le bit readfds est à 1, alors un client est arrivé
			lire_message (s [i]) ;
    }
}
