/* Configuration file parser. */

#include <cfp.h>
#include <av.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


/* CFP_PushStream --
 *
 * Synopsis:
 *
 *    Push a stream onto a configuration file control block.
 *
 * Returns:
 *
 * Parameters:
 *
 *    'chcbp' -- Pointer to the control block.
 *    'srcFile' -- The FILE* representing the stream.
 *    'srcName' -- A name to associate with this stream.
 *    'srcLine' -- Current line number in this stream.
 *    'closeOnPop' -- True iff 'srcFile' should be closed when we pop.
 */

void
CFP_PushStream(CFP_Control* cfcbp, FILE* srcFile, const char* srcName, size_t srcLine, int closeOnPop)
{
    CFP_Source* cfp;
    cfp = (CFP_Source*) malloc(sizeof(sizeof(CFP_Source) + strlen(srcName) + 1));
    /* FIXME: Out of memory? */

    cfp->srcFile = srcFile;
    cfp->srcName = ((char*) cfp) + sizeof(CFP_Source);
    strcpy((char*)(cfp->srcName), srcName);
    cfp->srcLine = srcLine;
    cfp->closeOnPop = closeOnPop;
    cfp->stackPtr = cfcbp->stackTop;
    cfcbp->stackTop = cfp;

    return;
}


/* CFP_PushFile --
 *
 * Synopsis:
 *
 *    Push a file onto a configuration file control block.  Open the file.
 *
 * Returns:
 *
 * Parameters:
 *
 *    'cfcbp' -- Pointer to the control block.
 *    'srcName' -- Name of the file source.
 */

void
CFP_PushFile(CFP_Control* cfcbp, const char* srcName)
{
    FILE*	srcFile;

    srcFile = fopen(srcName, "r");
    /* FIXME: Did it exist? */
    CFP_PushStream(cfcbp, srcFile, srcName, 1, 1);
}


/* CFP_Pop --
 *
 * Synopsis:
 *
 *    Pop the current source off the control block, closing it if needed.
 *
 * Returns:
 *
 *    None
 *
 * Parameters:
 *
 *    'cfcbp' - Pointer to the control block.
 */

void
CFP_Pop(CFP_Control* cfcbp)
{
    CFP_Source* cfp = cfcbp->stackTop;
    if (cfp != (CFP_Source*) NULL) {
	if (cfp->closeOnPop) {
	    fclose(cfp->srcFile);
	}
	cfcbp->stackTop = cfp->stackPtr;
	free(cfp);
    }
}


void
CFP_GetToken(CFP_Control* cfcbp, CharAccum* ca)
{
    CFP_Source* cfp;
    FILE*	srcFile;
    int		c;
    cfp_PState	pState;

    pState = cfcbp->pState;
start:
    cfp = cfcbp->stackTop;
    srcFile = cfp->srcFile;

    for (;;) {
    
	c = getc(srcFile);
	switch (pState) {
	case cfp_pSkip:
	    if (c == EOF) {
		CFP_Pop(cfcbp);
		goto start;
	    } else if (c == '\n') {
		cfp->srcLine++;
		continue;
	    } else if (isspace(c)) {
		continue;
	    } else if (c == '"') {
		pState = cfp_pDouble;
		continue;
	    } else if (c == '\'') {
		pState = cfp_pSingle;
		continue;
	    } else if (c == '#') {
		pState = cfp_pSkipComment;
		continue;
	    }
	    CHARACCUM_APPEND_CHAR(ca, c);
	    pState = cfp_pWord;
	    continue;

	case cfp_pSkipComment:
	    if (c == EOF) {
		CFP_Pop(cfcbp);
		pState = cfp_pSkip;
		goto start;
	    } else if (c == '\n') {
		cfp->srcLine++;
		pState = cfp_pSkip;
	    }
	    continue;

	case cfp_pSkipWhite:
	    if (c == '\n') {
		ungetc(c, srcFile);
		cfcbp->pState = cfp_pSkip;
		CHARACCUM_APPEND_CHAR(ca, '\n');
		return;
	    }
	    if (isspace(c)) {
		continue;
	    }
	    if (c == '"') {
		pState = cfp_pDouble;
		continue;
	    }
	    if (c == '\'') {
		pState = cfp_pSingle;
		continue;
	    }
	    if (c == '#') {
		pState = cfp_pSkipComment;
		continue;
	    }
	    CHARACCUM_APPEND_CHAR(ca, c);
	    pState = cfp_pWord;
	    continue;

	case cfp_pWord:
	    if (c == '\n') {
		ungetc(c, srcFile);
		cfcbp->pState = cfp_pSkipWhite;
		return;
	    }
	    if (isspace(c)) {
		cfcbp->pState = cfp_pSkipWhite;
		return;
	    }
	    if (c == '"') {
		pState = cfp_pDouble;
		continue;
	    }
	    if (c == '\'') {
		pState = cfp_pSingle;
		continue;
	    }
	    if (c == '#') {
		cfcbp->pState = cfp_pSkipComment;
		return;
	    }
	    CHARACCUM_APPEND_CHAR(ca, c);
	    continue;

	case cfp_pDouble:
	    if (c == '\n') {
		cfp->srcLine++;
	    }
	    if (c == '"') {
		pState = cfp_pWord;
		continue;
	    }
	    CHARACCUM_APPEND_CHAR(ca, c);
	    continue;

	case cfp_pSingle:
	    if (c == '\n') {
		cfp->srcLine++;
	    }
	    if (c == '\'') {
		pState = cfp_pWord;
		continue;
	    }
	    CHARACCUM_APPEND_CHAR(ca, c);
	    continue;

	}
    }
}


void
CFP_GetCommand(CFP_Control* cfcbp, AV_Control* avc)
{
    CharAccum	token;

    CHARACCUM_INIT(&token);

    for (;;) {
	CFP_GetToken(cfcbp, &token);

	printf("TOKEN: %s\n", CHARACCUM_STRING(&token));

    }
}


int main(){
    CFP_Control	cfcbp;
    AV_Control	avc;

    CFP_Init(&cfcbp);
    AV_Init(&avc);

    CFP_PushFile(&cfcbp, "config-script.sh");

    CFP_GetCommand(&cfcbp, &avc);
}
