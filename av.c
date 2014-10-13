/* Argument vector operations.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "av.h"

/* avp_add_string --
 *
 * Synopsis:
 *
 *    Add a string to an argument vector builder object.  If NULL is
 *    passed, it will terminate the string.
 */

static void
avp_add_string(AV_Control* avc, const char* s)
{
    size_t		sl, sl1;
    size_t		need;
    const char**	nv;

    /* Space needed for new string (0 if NULL) */
    if (s == (const char*) NULL) {
	sl = 0;
	sl1 = 0;
    } else {
	sl = strlen(s);
	sl1 = sl + 1;
    }


    need = avc->argb + ((1+avc->argc)*sizeof(const char*)) + sl1;
    nv = (const char**) malloc(need);
    if (avc->argc > 0) {

	/* Revised pointers, with a space for the new string pointer. */
	register size_t	i;
	for (i = 0;  i < avc->argc;  ++i) {
	    register ptrdiff_t soff = avc->argv[i] - (char*)(avc->argv);
	    nv[i] = (const char*)nv + soff + sizeof(const char*);
	}

	/* Copy the string data. */
	memcpy((char*)(nv+1+avc->argc),
	       (char*)((avc->argv)+(avc->argc)),
	       avc->argb);

	/* Done with the old block. */
	free(avc->argv);
    }
    if (s == (const char*) NULL) {
	nv[avc->argc] = s;
    } else {
	nv[avc->argc] = ((const char*)(nv+1+avc->argc))+avc->argb;
	memcpy((char*)(nv[avc->argc]), s, sl1);
    }
    
    avc->argv = nv;
    avc->argc++;
    avc->argb += sl1;
}

/* AV_AddString --
 *
 * Synopsis:
 *
 *    Add a string to an argument vector object.
 */

void
AV_AddString(AV_Control* avc, const char* s)
{
    assert(s != (const char*) NULL);
    avp_add_string(avc, s);
}

/* AV_Finalize --
 *
 * Synopsis:
 *
 *    Finish using a control block, and return the argument vector.
 */

const char **
AV_Finalize(AV_Control* avc, size_t* argc)
{
    register const char** av;
    avp_add_string(avc, (const char*) NULL);
    av = avc->argv;
    if (argc != (size_t*) NULL) {
	(*argc) = avc->argc;
    }
    avc->argv = (const char**) NULL;
    avc->argc = avc->argb = 0;
    return av;
}
