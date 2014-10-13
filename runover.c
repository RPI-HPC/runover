/* runover.c --
 *
 * Program to spawn jobs over several processors.
 *
 */

#ifndef RO_CONFIG_SCRIPT
#define RO_CONFIG_SCRIPT	"./config-script.sh"
#endif
#ifndef RO_MACHINE_SCRIPT
#define RO_MACHINE_SCRIPT	"./machine-script.sh"
#endif

#define MAX_MACHINE_LINE	1024
#define MAX_CONFIG_LINE		1024

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "qo.h"
#include "ca.h"
#include "av.h"


/* Configuration information.
 */

typedef struct roConfigData {
    char*	machineScript;
    char*	jobName;
    char*	spawnCommand;
} roConfigData;

typedef struct roJobData {
    const char* inTemplate;
    const char*	outTemplate;
    const char*	errTemplate;
    const char**	progargv;
} roJobData;


/* MachineItem and MachineList --
 *
 * A MachineItem object represents an instance of a process on a
 * particular machine.  It needs to include the hostname, the status
 * of the job currently in progress, and various queue linkages.
 *
 * A MachineList object contains a set of MachineItem objects, as well
 * as the queue control blocks for those items.
 */

typedef struct MachineItem {
    char*		mname;
    pid_t		runPid;
    QUEUE_LINKAGE(all, struct MachineItem*);
    QUEUE_LINKAGE(ready, struct MachineItem*);
    QUEUE_LINKAGE(run, struct MachineItem*);
} MachineItem;

typedef struct MachineList {
    size_t		mcnt;
    QUEUE_CONTROL_BLOCK(all, struct MachineItem*);
    QUEUE_CONTROL_BLOCK(ready, struct MachineItem*);
    QUEUE_CONTROL_BLOCK(run, struct MachineItem*);
} MachineList;


/* ParseMachineFile --
 *
 * Parse the machine file information from the specified file stream.
 * Return a machine structure.
 */
MachineList*
ParseMachineFile(FILE* mff)
{
    MachineList*	ms;
    MachineItem*	mi;
    char		ml[MAX_MACHINE_LINE+1];

    /*
     * Allocate, initialize the MachineList object.
     */
    ms = (MachineList*) malloc(sizeof(MachineList));
    /*FIXME: Out of memory */
    ms->mcnt = 0;
    QUEUE_CONTROL_BLOCK_INIT(all, ms);
    QUEUE_CONTROL_BLOCK_INIT(ready, ms);
    QUEUE_CONTROL_BLOCK_INIT(run, ms);

    /*
     * Read lines from machine file, and parse.
     */
    while (fgets(ml, MAX_MACHINE_LINE+1, mff) != NULL) {
	size_t	mls = strlen(ml);
	char*	cp;

	/*
	 * Remove trailing newline.  If missing, the line was
	 * truncated.
	 */
	if (ml[--mls] == '\n') {
	    ml[mls] = '\0';
	}
	/*FIXME: What to do if line was truncated? */

	/*
	 * Skip leading whitespace.  Check if we have a comment or
	 * blank line.
	 */
	for (cp = ml;  *cp && isspace(*cp);  ++cp)
	    ;
	if (!*cp || *cp == '#') {
	    continue;
	}

	/*
	 * Clean up any trailing whitespace.
	 */
	{
	    char *ecp;
	    for (ecp = cp + strlen(cp) - 1;  isspace(*ecp);  --ecp) {
		*ecp = '\0';
	    }
	}

	/*
	 * Create a machine item.  Add it to the list of machines.
	 */
	mi = (MachineItem*) malloc(sizeof(MachineItem));
	/*FIXME: OOM */
	mi->mname = (char*) malloc(strlen(cp) + 1);
	/*FIXME: OOM */
	strcpy(mi->mname, cp);
	QUEUE_ADD(all, ms, mi);
	QUEUE_ADD(ready, ms, mi);
	ms->mcnt++;
    }
    return ms;
}

/* MainSignalHandler --
 *
 * Synopsis:
 *
 *     Signal handler for the main program.  This flags that SIGINT
 *     etc. has been received; the signal is then propogated to any
 *     running spawner subprocesses.  Those spawners are responsible,
 *     in turn, for propogating the signal to the remote machine.
 */
static sig_atomic_t saw_SIGINT = 0;
static sig_atomic_t saw_SIGQUIT = 0;
static void
MainSignalHandler(int s)
{
    switch (s) {
    case SIGINT:
	saw_SIGINT = 1;
	break;
    case SIGQUIT:
	saw_SIGQUIT = 1;
	break;
    default:
	break;
    }	
}

/* WaitOnMachines --
 *
 * Wait for a process to complete, and move the corresponding
 * MachineItem from the run queue to the ready queue.
 */

static void
WaitOnMachines(MachineList* ms)
{
    pid_t	rc;
    int		ws;

    rc = wait(&ws);
    if (rc > 0) {
	MachineItem*	mi;
	/*
	 * rc is the pid of the child that exited.  Scan through the
	 * run queue to identify the MachineItem, and move it to the
	 * ready queue.
	 */
	for (mi = QUEUE_HEAD(run, ms);  mi != NULL;  mi = QUEUE_NEXT(run, mi)) {
	    if (mi->runPid == rc) {
		break;
	    }
	}
	if (mi != NULL) {
	    QUEUE_REMOVE(run, ms, mi);
	    QUEUE_ADD(ready, ms, mi);
	}
    } else if (errno == EINTR) {
	/*
	 * A signal was caught.  Pass it along to the various spawner
	 * subprocesses.
	 */
	printf("Caught signal!\n");
	if (saw_SIGINT) {
	    printf("Handle SIGINT\n");
	} else if (saw_SIGQUIT) {
	    printf("Handle SIGQUIT\n");
	}
    }
}


/* GetReadyMachine --
 *
 * Get a machine from the 'ready' queue.  Wait if neccessary.
 */

static
MachineItem*
GetReadyMachine(MachineList* ms)
{
    MachineItem* mi;

    do {
	QUEUE_TAKE(ready, ms, mi);
	if (mi != NULL) {
	    return mi;
	}

	WaitOnMachines(ms);
    } while (1);

}

/* RewriteString --
 *
 * Generate a string, to be freed with "free" by the caller, with
 * substitutions performed.
 */

static char*
RewriteString(const char* param, roConfigData* rcd, size_t proc)
{
    CharAccum	ca;
    char	buf[50];

    const char*	pp;

    enum { fsCHAR, fsPCT } fmtState = fsCHAR;

    CHARACCUM_INIT(&ca);

    /*
     * Copy from param to cp.
     */
    for (pp = param;  *pp;  ++pp) {
	switch (fmtState) {
	case fsCHAR:
	    if (*pp == '%') {
		fmtState = fsPCT;
	    } else {
		CHARACCUM_APPEND_CHAR(&ca, *pp);
	    }
	    break;
	case fsPCT:
	    switch (*pp) {
	    case '%':
		CHARACCUM_APPEND_CHAR(&ca, '%');
		fmtState = fsCHAR;
		break;
	    case 'j':
		CHARACCUM_APPEND_STR(&ca, rcd->jobName);
		fmtState = fsCHAR;
		break;
	    case 'p':
		sprintf(buf, "%lu", (unsigned long) proc);
		CHARACCUM_APPEND_STR(&ca, buf);
		fmtState = fsCHAR;
	    default:
		/* FIXME: What to do here??? */
		fmtState = fsCHAR;
		break;
	    }
	}
    }

    CHARACCUM_FINALIZE(&ca);
}

/* SpawnProcess --
 *
 * Spawn a process.
 */

void
SpawnProcess(const char* progname, MachineList* ms, MachineItem* mi, roConfigData* rcd, size_t proc, roJobData* rjd)
{
    const char**	nv;
    const char*		inPath = (const char*) NULL;
    const char*		outPath = (const char*) NULL;
    const char*		errPath = (const char*) NULL;
    pid_t		pid;

    /* 
     * Rewrite args, inserting spawn command and remote host name.
     */
    {
	const char**	ap;
	AV_Control	avc;

	AV_Init(&avc);

	AV_AddString(&avc, rcd->spawnCommand);
	AV_AddString(&avc, mi->mname);

	for (ap = rjd->progargv; *ap != NULL;  ++ap) {
	    const char*	np;
	    np = RewriteString(*ap, rcd, proc);
	    AV_AddString(&avc, np);
	    free((char*) np);
	}

	nv = AV_Finalize(&avc, NULL);

    }
    if (rjd->inTemplate) {
	inPath = RewriteString(rjd->inTemplate, rcd, proc);
    }
    if (rjd->outTemplate) {
	outPath = RewriteString(rjd->outTemplate, rcd, proc);
    }
    if (rjd->errTemplate) {
	errPath = RewriteString(rjd->errTemplate, rcd, proc);
    }


    /*
     * Fork.
     */

    pid = fork();
    if (pid > 0) {
	/*
	 * I am parent process.
	 */
	mi->runPid = pid;
    } else if (pid == 0) {
	/*
	 * I am child process.
	 */

	if (inPath) {
	    int fd = open(inPath, O_RDONLY);
	    if (fd < 0) {
		fprintf(stderr, "%s: Error opening \"%s\": %s\n",
			progname, inPath, strerror(errno));
		exit(1);
	    }
	    if (fd != 0) {
		close(0);
		dup2(fd, 0);
		close(fd);
	    }
	}

	if (outPath) {
	    int fd = open(outPath, O_WRONLY|O_APPEND|O_CREAT, 0644);
	    if (fd < 0) {
		fprintf(stderr, "%s: Error opening \"%s\": %s\n",
			progname, outPath, strerror(errno));
		exit(1);
	    }
	    if (fd != 1) {
		close(1);
		dup2(fd, 1);
		close(fd);
	    }
	}

	if (errPath) {
	    int fd = open(errPath, O_WRONLY|O_APPEND|O_CREAT, 0644);
	    if (fd < 0) {
		fprintf(stderr, "%s: Error opening \"%s\": %s\n",
			progname, errPath, strerror(errno));
		exit(1);
	    }
	    if (fd != 2) {
		close(2);
		dup2(fd, 2);
		close(fd);
	    }
	}

	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	setsid();

	execvp(nv[0], (char* const*)nv);
	/*FIXME: Big error!!! */
    }

    free((char*) nv);
    if (outPath) {
	free((char*) outPath);
    }
    if (errPath) {
	free((char*) errPath);
    }
}

/* SpawnJob --
 * 
 * Spawn the various processes in this job.
 */

void
SpawnJob(char* progname, MachineList* ms, roConfigData* rcd, size_t np, roJobData* rjd)
{
    size_t	proc;

    /*
     * Set up signal handling.
     */
    {
	struct sigaction sa;

	sa.sa_handler = MainSignalHandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
    }

    /*
     * Spawn the jobs.
     */
    for (proc = 0;  proc < np;  ++proc) {
	MachineItem*	mi;
	mi = GetReadyMachine(ms);
	SpawnProcess(progname, ms, mi, rcd, proc, rjd);
	QUEUE_ADD(run, ms, mi);
    }

    /*
     * Wait until everything is done.
     */
    while (QUEUE_HEAD(run, ms) != NULL) {
	WaitOnMachines(ms);
    }
}

/*==================================================
*
* Configuration file parsing.
*
*==================================================*/

/* SetMachineScript --
 *
 * Set the name of the machine script file.
 *
 */

static void SetMachineScript(roConfigData* rcd, const char* mfn)
{
    if (rcd->machineScript != NULL) {
	free(rcd->machineScript);
    }
    rcd->machineScript = (char*) malloc(strlen(mfn)+1);
    /*FIXME: Out of memory */
    strcpy(rcd->machineScript, mfn);
}

/* SetJobName --
 *
 * Set the name of the job.
 *
 */

static void SetJobName(roConfigData* rcd, const char* jobName)
{
    if (rcd->jobName != NULL) {
	free(rcd->jobName);
    }
    rcd->jobName = (char*) malloc(strlen(jobName)+1);
    /*FIXME: Out of memory */
    strcpy(rcd->jobName, jobName);
}

/* SetSpawnCommand --
 *
 * Set the name of the command to use for spawning remote shells.
 *
 */

static void SetSpawnCommand(roConfigData* rcd, const char* spawnCommand)
{
    if (rcd->spawnCommand != NULL) {
	free(rcd->spawnCommand);
    }
    rcd->spawnCommand = (char*) malloc(strlen(spawnCommand)+1);
    /*FIXME: Out of memory */
    strcpy(rcd->spawnCommand, spawnCommand);
}


/* ParseConfigScript --
 *
 * Parse a configuration script.
 *
 */

static roConfigData*
ParseConfigScript(const char* progname, FILE* cff)
{
    roConfigData*	rcd;
    char		cl[MAX_CONFIG_LINE+1];
    unsigned long	lineCount = 0;

    /*
     * Allocate, initialize the roConfigData object.
     */
    rcd = (roConfigData*) malloc(sizeof(roConfigData));
    /*FIXME: Out of memory */
    rcd->machineScript = rcd->jobName = (char*) NULL;
    SetMachineScript(rcd, RO_MACHINE_SCRIPT);
    SetJobName(rcd, "");
    /*FIXME: Default at configure time. */
    SetSpawnCommand(rcd, "/usr/bin/ssh");

    /*
     * Read lines from the configuration file, and parse.
     */
    while (fgets(cl, MAX_CONFIG_LINE+1, cff) != NULL) {
	size_t	cls = strlen(cl);
	char*	cp;
	char*	tok;

	lineCount++;

	/*
	 * Remove the trailing newline.  If missing, the line was
	 * truncated.
	 */
	if (cl[--cls] == '\n') {
	    cl[cls] = '\0';
	}
	/*FIXME: What if truncated? */

	/*
	 * Skip leading whitespace.  Check if we have a comment or
	 * blank line.
	 */
	for (cp = cl;  *cp && isspace(*cp);  ++cp)
	    ;
	if (!*cp || *cp == '#') {
	    continue;
	}

	/*
	 * Clean up any trailing whitespace.
	 */
	{
	    char *ecp;
	    for (ecp = cp + strlen(cp) - 1;  isspace(*ecp);  --ecp) {
		*ecp = '\0';;
	    }
	}

	/*
	 * We have a line.  First token identifies what we are
	 * configuring.  So break out the first token, then skip
	 * following white space.
	 */
	tok = cp;
	while (*cp && !isspace(*cp))
	    ++cp;
	if (*cp) {
	    *cp++ = '\0';
	}
	while (*cp && isspace(*cp))
	    ++cp;

	if (0 == strcmp(tok, "machinescript")) {
	    if (!*cp) {
		fprintf(stderr, "%s: %lu: machinescript directive requires a path\n",
			progname, lineCount);
		exit(1);
	    }
	    SetMachineScript(rcd, cp);
	} else if (0 == strcmp(tok, "jobname")) {
	    if (!*cp) {
		fprintf(stderr, "%s: %lu: jobname directive requires a path\n",
			progname, lineCount);
		exit(1);
	    }
	    SetJobName(rcd, cp);
	} else if (0 == strcmp(tok, "spawncommand")
		   || 0 == strcmp(tok, "spawncmd")
		   || 0 == strcmp(tok, "spawn")) {
	    if (!*cp) {
		fprintf(stderr, "%s: %lu: spawncommand directive requires a path\n",
			progname, lineCount);
		exit(1);
	    }
	    SetSpawnCommand(rcd, cp);
	} else {
	    fprintf(stderr, "%s: %lu: Unknown directive \"%s\"\n",
		    progname, lineCount, tok);
	    exit(1);
	}
    }

    return rcd;
}

/*==================================================
 *
 * Main program
 *
 *==================================================*/

/* Usage --
 *
 * Print a usage message, then exit with the specified code.
 */
static void
Usage(char* av0, int ec)
{
    fprintf(stderr, "Usage: %s [-np NP] [-machinefile MF] -- PROG ARGS...\n\n",
	    av0);
    fprintf(stderr, "  -np NP           Run job NP times.\n");
    fprintf(stderr, "  -machinefile MF  Use machines in MF.\n");
    fprintf(stderr, "  -stderr ERRTEMP  Path template for error file.\n");
    fprintf(stderr, "  -stdin INTEMP    Path template for input file.\n");
    fprintf(stderr, "  -stdout OUTTEMP  Path template for output file.\n");

    exit(ec);
}

/* main --
 *
 * Main program.
 */

int
main(int argc, char* argv[])
{
    char*	progname;
    int		np = -1;
    const char* 	mf = NULL;
    MachineList*	ms;
    roConfigData*	rcd;
    roJobData		rjd;

    /*
     * The program name, for error messages, etc.
     */
    progname = strrchr(argv[0], '/');
    if (!progname) {
	progname = argv[0];
    }

    /*
     * Parse the configuration script.
     */
    {
	FILE* cff = popen(RO_CONFIG_SCRIPT, "r");
	if (cff == (FILE*) NULL) {
	    fprintf(stderr, "%s: Unable to open configuration script \"%s\"\n",
		    progname, RO_CONFIG_SCRIPT);
	    exit(1);
	}
	rcd = ParseConfigScript(progname, cff);
	pclose(cff);
    }

    /*
     * Parse command line.  Options are of the form "-np", to resemble
     * normal MPI commands, so we cannot simply use getopt.
     */
    rjd.inTemplate = (const char*) NULL;
    rjd.outTemplate = (const char*) NULL;
    rjd.errTemplate = (const char*) NULL;
    {
	const char** op;
	enum { sOPT, sNP, sMACHINE,
	       sSTDIN, sSTDOUT, sSTDERR,
	       sPARAM, sDONE } state;

	state = sOPT;
	for (op = (const char**) (argv+1);  *op;  ++op) {
	    switch (state) {
	    case sOPT:
		/* Getting an option. */
		if (!strcmp(*op, "-np")) {
		    state = sNP;
		} else if (!strcmp(*op, "-machinefile")) {
		    state = sMACHINE;
		} else if (!strcmp(*op, "-stdin")) {
		    state = sSTDIN;
		} else if (!strcmp(*op, "-stdout")) {
		    state = sSTDOUT;
		} else if (!strcmp(*op, "-stderr")) {
		    state = sSTDERR;
		} else if (!strcmp(*op, "-help")
			   || !strcmp(*op, "-h")
			   || !strcmp(*op, "-?")) {
		    Usage(progname, 0);
		} else if (!strcmp(*op, "--")) {
		    state = sPARAM;
		} else if (*op[0] == '-') {
		    fprintf(stderr, "%s: Unknown option \"%s\"\n",
			    progname, *op);
		    Usage(progname, 1);
		} else {
		    rjd.progargv = op;
		    state = sDONE;
		    goto parseDone;
		}
		break;

	    case sNP:
	    {
		char *	ep;
		long	lnp;
		lnp = strtol(*op, &ep, 0);
		if (lnp <= 0 || *ep) {
		    fprintf(stderr, "%s: \"-np\" requires a positive integer.\n",
			    progname);
		    Usage(progname, 1);
		}
		/* FIXME: Make sure we don't go too large. */
		np = (int) lnp;
		state = sOPT;
		break;
	    }

	    case sMACHINE:
		mf = *op;
		state = sOPT;
		break;

	    case sSTDIN:
		rjd.inTemplate = *op;
		state = sOPT;
		break;

	    case sSTDOUT:
		rjd.outTemplate = *op;
		state = sOPT;
		break;

	    case sSTDERR:
		rjd.errTemplate = *op;
		state = sOPT;
		break;

	    case sPARAM:
		rjd.progargv = op;
		state = sDONE;
		goto parseDone;

	    case sDONE:
		break;
	    }
	}

	/*
	 * Need to make sure we end in sPARAM.
	 */
    parseDone:
	switch (state) {
	case sOPT:
	case sPARAM:
	    fprintf(stderr, "%s: Missing program to run.\n", progname);
	    Usage(progname, 1);

	case sNP:
	    fprintf(stderr, "%s: \"-np\" requires processor count.\n",
		    progname);
	    Usage(progname, 1);
	case sMACHINE:
	    fprintf(stderr, "%s: \"-machinefile\" requires the machine file.\n",
		    progname);
	    Usage(progname, 1);
	case sSTDOUT:
	    fprintf(stderr, "%s: \"-stdout\" requires a file template.\n",
		    progname);
	    Usage(progname, 1);
	case sSTDERR:
	    fprintf(stderr, "%s: \"-stderr\" requires a file template.\n",
		    progname);
	    Usage(progname, 1);
	case sDONE:
	    break;
	}
    }

    /*
     * We need a to process the machine list.  If no list was
     * provided, we need to choose a default.
     */
    if (mf != NULL) {
	/*
	 * A machine list was specified.
	 */
	FILE* mff = fopen(mf, "r");
	if (mff == (FILE*) NULL) {
	    fprintf(stderr, "%s: Unable to open machine file \"%s\"\n",
		    progname, mf);
	    exit(1);
	}
	ms = ParseMachineFile(mff);
	fclose(mff);
    } else {
	/*
	 * Use the program to generate the machine list.
	 */
	FILE* mff = popen(rcd->machineScript, "r");
	if (mff == (FILE*) NULL) {
	    fprintf(stderr, "%s: Unable to open machine script \"%s\"\n",
		    progname, RO_MACHINE_SCRIPT);
	    exit(1);
	}
	ms = ParseMachineFile(mff);
	pclose(mff);
    }

    /*
     * If 'np' was not specified, use the size of the machine list.
     */
    if (np < 0) {
	np = ms->mcnt;
    }

    /*
     * Spawn processes in this job.
     */
    SpawnJob(progname, ms, rcd, np, &rjd);


#if 0
    /* DEBUG */
    {
	const char**	progargv = rjd.progargv;

	printf("np = %d\n", np);
	for (; *progargv; ++progargv) {
	    printf("AV: %s\n", *progargv);
	}
	{
	    MachineItem*	mi = QUEUE_HEAD(all, ms);
	    while (mi) {
		printf("HOST: |%s|\n", mi->mname);
		mi = QUEUE_NEXT(all, mi);
	    }
	}
    }
#endif

    return 0;
}
