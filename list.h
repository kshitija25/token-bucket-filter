#ifndef _LIST_H_
#define _LIST_H_

#include "util.h"

typedef struct ListNode {
    void *obj;
    struct ListNode *next;
    struct ListNode *prev;
} ListNode;

typedef struct List {
    int num_members;
    ListNode anchor;

    /* optional function pointers */
    int  (*Length)(struct List *);
    int  (*Empty)(struct List *);

    int  (*Append)(struct List *, void*);
    int  (*Prepend)(struct List *, void*);
    void (*Unlink)(struct List *, ListNode*);
    void (*UnlinkAll)(struct List *);
    int  (*InsertBefore)(struct List *, void*, ListNode*);
    int  (*InsertAfter)(struct List *, void*, ListNode*);

    ListNode *(*First)(struct List *);
    ListNode *(*Last)(struct List *);
    ListNode *(*Next)(struct List *, ListNode *cur);
    ListNode *(*Prev)(struct List *, ListNode *cur);

    ListNode *(*Find)(struct List *, void *obj);
} List;

/* function prototypes */
extern int  ListLength(List*);
extern int  ListEmpty(List*);

extern int  ListAppend(List*, void*);
extern int  ListPrepend(List*, void*);
extern void ListUnlink(List*, ListNode*);
extern void ListUnlinkAll(List*);
extern int  ListInsertAfter(List*, void*, ListNode*);
extern int  ListInsertBefore(List*, void*, ListNode*);

extern ListNode *ListFirst(List*);
extern ListNode *ListLast(List*);
extern ListNode *ListNext(List*, ListNode*);
extern ListNode *ListPrev(List*, ListNode*);

extern ListNode *ListFind(List*, void*);

extern int ListInit(List*);

#endif /* _LIST_H_ */
