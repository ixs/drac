/*
 * DRAC client library functions:
 *
 * dracauth(server, userip, errmsg)
 * 	server = hostname of DRAC server
 * 	userip = IP address to authorize for relaying
 *	errmsg = place to store pointer to error message
 *
 * dracconn(server, errmsg)
 * 	server = hostname of DRAC server
 *	errmsg = place to store pointer to error message
 * dracsend(userip, errmsg)
 * 	userip = IP address to authorize for relaying
 *	errmsg = place to store pointer to error message
 * dracdisc(errmsg)
 *	errmsg = place to store pointer to error message
 */

#include <sys/types.h>
#include <netinet/in.h>
#include "drac.h"


int
dracauth(server, userip, errmsg)
	char *server; unsigned long userip; char **errmsg; {

	CLIENT *clnt;
	addstat  *result;
	drac_add_parm  dracproc_add_1_arg;

#ifdef TI_RPC
	clnt = clnt_create(server, DRACPROG, DRACVERS, "datagram_v");
#endif
#ifdef SOCK_RPC
	clnt = clnt_create(server, DRACPROG, DRACVERS, "udp");
#endif
	if (clnt == (CLIENT *) NULL) {
		if ( errmsg ) *errmsg = clnt_spcreateerror(server);
		return (-1);
	}
	dracproc_add_1_arg.ip_addr = ntohl(userip); /* to host byte order */
	result = dracproc_add_1(&dracproc_add_1_arg, clnt);
	if (result == (addstat *) NULL) {
		if ( errmsg ) *errmsg = clnt_sperror(clnt, "call failed");
		clnt_destroy(clnt);
		return (-2);
	}
	clnt_destroy(clnt);
	if ( errmsg ) {
	    switch (*result) {
	      case ADD_SUCCESS:
		*errmsg = "Server reports add succeeded";
		break;
	      case ADD_PERM:
		*errmsg = "Server reports permission denied";
		break;
	      case ADD_SYSERR:
		*errmsg = "Server reports system error";
		break;
	      default:
		*errmsg = "Server reports unknown error";
	    }
	}
	return *result;
}

static CLIENT *clnt;

int
dracconn(server, errmsg)
	char *server; char **errmsg; {

#ifdef TI_RPC
	clnt = clnt_create(server, DRACPROG, DRACVERS, "datagram_v");
#endif
#ifdef SOCK_RPC
	clnt = clnt_create(server, DRACPROG, DRACVERS, "udp");
#endif
	if (clnt == (CLIENT *) NULL) {
		if ( errmsg ) *errmsg = clnt_spcreateerror(server);
		return (-1);
	}
	if ( errmsg ) *errmsg = "Connect succeeded";
	return 0;
}

int
dracsend(userip, errmsg)
	unsigned long userip; char **errmsg; {

	addstat  *result;
	drac_add_parm  dracproc_add_1_arg;

	if (clnt == (CLIENT *) NULL) {
		if ( errmsg ) *errmsg = "Not connected";
		return (-1);
	}
	dracproc_add_1_arg.ip_addr = ntohl(userip); /* to host byte order */
	result = dracproc_add_1(&dracproc_add_1_arg, clnt);
	if (result == (addstat *) NULL) {
		if ( errmsg ) *errmsg = clnt_sperror(clnt, "call failed");
		return (-2);
	}
	if ( errmsg ) {
	    switch (*result) {
	      case ADD_SUCCESS:
		*errmsg = "Server reports add succeeded";
		break;
	      case ADD_PERM:
		*errmsg = "Server reports permission denied";
		break;
	      case ADD_SYSERR:
		*errmsg = "Server reports system error";
		break;
	      default:
		*errmsg = "Server reports unknown error";
	    }
	}
	return *result;
}


int
dracdisc(errmsg)
	char **errmsg; {

	if (clnt == (CLIENT *) NULL) {
		if ( errmsg ) *errmsg = "Not connected";
		return (-1);
	}
	clnt_destroy(clnt);
	clnt = (CLIENT *) NULL;
	if ( errmsg ) *errmsg = "Disconnect succeeded";
	return 0;
}

int
dracauth6(server, userip6, errmsg)
	char *server; unsigned char userip6[16]; char **errmsg; {

	CLIENT *clnt6;
	addstat  *result;
	drac_add_parm6  dracproc_add_2_arg;

#ifdef TI_RPC
	clnt6 = clnt_create(server, DRACPROG, DRACVERS6, "datagram_v");
#endif
#ifdef SOCK_RPC
	clnt6 = clnt_create(server, DRACPROG, DRACVERS6, "udp");
#endif
	if (clnt6 == (CLIENT *) NULL) {
		if ( errmsg ) *errmsg = clnt_spcreateerror(server);
		return (-1);
	}
	memcpy(dracproc_add_2_arg.ip_addr6, userip6,
	 	sizeof(dracproc_add_2_arg.ip_addr6));
	result = dracproc_add_2(&dracproc_add_2_arg, clnt6);
	if (result == (addstat *) NULL) {
		if ( errmsg ) *errmsg = clnt_sperror(clnt, "call failed");
		clnt_destroy(clnt6);
		return (-2);
	}
	clnt_destroy(clnt6);
	if ( errmsg ) {
	    switch (*result) {
	      case ADD_SUCCESS:
		*errmsg = "Server reports add succeeded";
		break;
	      case ADD_PERM:
		*errmsg = "Server reports permission denied";
		break;
	      case ADD_SYSERR:
		*errmsg = "Server reports system error";
		break;
	      default:
		*errmsg = "Server reports unknown error";
	    }
	}
	return *result;
}

static CLIENT *clnt6;

int
dracconn6(server, errmsg)
	char *server; char **errmsg; {

#ifdef TI_RPC
	clnt6 = clnt_create(server, DRACPROG, DRACVERS6, "datagram_v");
#endif
#ifdef SOCK_RPC
	clnt6 = clnt_create(server, DRACPROG, DRACVERS6, "udp");
#endif
	if (clnt6 == (CLIENT *) NULL) {
		if ( errmsg ) *errmsg = clnt_spcreateerror(server);
		return (-1);
	}
	if ( errmsg ) *errmsg = "Connect succeeded";
	return 0;
}

int
dracsend6(userip6, errmsg)
	unsigned char userip6[16]; char **errmsg; {

	addstat  *result;
	drac_add_parm6  dracproc_add_2_arg;

	if (clnt6 == (CLIENT *) NULL) {
		if ( errmsg ) *errmsg = "Not connected";
		return (-1);
	}
	memcpy(dracproc_add_2_arg.ip_addr6, userip6,
		sizeof(dracproc_add_2_arg.ip_addr6));
	result = dracproc_add_2(&dracproc_add_2_arg, clnt6);
	if (result == (addstat *) NULL) {
		if ( errmsg ) *errmsg = clnt_sperror(clnt6, "call failed");
		return (-2);
	}
	if ( errmsg ) {
	    switch (*result) {
	      case ADD_SUCCESS:
		*errmsg = "Server reports add succeeded";
		break;
	      case ADD_PERM:
		*errmsg = "Server reports permission denied";
		break;
	      case ADD_SYSERR:
		*errmsg = "Server reports system error";
		break;
	      default:
		*errmsg = "Server reports unknown error";
	    }
	}
	return *result;
}


int
dracdisc6(errmsg)
	char **errmsg; {

	if (clnt6 == (CLIENT *) NULL) {
		if ( errmsg ) *errmsg = "Not connected";
		return (-1);
	}
	clnt_destroy(clnt6);
	clnt6 = (CLIENT *) NULL;
	if ( errmsg ) *errmsg = "Disconnect succeeded";
	return 0;
}

/**/
