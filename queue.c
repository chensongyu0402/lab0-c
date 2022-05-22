#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

/* Notice: sometimes, Cppcheck would find the potential NULL pointer bugs,
 * but some of them cannot occur. You can suppress them by adding the
 * following line.
 *   cppcheck-suppress nullPointer
 */

/*use element_new for insert_head,insert_tail function*/
element_t *element_new(char *s)
{
    element_t *node;
    if (!(node = malloc(sizeof(element_t))))
        return NULL;
    node->value = NULL;

    if (s == NULL)
        return node;

    int s_len = strlen(s) + 1;
    /*char array*/
    char *str;
    /* sizeof(char)=1*/
    if (!(str = malloc(s_len))) {
        free(node);
        return NULL;
    }
    strncpy(str, s, s_len);
    node->value = str;
    return node;
}


/* Create an empty queue */
struct list_head *q_new()
{
    struct list_head *node = malloc(sizeof(struct list_head));
    if (node)
        INIT_LIST_HEAD(node);
    return node;
}

/* Free all storage used by queue */
void q_free(struct list_head *l)
{
    if (!l)
        return;
    if (list_empty(l)) {
        free(l);
        return;
    }
    element_t *entry, *safe;
    list_for_each_entry_safe (entry, safe, l, list) {
        if (entry->value)
            free(entry->value);
        free(entry);
    }
    free(l);
    return;
}

/* Insert an element at head of queue */
bool q_insert_head(struct list_head *head, char *s)
{
    if (!head)
        return false;
    element_t *node = element_new(s);
    if (!node)
        return false;
    list_add(&node->list, head);
    return true;
}

/* Insert an element at tail of queue */
bool q_insert_tail(struct list_head *head, char *s)
{
    if (!head)
        return false;
    element_t *node = element_new(s);
    if (!node)
        return false;
    list_add_tail(&node->list, head);
    return true;
}

/* Remove an element from head of queue */
element_t *q_remove_head(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;
    struct list_head *rm_node = head->next;
    list_del(rm_node);
    element_t *rm_ele = list_entry(rm_node, element_t, list);
    if (!sp || !rm_ele->value)
        return NULL;
    strncpy(sp, rm_ele->value, bufsize);
    sp[bufsize - 1] = '\0';
    return rm_ele;
}

/* Remove an element from tail of queue */
element_t *q_remove_tail(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;
    struct list_head *rm_node = head->prev;
    list_del(rm_node);
    element_t *rm_ele = list_entry(rm_node, element_t, list);
    if (!sp || !(rm_ele->value))
        return NULL;
    strncpy(sp, rm_ele->value, bufsize);
    sp[bufsize - 1] = '\0';
    return rm_ele;
}

/* Return number of elements in queue */
int q_size(struct list_head *head)
{
    if (!head || list_empty(head))
        return 0;
    struct list_head *node;
    unsigned count = 0;
    list_for_each (node, head)
        count++;
    return count;
}

/* Delete the middle node in queue */
bool q_delete_mid(struct list_head *head)
{
    if (!head || list_empty(head))
        return false;
    struct list_head *slow, *fast;
    for (slow = fast = head->next; fast != head && fast->next != head;
         fast = fast->next->next, slow = slow->next)
        ;
    list_del(slow);
    q_release_element(list_entry(slow, element_t, list));
    return true;
}

/* Delete all nodes that have duplicate string */
bool q_delete_dup(struct list_head *head)
{
    if (!head || list_empty(head))
        return false;
    struct list_head *node, *safe;
    bool last_dup = false;
    list_for_each_safe (node, safe, head) {
        element_t *cur = list_entry(node, element_t, list);
        bool match =
            node->next != head &&
            !strcmp(cur->value, list_entry(node->next, element_t, list)->value);
        if (last_dup || match) {
            list_del(node);
            q_release_element(cur);
        }
        last_dup = match;
    }
    return true;
}

/* Swap every two adjacent nodes */
void q_swap(struct list_head *head)
{
    if (!head || list_empty(head))
        return;
    struct list_head *node;
    for (node = head->next; node != head && node->next != head;
         node = node->next)
        list_move_tail(node->next, node);
}

/* Reverse elements in queue */
void q_reverse(struct list_head *head)
{
    if (!head || list_empty(head))
        return;
    struct list_head *node;
    for (node = head->next; node != head && node->next != head;
         node = node->next)
        list_move_tail(node, head);
}

/* Sort elements of queue in ascending order */
void q_sort(struct list_head *head) {}
