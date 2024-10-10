/*
 * Description: Doubly circular linked list 
 */

#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "list.h"

/* ---------- Basic queries ---------- */

int ListLength(List *list)
{
    return list->num_members;
}

int ListEmpty(List *list)
{
    return (list->num_members == 0);
}

/* ---------- Core insert/remove ---------- */

int ListAppend(List *list, void *obj)
{
    ListNode *anchor = &list->anchor;
    ListNode *node = (ListNode*)malloc(sizeof(ListNode));
    if (!node) return 0;

    node->obj = obj;

    if (ListEmpty(list)) {
        /* first element */
        node->next = anchor;
        node->prev = anchor;
        anchor->next = node;
        anchor->prev = node;
    } else {
        /* insert before anchor (at the tail) */
        ListNode *tail = anchor->prev;
        node->next = anchor;
        node->prev = tail;
        tail->next = node;
        anchor->prev = node;
    }
    list->num_members++;
    return 1;
}

int ListPrepend(List *list, void *obj)
{
    ListNode *anchor = &list->anchor;
    ListNode *node = (ListNode*)malloc(sizeof(ListNode));
    if (!node) return 0;

    node->obj = obj;

    if (ListEmpty(list)) {
        node->next = anchor;
        node->prev = anchor;
        anchor->next = node;
        anchor->prev = node;
    } else {
        /* insert after anchor (at the head) */
        ListNode *head = anchor->next;
        node->next = head;
        node->prev = anchor;
        head->prev = node;
        anchor->next = node;
    }
    list->num_members++;
    return 1;
}

void ListUnlink(List *list, ListNode *elem)
{
    if (!elem || ListEmpty(list)) return;

    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
    free(elem);
    list->num_members--;
}

void ListUnlinkAll(List *list)
{
    if (ListEmpty(list)) {
        list->anchor.next = &list->anchor;
        list->anchor.prev = &list->anchor;
        return;
    }

    ListNode *anchor = &list->anchor;
    ListNode *cur = anchor->next;

    while (cur != anchor) {
        ListNode *next = cur->next;
        free(cur);
        cur = next;
    }
    list->num_members = 0;
    anchor->next = anchor->prev = anchor;
}

/* ---------- Positional insert ---------- */

int ListInsertBefore(List *list, void *obj, ListNode *elem)
{
    if (elem == NULL) {
        return ListPrepend(list, obj);
    }

    ListNode *node = (ListNode*)malloc(sizeof(ListNode));
    if (!node) return 0;

    node->obj = obj;

    node->prev = elem->prev;
    node->next = elem;
    elem->prev->next = node;
    elem->prev = node;

    list->num_members++;
    return 1;
}

int ListInsertAfter(List *list, void *obj, ListNode *elem)
{
    if (elem == NULL) {
        return ListAppend(list, obj);
    }

    ListNode *node = (ListNode*)malloc(sizeof(ListNode));
    if (!node) return 0;

    node->obj = obj;

    node->next = elem->next;
    node->prev = elem;
    elem->next->prev = node;
    elem->next = node;

    list->num_members++;
    return 1;
}

/* ---------- Navigation ---------- */

ListNode *ListFirst(List *list)
{
    if (ListEmpty(list)) return NULL;
    return list->anchor.next;
}

ListNode *ListLast(List *list)
{
    if (ListEmpty(list)) return NULL;
    return list->anchor.prev;
}

ListNode *ListNext(List *list, ListNode *elem)
{
    (void)list;
    if (!elem) return NULL;
    return (elem->next == &list->anchor) ? NULL : elem->next;
}

ListNode *ListPrev(List *list, ListNode *elem)
{
    (void)list;
    if (!elem) return NULL;
    return (elem->prev == &list->anchor) ? NULL : elem->prev;
}

/* ---------- Search ---------- */

ListNode *ListFind(List *list, void *obj)
{
    if (ListEmpty(list)) return NULL;

    ListNode *anchor = &list->anchor;
    for (ListNode *cur = anchor->next; cur != anchor; cur = cur->next) {
        if (cur->obj == obj) return cur;
    }
    return NULL;
}

/* ---------- Init ---------- */

int ListInit(List *list)
{
    if (!list) return 0;
    memset(list, 0, sizeof(*list));
    list->anchor.next = &list->anchor;
    list->anchor.prev = &list->anchor;
    /* (optional) leave function pointers unset */
    return 1;
}
