#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <syslog.h>

#define	MAXLEN	1024
#define	SERVICE	"9000"		/* ou un nom dans /etc/services */
#define	MAXSOCK	32

void raler_log (char *msg)
{
    syslog (LOG_ERR, "%s: %m", msg) ;
    exit (1) ;
}

void usage (char *argv0)
{
    fprintf (stderr, "usage: %s [port]\n", argv0) ;
    exit (1) ;
}

void serveur (int in)
{
    int r, n = 0 ;
    char buf [MAXLEN] ;

    while ((r = read (in, buf, MAXLEN)) > 0)
	n += r ;
    syslog (LOG_ERR, "nb d'octets lus = %d\n", n) ;
}

void demon (char *serv)
{
    int s [MAXSOCK], sd, nsock, r, opt = 1 ;
    struct addrinfo hints, *res, *res0 ;
    char *cause ;

    memset (&hints, 0, sizeof hints) ;
    hints.ai_family = PF_UNSPEC ;
    hints.ai_socktype = SOCK_STREAM ;		// TCP
    hints.ai_flags = AI_PASSIVE ;
    if ((r = getaddrinfo (NULL, serv,  &hints, &res0)) != 0) {
	fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (r)) ;
	exit (1) ;
    }

    nsock = 0 ;
    for (res = res0; res && nsock < MAXSOCK; res = res->ai_next) {
	s [nsock] = socket (res->ai_family, res->ai_socktype, res->ai_protocol) ;
	if (s [nsock] == -1)
	    cause = "socket" ;
	else {
	    setsockopt (s [nsock], IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof opt) ;
	    setsockopt (s [nsock], SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt) ;	// SO_REUSEADDR ==> on s'enfout du timeout apres la fermeture de connection
	    r = bind (s [nsock], res->ai_addr, res->ai_addrlen) ;
	    if (r == -1) {
		cause = "bind" ;
		close (s [nsock]) ;
	    } else {
		listen (s [nsock], 5) ;
		nsock++ ;
	    }
	}
    }
    if (nsock == 0) raler_log (cause) ;
    freeaddrinfo (res0) ;

    for (;;) {
	fd_set readfds ;
	int i, max = 0 ;

	FD_ZERO (&readfds) ;
	for (i = 0 ; i < nsock ; i++) {
	    FD_SET (s [i], &readfds) ;
	    if (s [i] > max)
		max = s [i] ;
	}
	if (select (max+1, &readfds, NULL, NULL, NULL) == -1)
	    raler_log ("select") ;

	for (i = 0 ; i < nsock ; i++) {
	    struct sockaddr_storage sonadr ;
	    socklen_t salong ;

	    if (FD_ISSET (s[i], &readfds))
	    {
		salong = sizeof sonadr ;
		sd = accept (s [i], (struct sockaddr *) &sonadr, &salong) ;
		if (fork () == 0) {
		    serveur (sd) ;
		    exit (0) ;
		}
		close (sd) ;
	    }
	}
    }
}

int main (int argc, char *argv [])
{
    char *serv ;

    switch (argc) {
	case 1 : serv = SERVICE ; break ;
	case 2 : serv = argv [1] ; break ;
	default : usage (argv [0]) ; break ;
    }

    switch (fork ()) {
	case -1 :
	    perror ("cannot fork") ;
	    exit (1) ;
	case 0 :		/* le demon proprement dit */			// NE PAS FAIRE LE DEMON POUR LE PROJET !!
	    setsid () ; chdir ("/") ; umask (0) ;
	    close (0) ; close (1) ; close (2) ;
	    openlog ("exemple", LOG_PID | LOG_CONS, LOG_DAEMON) ;
	    demon (serv) ;	
	    exit (1) ;
	default :
	    exit (0) ;
    }
}
