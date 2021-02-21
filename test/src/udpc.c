#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define	MAXLEN	1024

void usage (char *argv0)
{
    fprintf (stderr, "usage: %s addr [port]\n", argv0) ;
    exit (1) ;
}

void raler (char *msg)
{
    perror (msg) ;
    exit (1) ;
}

int main (int argc, char *argv [])
{
    struct sockaddr_storage sadr ;	// taille >= a ka taille des addresses qui sont manipulées
    struct sockaddr_in  *sadr4 = (struct sockaddr_in  *) &sadr ;		// les deux pointent au me endroi
    struct sockaddr_in6 *sadr6 = (struct sockaddr_in6 *) &sadr ;
    socklen_t salong ;
    char *padr = NULL ;
    int s, r, family, port = 0, o ;
    char buf [MAXLEN] ;

    memset (&sadr, 0, sizeof sadr) ;

    switch (argc)
    {
	case 2 :
	    padr = argv [1] ;
	    port = 9000 ;
	    break ;
	case 3 :
	    padr = argv [1] ;
	    port = atoi (argv [2]) ;
	    break ;
	default :
	    usage (argv [0]) ;
    }

    // Afficher ce num en hexa sinon on ne remarque pas la conversion)
    port = htons (port) ;		// Covertit en network byte order sachant que les num de port son en host byte order (peut etre big)

    if (inet_pton (AF_INET6, padr, & sadr6->sin6_addr) == 1)		// inet_pton transforme un echaine de charatere en addresse IPv6 
    {
	family = PF_INET6 ;
	sadr6->sin6_family = AF_INET6 ;
	sadr6->sin6_port = port ;
	salong = sizeof *sadr6 ;
    }
    else if (inet_pton (AF_INET, padr, & sadr4->sin_addr) == 1)		// Si ca marche pas, convertir ca enIPv4
    {
	family = PF_INET ;
	sadr4->sin_family = AF_INET ;
	sadr4->sin_port = port ;
	salong = sizeof *sadr4 ;
    }
    else
    {
	fprintf (stderr, "%s: adresse '%s' non reconnue\n", argv [0], padr) ;
	exit (1) ;
    }

    //// A ce niveau on une addresse IP

    s = socket (family, SOCK_DGRAM, 0) ;		// On fonctionne en mode DATAGRAMME, 0 veut dire débrouille toi pour trouver le protocle utilié
    if (s == -1) raler ("socket") ;

    o = 1 ;
    setsockopt (s, SOL_SOCKET, SO_BROADCAST, &o, sizeof o) ;		// Option de socket, je m'autorise a utiliser le broadcast, si o!=1, le noyau va refuser d'envoyer des donnnes a l'ddresse de broacast

    while ((r = read (0, buf, MAXLEN)) > 0)		// On lit des octets sur l'entrée standar // Dans r, on a ne nmb d'octets transmis
    {
	r = sendto (s, buf, r, 0, (struct sockaddr *) &sadr, salong) ;		// Ne pas oublier de convertir en socaddr *
	if (r == -1) raler ("sendto") ;
    }
    if (r == -1) raler ("read") ;

    close (s) ;

    exit (0) ;
}
