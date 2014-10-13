/* Queued Objects. */

#ifndef QUEUED_OBJECTS_H
#define QUEUED_OBJECTS_H

#include <stdio.h>
#include <assert.h>


/*
 * Queue operations.
 *
 * Objects that use queue must have the appropriate QUEUE_LINKAGE in
 * the object structure.  The qObjType is the type of the object
 * itself.
 *
 * Structures that contain queue control blocks must have an
 * appropriate QUEUE_CONTROL_BLOCK in the structure.  The qObjType is
 * the type of the objects being queued.  Make sure to call
 * QUEUE_CONTROL_BLOCK_INIT when the object is initialized.
 *
 * Normally objects are removed from the QUEUE_HEAD and added to the
 * QUEUE_TAIL.  If there is a need to traverse the set of objects, use
 * QUEUE_NEXT to move from head to tail, and QUEUE_PREV to move from
 * tail to head.
 * 
 *
 * An object can be removed from the chain with QUEUE_REMOVE.  But
 * normally one takes an object from the head, with QUEUE_TAKE (or
 * QUEUE_TAKE_HEAD).
 */

#define QUEUE_LINKAGE(qname, qObjType) \
struct { \
    qObjType	q_prev; \
    qObjType	q_next; \
} qname ## _linkage

#define QUEUE_NEXT(qname, obj) obj->qname ## _linkage.q_next
#define QUEUE_PREV(qname, obj) obj->qname ## _linkage.q_prev

#define QUEUE_CONTROL_BLOCK(qname, qObjType) \
struct { \
    qObjType	q_head; \
    qObjType	q_tail; \
} qname ## _control_block

#define QUEUE_CONTROL_BLOCK_INIT(qname, cb) \
    (cb)->qname ## _control_block.q_head    \
    = (cb)->qname ## _control_block.q_tail  \
	= NULL

#define QUEUE_HEAD(qname, cb) (cb)->qname ## _control_block.q_head
#define QUEUE_TAIL(qname, cb) (cb)->qname ## _control_block.q_tail

#define QUEUE_ADD_TAIL(qname, cb, obj) \
{ \
    (obj)->qname ## _linkage.q_next = NULL;				\
    (obj)->qname ## _linkage.q_prev = (cb)->qname ## _control_block.q_tail; \
    if ((cb)->qname ## _control_block.q_tail == NULL) { \
	assert((cb)->qname ## _control_block.q_head == NULL); \
	(cb)->qname ## _control_block.q_head = (obj);	      \
    } else { \
	(cb)->qname ## _control_block.q_tail->qname ## _linkage.q_next = (obj); \
    } \
    (cb)->qname ## _control_block.q_tail = (obj);	\
}

#define QUEUE_ADD(qname, cb, obj) QUEUE_ADD_TAIL(qname, cb, obj)

#define QUEUE_ADD_HEAD(qname, cb, obj) \
{ \
    (obj)->qname ## _linkage.q_prev = NULL;				\
    (obj)->qname ## _linkage.q_next = (cb)->qname ## _control_block.q_head; \
    if ((cb)->qname ## _control_block.q_head == NULL) { \
	assert((cb)->qname ## _control_block.q_tail == NULL); \
	(cb)->qname ## _control_block.q_tail = (obj);	      \
    } else { \
	(cb)->qname ## _control_block.q_head->qname ## _linkage.q_prev = (obj); \
    } \
    (cb)->qname ## _control_block.q_head = (obj);	\
}

#define QUEUE_TAKE_HEAD(qname, cb, objVar) \
{ \
    if (((objVar) = cb->qname ## _control_block.q_head) != NULL) { \
	if ((cb->qname ## _control_block.q_head = cb->qname ## _control_block.q_head->qname ## _linkage.q_next) == NULL) { \
	    (cb)->qname ## _control_block.q_tail = NULL; \
	} \
    } \
}

#define QUEUE_TAKE(qname, cb, objVar) QUEUE_TAKE_HEAD(qname, cb, objVar)

#define QUEUE_TAKE_TAIL(qname, cb, objVar) \
{ \
    if (((objVar) = cb->qname ## _control_block.q_tail) != NULL) { \
	if ((cb->qname ## _control_block.q_tail = cb->qname ## _control_block.q_tail->qname ## _linkage.q_prev) == NULL) { \
	    (cb)->qname ## _control_block.q_head = NULL; \
	} \
    } \
}


#define QUEUE_REMOVE(qname, cb, obj) \
{ \
    if ((obj)->qname ## _linkage.q_next == NULL) { \
	(cb)->qname ## _control_block.q_tail = (obj)->qname ## _linkage.q_prev; \
    } else { \
	(obj)->qname ## _linkage.q_next->qname ## _linkage.q_prev = (obj)->qname ## _linkage.q_prev; \
    } \
    if ((obj)->qname ## _linkage.q_prev == NULL) { \
	(cb)->qname ## _control_block.q_head = (obj)->qname ## _linkage.q_next; \
    } else { \
	(obj)->qname ## _linkage.q_prev->qname ## _linkage.q_next = (obj)->qname ## _linkage.q_next; \
    } \
}


#endif	/*  !defined QUEUED_OBJECTS_H */
