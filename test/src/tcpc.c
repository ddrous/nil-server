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
    struct sockaddr_storage sadr ;
    struct sockaddr_in *sadr4 = (struct sockaddr_in *) &sadr ;
    struct sockaddr_in6 *sadr6 = (struct sockaddr_in6 *) &sadr ;
    char *padr = NULL ;
    int s, r, family, port ;
    socklen_t len ;
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

    port = htons (port) ;

    if (inet_pton (AF_INET6, padr, & sadr6->sin6_addr) == 1)
    {
	len = sizeof *sadr6 ;
	family = PF_INET6 ;
	sadr6->sin6_family = AF_INET6 ;
	sadr6->sin6_port = port ;
    }
    else if (inet_pton (AF_INET, padr, & sadr4->sin_addr) == 1)
    {
	len = sizeof *sadr4 ;
	family = PF_INET ;
	sadr4->sin_family = AF_INET ;
	sadr4->sin_port = port ;
    }
    else
    {
	fprintf (stderr, "%s: adresse '%s' non reconnue\n", argv [0], padr) ;
	exit (1) ;
    }

    s = socket (family, SOCK_STREAM, 0) ;       // SOCK_STREAM == TCP car TCP traire les flux de donnees
    if (s == -1) raler ("socket") ;

    r = connect (s, (struct sockaddr *) &sadr, len) ;
    if (r == -1) raler ("connect") ;

    while ((r = read (0, buf, MAXLEN)) > 0)             
	write (s, buf, r) ;
    close (s) ;     // On ferme la connexion

    exit (0) ;
}
