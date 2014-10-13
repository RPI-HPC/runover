/* Configuration file parser operations. */

#ifndef CONFIGURATION_FILE_PARSING_H
#define CONFIGURATION_FILE_PARSING_H

#include <stdio.h>
#include <stddef.h>

#include <ca.h>

struct CFP_Control;
struct CFP_Source;

typedef enum  cfp_PState {
    cfp_pSkip,		/* Skip white until a token starts. */
    cfp_pSkipComment,	/* Skip all until end of line, the pSkip */
    cfp_pSkipWhite,	/* Skip white until a token starts, or line ends */
    cfp_pWord,		/* Build up a word. */
    cfp_pDouble,	/* Build up a word, double quoted. */
    cfp_pSingle		/* Build up a word, single quoted. */
} cfp_PState;


typedef struct CFP_Control {
    struct CFP_Source*	stackTop;
    cfp_PState		pState;
} CFP_Control;

typedef struct CFP_Source {
    const char*	srcName;
    size_t	srcLine;
    FILE*	srcFile;
    int		closeOnPop : 1;
    struct CFP_Source*	stackPtr;
} CFP_Source;

#define CFP_Init(cfcbp) \
{ \
    (cfcbp)->stackTop = (CFP_Source*) NULL;	\
    (cfcbp)->pState = cfp_pSkip;		\
}

void
CFP_PushStream(CFP_Control* cfcbp, FILE* srcFile, const char* srcName, size_t srcLine, int closeOnPop);

void
CFP_PushFile(CFP_Control* cfcbp, const char* srcName);

void
CFP_Pop(CFP_Control* cfcbp);


#endif /* !defined CONFIGURATION_FILE_PARSING_H */
