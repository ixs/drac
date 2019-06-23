
#### Makefile for drac

## Tuneables

# Paths

INSTALL = /usr/ucb/install
EBIN = /usr/local/sbin
MAN = /usr/local/man/man

# OS-Dependant settings

# Choose one of this pair...
# -DTI_RPC	# Transport-independant RPC
# -DSOCK_RPC	# Socket RPC

# Choose one of this pair...
# -DFCNTL_LOCK	# fcntl() locking
# -DFLOCK_LOCK	# flock() locking

# Choose one of this pair...
# -DSYSINFO	# hostname from sysinfo()
# -DGETHOST	# hostname from gethostname()

# If rpcgen -C is specified below...
# -DDASH_C	# ANSI-C mode

# Settings for postfix and exim

# Do not set these for sendmail
# -DREQ_HASH	# requires hash format
# -DCIDR_KEY	# keys in CIDR format
# -DTERM_KD	# keys and data nul-terminated

DEFS = -DTI_RPC -DFCNTL_LOCK -DSYSINFO

# Compiler flags 
CC = cc
RANLIB = :
CFLAGS = $(DEFS) -g -I/usr/local/src/db/db-4.1.25/build_unix
#CFLAGS = $(DEFS) -g -I/usr/local/src/db/db-3.1.17/build_unix
#CFLAGS = $(DEFS) -g -I/usr/local/src/db/db-2.4.14/Unix
#CFLAGS = $(DEFS) -g -I/usr/local/src/db/db.1.85/PORT/sunos.5.2/include
LDLIBS = -L/usr/local/src/db/db-4.1.25/build_unix -lnsl -ldb-4.1
#LDLIBS = -L/usr/local/src/db/db-3.1.17/build_unix -lnsl -ldb
#LDLIBS = -L/usr/local/src/db/db-2.4.14/Unix -lnsl -ldb
#LDLIBS = -L/usr/local/src/db/db.1.85/PORT/sunos.5.2 -lnsl -ldb
TSTLIBS = -L. -ldrac -lnsl
RPCGENFLAGS = 
#RPCGENFLAGS = -C

# Man sections
MANLIB = 3
MANADM = 1m

## Nothing to change after this point

# Package files

DOCFILES = COPYRIGHT Changes INSTALL PORTING README version.h dracauth.3 \
	rpc.dracd.1m dracd.allow-sample dracd-setup dracd-setup.linux
MAKEFILE = Makefile
RPC_SRC = drac.x
LIB_SRC = dracauth.c
SVC_SRC = rpc.dracd.c
TST_SRC = testing.c
TST6_SRC = testing6.c
PACKAGE = $(DOCFILES) $(MAKEFILE) $(RPC_SRC) $(LIB_SRC) $(SVC_SRC) $(TST_SRC) $(TST6_SRC)

# Final targets

CLIENT = testing
CLIENT6 = testing6
SERVER = rpc.dracd
LIBRAR = libdrac.a

# rpcgen output

RPC_H = drac.h
RPC_XDR = drac_xdr.c
RPC_SVC = drac_svc.c
RPC_CLNT = drac_clnt.c
RPC_ALL = $(RPC_H) $(RPC_XDR) $(RPC_SVC) $(RPC_CLNT)

# Object files

LIB_OBJ = dracauth.o
SVC_OBJ = rpc.dracd.o
TST_OBJ = testing.o
TST_OBJ6 = testing6.o
H_OBJS = drac_xdr.o drac_svc.o drac_clnt.o $(SVC_OBJ) $(LIB_OBJ)
L_OBJS = $(LIB_OBJ) drac_xdr.o drac_clnt.o
S_OBJS = $(SVC_OBJ) drac_xdr.o drac_svc.o

# Rules

all: $(CLIENT) $(CLIENT6) $(SERVER)

$(RPC_ALL): $(RPC_SRC) 
	rpcgen $(RPCGENFLAGS) $(RPC_SRC)

$(H_OBJS): $(RPC_H) 

$(LIB_OBJ) $(SVC_OBJ): $(MAKEFILE)

$(LIBRAR): $(L_OBJS)
	rm -f $@
	ar cq $@ $(L_OBJS)
	$(RANLIB) $@

$(CLIENT): $(TST_OBJ) $(LIBRAR)
	$(CC) -o $(CLIENT) $(TST_OBJ) $(TSTLIBS)

$(CLIENT6): $(TST6_OBJ) $(LIBRAR)
	$(CC) -o $(CLIENT6) $(TST6_OBJ) $(TSTLIBS)

$(SERVER): $(S_OBJS) 
	$(CC) -o $(SERVER) $(S_OBJS) $(LDLIBS)

clean:
	rm -f core $(RPC_ALL) $(H_OBJS) $(TST_OBJ) $(TST6_OBJ) $(CLIENT) $(CLIENT6) \
		$(SERVER) $(LIBRAR)

tar: $(PACKAGE)
	tar cf drac.tar $(PACKAGE)

install: $(SERVER)
	$(INSTALL) -c -o bin -g bin -m 0755 $(SERVER) $(EBIN)

install-man: $(SERVER).1m dracauth.3
	$(INSTALL) -c -m 0444 $(SERVER).1m $(MAN)$(MANADM)/$(SERVER).$(MANADM)
	$(INSTALL) -c -m 0444 dracauth.3 $(MAN)$(MANLIB)/dracauth.$(MANLIB)

