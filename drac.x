/*
 * Dynamic Relay Authorization Control
 */

#ifdef RPC_SVC
%#define svc_run drac_run
%#define main dracmain
#endif

#ifdef RPC_HDR
%#ifndef CLIENT
%#include <rpc/rpc.h>
%#endif
#endif

struct drac_add_parm {
	unsigned long ip_addr;	/* In network order */
};

enum addstat {
	ADD_SUCCESS,	/* Succeeded */
	ADD_PERM,	/* Permission denied */
	ADD_SYSERR,	/* System error */
	ADD_UNKOWN	/* Unknown */
};

program DRACPROG {
	version DRACVERS {
		/*
		 * Update my passwd entry 
		 */
		addstat
		DRACPROC_ADD(drac_add_parm) = 1;
	} = 1;
} = 900101;
