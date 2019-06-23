/*
 * rpc.dracd: Dynamic Relay Authorization Control daemon
 */

#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <errno.h>
#include <syslog.h>
#ifdef TI_RPC
#include <netdir.h>
#include <netconfig.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <db.h>
#include <signal.h>
#include <fcntl.h>
#ifdef FLOCK_LOCK
#include <sys/file.h>
#endif
#ifdef SYSINFO
#include <sys/systeminfo.h>
#endif
#ifdef GETHOST
#include <unistd.h>
#include <limits.h>
#endif
#ifndef DB_VERSION_MAJOR
#define DB_VERSION_MAJOR 1
#endif
#include "drac.h"

#define DBFILE "/etc/mail/dracd.db"
#define ALFILE "/etc/mail/dracd.allow"

struct net_def {
    struct net_def *nd_next;
    struct in_addr nd_mask;
    struct in_addr nd_addr;
};

int initdb = 0;			/* initialize database option */
int terminate = 0;		/* terminate daemon flag */
long explimit = 30 * 60;	/* expiry limit (seconds) */
char *dbfile = DBFILE;		/* database file name */
char *alfile = ALFILE;		/* allow file name */
struct net_def *net_tbl;	/* trusted network table */
#if DB_VERSION_MAJOR < 2
#elif DB_VERSION_MAJOR == 2
DB_ENV *dbenv;			/* db environment structure */
#else
#endif
DB *dbp;			/* DB structure */
int dbfd;			/* db file descriptor */
#ifdef DEBUG
FILE *debugf;
#endif

/* On SIGTERM, must close db */
void catcher(n) int n; {
    terminate = 1;
}

/* Parse command-line options */
main(argc, argv) int argc; char **argv; {
    int c;
    extern char *optarg;
    extern int optind;

    while ((c = getopt(argc, argv, "ie:")) != EOF) {
	switch (c) {
	  case 'i':
	    initdb = 1;
	    break;
	  case 'e':
	    explimit = atoi(optarg) * 60;
	    break;
	  case '?':
	    fprintf(stderr, "Usage: %s [-i] [-e expire] [dbfile]\n",
		    argv[0]);
	    exit(2);
	}
    }
    if ( optind < argc ) dbfile = argv[optind];
    dracmain();		/* the main function from rpcgen */
    exit(1);
}

/* Called once after fork */
drac_run() {
    int sel;
    time_t nexte, now;
    fd_set readfs;
    struct timeval to;
#if DB_VERSION_MAJOR < 2
#ifndef REQ_HASH
    BTREEINFO bti;
#endif
#elif DB_VERSION_MAJOR == 2
    DB_ENV adbenv;
    DB_INFO dbinfo;
#else
#endif

#ifdef DEBUG
    debugf = fopen("/var/tmp/drac.debug", "a+");
#endif
    iniclist();

    /* Schedule the next expire */
    nexte = time((time_t *)NULL) + explimit / 10;

    /* Catch SIGTERM */
    signal(SIGTERM, catcher);

    /* Open the database and save the file descriptor */
#if DB_VERSION_MAJOR < 2
#ifdef REQ_HASH
    errno = 0;
    dbp = dbopen(dbfile,
		  (initdb) ? O_CREAT|O_TRUNC|O_RDWR : O_CREAT|O_RDWR,
		  0644, DB_HASH, (void *)NULL);
#else
    memset(&bti, 0, sizeof(bti));
    bti.psize = 512;
    errno = 0;
    dbp = dbopen(dbfile,
		  (initdb) ? O_CREAT|O_TRUNC|O_RDWR : O_CREAT|O_RDWR,
		  0644, DB_BTREE, &bti);
#endif
#elif DB_VERSION_MAJOR == 2
    memset(&adbenv, 0, sizeof(adbenv));
    dbenv = &adbenv;
    memset(&dbinfo, 0, sizeof(dbinfo));
    dbinfo.db_pagesize = 512;
#ifdef REQ_HASH
    errno = db_open(dbfile, DB_HASH,
			  (initdb) ? DB_TRUNCATE|DB_CREATE : DB_CREATE,
			  0644, dbenv, &dbinfo, &dbp);
#else
    errno = db_open(dbfile, DB_BTREE,
			  (initdb) ? DB_TRUNCATE|DB_CREATE : DB_CREATE,
			  0644, dbenv, &dbinfo, &dbp);
#endif
#elif DB_VERSION_MAJOR >= 4 && DB_VERSION_MINOR >= 1
    errno = db_create(&dbp, NULL, 0);
    if ( errno != 0 ) {
	syslog(LOG_ERR, "drac_run db_create failed: %m");
	exit(3);
    }
    dbp->set_pagesize(dbp, 512);
#ifdef REQ_HASH
    errno = dbp->open(dbp, NULL, dbfile, NULL, DB_HASH,
			  (initdb) ? DB_TRUNCATE|DB_CREATE : DB_CREATE,
			  0644);
#else
    errno = dbp->open(dbp, NULL, dbfile, NULL, DB_BTREE,
			  (initdb) ? DB_TRUNCATE|DB_CREATE : DB_CREATE,
			  0644);
#endif
#else
    errno = db_create(&dbp, NULL, 0);
    if ( errno != 0 ) {
	syslog(LOG_ERR, "drac_run db_create failed: %m");
	exit(3);
    }
    dbp->set_pagesize(dbp, 512);
#ifdef REQ_HASH
    errno = dbp->open(dbp, dbfile, NULL, DB_HASH,
			  (initdb) ? DB_TRUNCATE|DB_CREATE : DB_CREATE,
			  0644);
#else
    errno = dbp->open(dbp, dbfile, NULL, DB_BTREE,
			  (initdb) ? DB_TRUNCATE|DB_CREATE : DB_CREATE,
			  0644);
#endif
#endif
    if ( errno != 0 ) {
	syslog(LOG_ERR, "drac_run open failed: %m");
	exit(3);
    }
#if DB_VERSION_MAJOR < 2
    errno = 0;
    dbfd = dbp->fd(dbp);
#else
    errno = dbp->fd(dbp, &dbfd);
#endif
    if ( errno != 0 ) {
	syslog(LOG_ERR, "drac_run fd failed: %m");
	exit(3);
    }

    /* Repeat forever */
    while (1) {
#ifdef DEBUG
	fprintf(debugf, "Select bits: %x\n", svc_fdset.fds_bits[0]);
	fflush(debugf);
#endif

	/* Wait for action */
	memcpy(&readfs, &svc_fdset, sizeof(fd_set));
	to.tv_sec = explimit / 10;
	to.tv_usec = 0L;
	sel = select(FD_SETSIZE, &readfs, (fd_set *)NULL, (fd_set *)NULL, &to);
	now = time((time_t *)NULL);
	switch ( sel ) {
	  case 0:	/* Timeout */
	    nexte = now + explimit / 10;
	    expire();
	    break;
	  case (-1):	/* Error */
	    if ( errno != EINTR ) {
		syslog(LOG_ERR, "drac_run select failed: %m");
	    }
	    else if ( terminate ) {
#if DB_VERSION_MAJOR < 2
		(void)dbp->close(dbp);
#else
		(void)dbp->close(dbp, 0);
#endif
		exit(0);
	    }
	    break;
	  default:	/* Ready */
	    if ( now >= nexte ) {
		nexte = now + explimit / 10;
		expire();
	    }
	    svc_getreqset(&readfs);	/* Service the request */
	}
    }
}

/* Add an entry to the database */
addstat *
#ifdef DASH_C
dracproc_add_1_svc(argp, rqstp)
#else
dracproc_add_1(argp, rqstp)
#endif
	drac_add_parm *argp;
	struct svc_req *rqstp; {

	static addstat  result;
#ifdef TI_RPC
	struct netbuf *nb;
	struct netconfig *nc;
	char *cad, *pt;
#endif
#ifdef SOCK_RPC
	struct sockaddr_in *si;
#endif
	struct in_addr client_ip, requ_ip;
	DBT key, data;
	char akey[32], alimit[32];
	struct net_def *nd;

	result = ADD_SUCCESS;

	/* Get the IP address of the client */
#ifdef TI_RPC
	if ( (nc = getnetconfigent(rqstp->rq_xprt->xp_netid)) == NULL
	    || (nb = svc_getrpccaller(rqstp->rq_xprt)) == NULL
	    || (cad = taddr2uaddr(nc, nb)) == NULL ) {
	    if (nc) freenetconfigent(nc);
	    result = ADD_SYSERR;
	    return (&result);
	}
	if ( (pt = strrchr(cad, '.')) != NULL ) *pt = '\0';
	if ( (pt = strrchr(cad, '.')) != NULL ) *pt = '\0';
	client_ip.s_addr = inet_addr(cad);
	freenetconfigent(nc);
	free(cad);
#endif
#ifdef SOCK_RPC
	if ( (si = svc_getcaller(rqstp->rq_xprt)) == NULL ) {
	    result = ADD_SYSERR;
	    return (&result);
	}
	client_ip.s_addr = si->sin_addr.s_addr;
#endif
#ifdef DEBUG
	fprintf(debugf, "Client Address: %s\n", inet_ntoa(client_ip));
	fflush(debugf);
#endif

	/* Check agains the table of trusted clients */
	for ( nd = net_tbl; nd != NULL; nd = nd->nd_next ) {
	    if ( (client_ip.s_addr & nd->nd_mask.s_addr)
		== nd->nd_addr.s_addr ) break;
	}
	if ( nd == NULL ) {
	    result = ADD_PERM;
	    return (&result);
	}

	/* Set up for the add */
	requ_ip.s_addr = htonl(argp->ip_addr);	/* to network byte order */
#ifdef DEBUG
	fprintf(debugf, "Requested IP Address: %s\n",
		inet_ntoa(requ_ip));
	fflush(debugf);
#endif
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	strcpy(akey, inet_ntoa(requ_ip));
#ifdef CIDR_KEY
	strcat(akey, "/32");
#endif
	key.data = akey;
#ifdef TERM_KD
	key.size = strlen(akey) + 1;
#else
	key.size = strlen(akey);
#endif
	sprintf(alimit, "%lu", time((time_t *)NULL) + explimit);
	data.data = alimit;
#ifdef TERM_KD
	data.size = strlen(alimit) + 1;
#else
	data.size = strlen(alimit);
#endif

	/* Do the add and sync, with locking */
	if ( lockdb() == (-1) ) {
	    syslog(LOG_ERR, "dracproc_add_1 lockdb failed: %m");
	}
#if DB_VERSION_MAJOR < 2
	errno = 0;
	dbp->put(dbp, &key, &data, 0);
#else
	errno = dbp->put(dbp, NULL, &key, &data, 0);
#endif
	if ( errno != 0 ) {
	    syslog(LOG_ERR, "dracproc_add_1 put failed: %m");
	    result = ADD_SYSERR;
	}
#if DB_VERSION_MAJOR < 2
	errno = 0;
	dbp->sync(dbp, 0);
#else
	errno = dbp->sync(dbp, 0);
#endif
	if ( errno != 0 ) {
	    syslog(LOG_ERR, "dracproc_add_1 sync failed: %m");
	}
	(void)unlockdb();

	/* Send result code back to client */
	return (&result);
}

/* Add an entry to the database */
addstat *
#ifdef DASH_C
dracproc_add_2_svc(argp, rqstp)
#else
dracproc_add_2(argp, rqstp)
#endif
	drac_add_parm6 *argp;
	struct svc_req *rqstp; {

	static addstat  result;
	char buf[INET6_ADDRSTRLEN];
#ifdef TI_RPC
	struct netbuf *nb;
	struct netconfig *nc;
	char *cad, *pt;
#endif
#ifdef SOCK_RPC
	struct sockaddr_in *si;
#endif
	struct in_addr client_ip;
	struct in6_addr requ_ip;
	DBT key, data;
	char akey[INET6_ADDRSTRLEN+4], alimit[32];
	struct net_def *nd;

	result = ADD_SUCCESS;

	/* Get the IP address of the client */
#ifdef TI_RPC
	if ( (nc = getnetconfigent(rqstp->rq_xprt->xp_netid)) == NULL
		|| (nb = svc_getrpccaller(rqstp->rq_xprt)) == NULL
		|| (cad = taddr2uaddr(nc, nb)) == NULL ) {
		if (nc) freenetconfigent(nc);
			result = ADD_SYSERR;
			return (&result);
	}
	if ( (pt = strrchr(cad, '.')) != NULL ) *pt = '\0';
	if ( (pt = strrchr(cad, '.')) != NULL ) *pt = '\0';
	client_ip.s_addr = inet_addr(cad);
	freenetconfigent(nc);
	free(cad);
#endif
#ifdef SOCK_RPC
	if ( (si = svc_getcaller(rqstp->rq_xprt)) == NULL ) {
		result = ADD_SYSERR;
		return (&result);
	}
	client_ip.s_addr = si->sin_addr.s_addr;
#endif
#ifdef DEBUG
	fprintf(debugf, "Client Address: %s\n", inet_ntoa(client_ip));
	fflush(debugf);
#endif
	/* Check agains the table of trusted clients */
	for ( nd = net_tbl; nd != NULL; nd = nd->nd_next ) {
		if ( (client_ip.s_addr & nd->nd_mask.s_addr)
			== nd->nd_addr.s_addr ) break;
		}
		if ( nd == NULL ) {
			result = ADD_PERM;
			return (&result);
		}
		
		/* Set up for the add */
		memcpy(requ_ip.s6_addr, argp->ip_addr6,
			sizeof(requ_ip.s6_addr));
#ifdef DEBUG
	fprintf(debugf, "Requested IP Address: %s\n",
			inet_ntop(AF_INET6, requ_ip.s6_addr, buf, sizeof(buf)));
	fflush(debugf);
#endif
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	inet_ntop(AF_INET6, requ_ip.s6_addr, akey, sizeof(akey)-4);
#ifdef CIDR_KEY
	strcat(akey, "/128");
#endif
	key.data = akey;
#ifdef TERM_KD
	key.size = strlen(akey) + 1;
#else
	key.size = strlen(akey);
#endif
	sprintf(alimit, "%lu", time((time_t *)NULL) + explimit);
	data.data = alimit;
#ifdef TERM_KD
	data.size = strlen(alimit) + 1;
#else
	data.size = strlen(alimit);
#endif

	/* Do the add and sync, with locking */
	if ( lockdb() == (-1) ) {
		syslog(LOG_ERR, "dracproc_add_1 lockdb failed: %m");
	}
#if DB_VERSION_MAJOR < 2
	errno = 0;
	dbp->put(dbp, &key, &data, 0);
#else
	errno = dbp->put(dbp, NULL, &key, &data, 0);
#endif
	if ( errno != 0 ) {
		syslog(LOG_ERR, "dracproc_add_1 put failed: %m");
		result = ADD_SYSERR;
	}
#if DB_VERSION_MAJOR < 2
	errno = 0;
	dbp->sync(dbp, 0);
#else
	errno = dbp->sync(dbp, 0);
#endif
	if ( errno != 0 ) {
		syslog(LOG_ERR, "dracproc_add_1 sync failed: %m");
	}
	(void)unlockdb();

	/* Send result code back to client */
	return (&result);
}


/* Expire old entries from the database */
expire() {
#if DB_VERSION_MAJOR < 2
    int seqerr, flags;
#else
    DBC *dbcp;
#endif
    DBT key, data;
    time_t old;
    char alimit[32];

    /* Oldest possible entry is now */
    old = time((time_t *)NULL);

    /* Lock the database */
    if ( lockdb() == (-1) ) {
	syslog(LOG_ERR, "expire lockdb failed: %m");
    }

    /* Obtain a cursor */
#if DB_VERSION_MAJOR < 2
    flags = R_FIRST;
#else
# if DB_VERSION_MAJOR > 2 || DB_VERSION_MINOR >=6
    errno = dbp->cursor(dbp, NULL, &dbcp, 0);
#else
    errno = dbp->cursor(dbp, NULL, &dbcp);
#endif
    if ( errno != 0 ) {
	syslog(LOG_ERR, "expire cursor failed: %m");
	return 0;
    }
#endif

    /* Scan the database, deleting old entries */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
#if DB_VERSION_MAJOR < 2
    while ((seqerr = dbp->seq(dbp, &key, &data, flags)) == 0) {
	flags = R_NEXT;
#else
    while ((errno = dbcp->c_get(dbcp, &key, &data, DB_NEXT)) == 0) {
#endif
	memcpy(alimit, data.data, data.size);
#ifndef TERM_KD
	alimit[data.size] = '\0';
#endif
	if ( strtoul(alimit, (char **)NULL, 10) < old ) {
#if DB_VERSION_MAJOR < 2
	    errno = 0;
	    dbp->del(dbp, &key, R_CURSOR);
#else
	    errno = dbcp->c_del(dbcp, 0);
#endif
	    if ( errno != 0 ) {
		syslog(LOG_ERR, "expire c_del failed: %m");
	    }
	}
    }
#if DB_VERSION_MAJOR < 2
    if (seqerr != 1) {
#else
    if (errno != DB_NOTFOUND) {
#endif
	syslog(LOG_ERR, "expire c_get failed: %m");
    }

    /* Discard the cursor */
#if DB_VERSION_MAJOR < 2
    errno = 0;
    dbp->sync(dbp, 0);
#else
    (void)dbcp->c_close(dbcp);
    errno = dbp->sync(dbp, 0);
#endif
    if ( errno != 0 ) {
	syslog(LOG_ERR, "expire sync failed: %m");
    }

    /* Unlock the database */
    (void) unlockdb();
}

/* Lock the database */
lockdb() {
#ifdef FCNTL_LOCK
    struct flock lfd;

    memset(&lfd, 0, sizeof(lfd));
    lfd.l_type = F_WRLCK;
    return fcntl(dbfd, F_SETLKW, &lfd);
#endif
#ifdef FLOCK_LOCK
    return flock(dbfd, LOCK_EX);
#endif
}

/* Unlock the database */
unlockdb() {
#ifdef FCNTL_LOCK
    struct flock lfd;

    memset(&lfd, 0, sizeof(lfd));
    lfd.l_type = F_UNLCK;
    return fcntl(dbfd, F_SETLK, &lfd);
#endif
#ifdef FLOCK_LOCK
    return flock(dbfd, LOCK_UN);
#endif
}

/* Initialize the trusted client table */
/*	All in network byte order */
iniclist() {
    FILE *alfp;
    char buf[128], mask[32], addr[32], hname[128];
    struct net_def *nd;
    struct hostent *he;
    unsigned long **p;

    /* Check if the allow file exists */
    if ( (alfp = fopen(alfile, "r")) != NULL ) {

	/* Read lines from the file */
	while ( fgets(buf, sizeof(buf), alfp) != NULL ) {
	    if ( buf[0] == '#' ) continue;
	    if ( sscanf(buf, "%[0-9.] %[0-9.]", mask, addr) == 2 ) {

		/* Create table entry from the line */
		nd = (struct net_def *)malloc(sizeof(struct net_def));
		if ( nd != NULL ) {
		    nd->nd_next = net_tbl;
		    nd->nd_mask.s_addr = inet_addr(mask);
		    nd->nd_addr.s_addr = inet_addr(addr);
		    net_tbl = nd;
		}
		else {
		    syslog(LOG_ERR, "iniclist malloc failed");
		}
	    }
	}
    }
    else {

	/* Default to loopback address */
	nd = (struct net_def *)malloc(sizeof(struct net_def));
	if ( nd != NULL ) {
	    nd->nd_next = net_tbl;
	    nd->nd_mask.s_addr = htonl(0xffffffff);
	    nd->nd_addr.s_addr = htonl(0x7f000001);
	    net_tbl = nd;
	}

	/* Add local IP addresses */
#ifdef SYSINFO
	(void)sysinfo(SI_HOSTNAME, hname, sizeof(hname));
#endif
#ifdef GETHOST
	(void)gethostname(hname, sizeof(hname));
#endif
	if ( (he = gethostbyname(hname)) != NULL ) {
	    for (p = (unsigned long **)he->h_addr_list; *p != NULL; p++) {
		nd = (struct net_def *)malloc(sizeof(struct net_def));
		if ( nd != NULL ) {
		    nd->nd_next = net_tbl;
		    nd->nd_mask.s_addr = htonl(0xffffffff);
		    nd->nd_addr.s_addr = **p;
		    net_tbl = nd;
		}
		else {
		    syslog(LOG_ERR, "iniclist malloc failed");
		}
	    }
	}
    }
}

/**/
