/* Argument vector operations. */

#ifndef ARGUMENT_VECTORS_H
#define ARGUMENT_VECTORS_H

#include <stddef.h>

typedef struct AV_Control {
    const char**	argv;
    size_t		argc;
    size_t		argb;
} AV_Control;

#define	AV_Init(avc) \
{ \
    (avc)->argv = (const char**)NULL; \
    (avc)->argc = (size_t) 0; \
    (avc)->argb = (size_t) 0; \
}

void
AV_AddString(AV_Control* avc, const char*s);

const char **
AV_Finalize(AV_Control* avc, size_t* argc);


#endif /* !defined ARGUMENT_VECTORS_H */
