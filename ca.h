/* Character Accumulators */

#ifndef CHARACTER_ACCUMULATOR_H
#define CHARACTER_ACCUMULATOR_H

#include <stdio.h>
#include <assert.h>


/* Character accumulator.
 */

typedef struct CharAccum {
    char*	cb;
    size_t	cbLen;
    size_t	cbMax;
} CharAccum;


#define CHARACCUM_INIT(ca) \
{ \
    (ca)->cb = NULL; \
    (ca)->cbLen = 0; \
    (ca)->cbMax = 0; \
}

#define CHARACCUM_APPEND_CHAR(ca, c) \
{ \
    if ((ca)->cbLen+1 >= (ca)->cbMax) { \
	if ((ca)->cbMax == 0) { \
	    assert((ca)->cb == NULL); \
	    (ca)->cbMax = 31; \
	    (ca)->cb = (char*) malloc(32); \
	} else { \
	    (ca)->cbMax *= 2; \
	    (ca)->cb = (char*) realloc((ca)->cb, (ca)->cbMax+1); \
	} \
    } \
    (ca)->cb[(ca)->cbLen++] = (c); \
    (ca)->cb[(ca)->cbLen] = '\0'; \
}

#define CHARACCUM_APPEND_STR(ca, s) \
{ \
    register char* _sp; \
    for (_sp = (char*) (s);  *_sp;  ++_sp) { \
	CHARACCUM_APPEND_CHAR(ca, *_sp); \
    } \
}

#define CHARACCUM_FINALIZE(ca) \
{ \
    CHARACCUM_APPEND_CHAR(ca, '\0'); \
    return (ca)->cb; \
}

#define CHARACCUM_CLEAR(ca) \
{ \
    if ((ca)->cb != (char*) NULL) { \
	(ca)->cb[0] = '\0'; \
    } \
    (ca)->cbLen = 0; \
}

#define CHARACCUM_LENGTH(ca) \
    ((ca)->cbLen)

#define CHARACCUM_STRING(ca) \
    ( ((ca)->cbMax) ? ((ca)->cb) : "")

#endif /* !defined CHARACTER_ACCUMULATOR_H */
