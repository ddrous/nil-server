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

void usage (char *argv0)
{
    fprintf (stderr, "usage: %s host [port]\n", argv0) ;
    exit (1) ;
}

void raler (char *msg)
{
    perror (msg) ;
    exit (1) ;
}

int main (int argc, char *argv [])
{
    char *host = NULL, *serv = NULL ;
    char *cause ;
    int s, r ;
    char buf [MAXLEN] ;
    struct addrinfo hints, *res, *res0 ;

    switch (argc)
    {
	case 2 :
	    host = argv [1] ;
	    serv = SERVICE ;
	    break ;
	case 3 :
	    host = argv [1] ;
	    serv = argv [2] ;
	    break ;
	default :
	    usage (argv [0]) ;
    }

    memset (&hints, 0, sizeof hints) ;
    hints.ai_family = PF_UNSPEC ;
    hints.ai_socktype = SOCK_STREAM ;
    r = getaddrinfo (host, serv,  &hints, &res0) ;
    if (r != 0)
    {
	fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (r)) ;
	exit (1) ;
    }

    s = -1 ;
    for (res = res0 ; res != NULL ; res = res->ai_next)
    {
	s = socket (res->ai_family, res->ai_socktype, res->ai_protocol) ;
	if (s == -1)
	    cause = "socket" ;
	else
	{
	    r = connect (s, res->ai_addr, res->ai_addrlen) ;
	    if (r == -1)
	    {
		cause = "connect" ;
		close (s) ;
		s = -1 ;
	    }
	    else break ;       //  Des qu'on réussit a se connecté, on sort de la boucle
	}
    }
    if (s == -1) raler (cause) ;
    freeaddrinfo (res0) ;

    while ((r = read (0, buf, MAXLEN)) > 0)
	write (s, buf, r) ;
    close (s) ;

    exit (0) ;
}
