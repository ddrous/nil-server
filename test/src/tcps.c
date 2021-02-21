#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void usage (char *argv0)
{
    fprintf (stderr, "usage: %s [port]\n", argv0) ;
    exit (1) ;
}

void raler (char *msg)
{
    perror (msg) ;
    exit (1) ;
}

#define	MAXLEN	1024

void serveur (int in)
{
    int r ;
    char buf [MAXLEN] ;
    int n ;

    n = 0 ;
    while ((r = read (in, buf, MAXLEN)) > 0)
	    n += r ;
    printf ("%d\n", n) ;
}

int main (int argc, char *argv [])
{
    int s, sd, r ;
    struct sockaddr_in6 monadr, sonadr ;
    socklen_t salong ;
    int port, val ;
    char padr [INET6_ADDRSTRLEN] ;

    switch (argc)
    {
	case 1 :
	    port = 9000 ;
	    break ;
	case 2 :
	    port = atoi (argv [1]) ;
	    break ;
	default :
	    usage (argv [0]) ;
    }

    s = socket (PF_INET6, SOCK_STREAM, 0) ;     // On cree directemtn une socket IPv6
    if (s == -1) raler ("socket") ;

    val = 0 ;
    r = setsockopt (s, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof val) ;       // IPV6_V6ONLY ==> le socket peut accepter les IPv4 aussi
    if (r == -1) raler ("setsockopt") ;

    memset (&monadr, 0, sizeof monadr) ;
    monadr.sin6_family = AF_INET6 ;
    monadr.sin6_port = htons (port) ;
    monadr.sin6_addr = in6addr_any ;        // ca paeut aussi etre ipv4
    r = bind (s, (struct sockaddr *) &monadr, sizeof monadr) ;
    if (r == -1) raler ("bind") ;

    r = listen (s, 5) ;
    if (r == -1) raler ("listen") ;

    salong = sizeof sonadr ;
    sd = accept (s, (struct sockaddr *) &sonadr, &salong) ;     // la socket sd est dédié au client
    if (sd == -1) raler ("accept") ;

    if (inet_ntop (AF_INET6, &sonadr.sin6_addr, padr, sizeof padr) == NULL)
	raler ("inet_ntop") ;
    printf ("Connexion depuis %s\n", padr) ;

    serveur (sd) ;      // compte ne nbre d'octets
    
    close (sd) ;
    close (s) ;

    exit (0) ;
}
