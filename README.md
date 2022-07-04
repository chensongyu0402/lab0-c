# Circular Linked List in Linux Kernel 

###### tags: `github_project`
## introduce
- 在 linux kernel 中實作 circular linked list，並利用 valgrind 分析自己的 sort 實作與 linux 中 list_sort 差別
  * cpu time
  * 利用 gnuplot 分析 comparisons 次數，並嘗試改良
    * qsort 與 list_sort 的 k-value 沒有顯著差異
  * 利用 valgrind 討論 cache miss rate
## 開發
### element_t
- 在這裡常用
- code:
```c
typedef struct {
    /* Pointer to array holding string.
     * This array needs to be explicitly allocated and freed
     */
    char *value;
    struct list_head list;
} element_t;
```
### q_new
- list_head 為linux kernel的雙向circular linklist的data structure
- INIT_LIST_HEAD(ptr) 為初始化head node.
- code:
```c
/* Create an empty queue */
struct list_head *q_new()
{
    struct list_head *node = malloc(sizeof(struct list_head));
    if (node)
        INIT_LIST_HEAD(node);
    return node;
}
```
### q_free
- 三種情況都需要考慮
  * NULL
  * one node(所以pointer會指向自己)
    * list_empty 
  * 正常的
- list_empty:
  * tests whether a list is empty
  * 檢查 head 的 next 是否指向自身，確認 list 是否為 empty 狀態。
- list_for_each_entry_safe:
  * 透過額外的 safe 紀錄每個迭代 (iteration) 所操作的節點的下一個節點，因此目前的節點可允許被移走，其他操作則同為不可預期行為。
  * 就算node被free掉，還是可以用safe繼續往下找
- code:
```c
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
```
### q_insert_head
- 先額外用一個function用於配置一個new address space給 element_t，並將value複製為該字串(為獨立配置記憶體空間與前者無關)。
  * 例外狀況
  * 如果配置失敗會回傳NULL
  * 如果字串為空(NULL)則會將value也指向NULL
- 補充：
  * 因為sizeof(char)為1，所以第二個malloc才沒有寫sizeof
- code:
```c
/*use element_new for insert_head,insert_tail function*/
element_t *element_new(char *s)
{
    element_t *node;
    if (!(node = malloc(sizeof(element_t))))
        return NULL;
    node->value = NULL;

    if (s == NULL)
        return node;

    int s_len = strlen(*s) + 1;
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
```
- head不能為空，不然就不能插入值
- node值的輸出也不能為空，因為有可能是建立失敗或是輸入NULL值
- list_add function可以將node插入head
- code:
```c
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
```
### q_insert_tail
- 方法同上，最大差別為"最後是list_add_tail"
- list_add_tail是將node插入到tail
- code:
```c
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
```
### q_remove_head
- 注意 list_del 並不是真的delete node，而是將 node 從其所屬的 linked list 結構中移走。且此時 node 本體甚至是包含 node 的結構所分配的記憶體，在此皆未被釋放，僅僅是將 node 從其原本的 linked list 連接移除。亦即，使用者需自行管理記憶體。
- 對於 sp 的理解
  * 我覺的他只要有值就好，比 rm_ele->value 長即可，因為 remove head 不需要知道值也是可以 remove。
  * sp 可能只是用來存 rm_ele->value 而已。或許在測試當中是有用到的。
- code:
```c
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
```
### q_remove_tail
- 概念與 q_remove_head 類似
- code:
```c
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
```
### q_size
- list_for_each 走訪整個 linked list。注意， node 和 head 不能在迴圈中被更改 (可能在多工環境中出現)，否則行為不可預期。
- code:
```c
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
```
### q_delete_mid
- slow and fast node 類似 mergesort 的 mergesort_list 方法
- fast 往前兩步 ， slow 往前一步，最終 fast 到了終點， slow 剛好會在中間。
- code:
```c
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
```
### q_delete_dup
- 此題為只要有相同就刪去包含原本的那個。
- last_dup 可以避免最後一個重複的 node 忘記刪掉。
  * last_dup = match
- match 判斷當前 node 與 next node 的值是不是一樣。
- code:
```c
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

```
### q_swap
- list_move_tail
  * list_del 可以幫助 再移除 next node 的同時 node 與 下下一個 node接起來。
  * 最後在透過 list_add_trail 達到 swap的效果
- code:
```c
static inline void list_move_tail(struct list_head *node,
                                  struct list_head *head) {
    list_del(node);
    list_add_tail(node, head);
}
```
- code:
```c
void q_swap(struct list_head *head)
{
    if (!head || list_empty(head))
        return;
    struct list_head *node;
    for (node = head->next; node != head && node->next != head;
         node = node->next)
        list_move_tail(node->next, node);
}
```
### q_reverse
- list_move 可以把 node 搬移到 head 前
- code:
```c
/* Reverse elements in queue */
void q_reverse(struct list_head *head)
{
    if (!head || list_empty(head))
        return;
    struct list_head *node, *safe;
    list_for_each_safe (node, safe, head)
        list_move(node, head);
}
```
### q_sort
- INIT_LIST_HEAD
  * INIT_LIST_HEAD 把 prev 和 next 指向自身，變成 head
- Each node itself is a sorted list
  * 為了要讓每個點都能成為一個 link list 但又不想要讓他們不連接彼此，所以將他們的 prev points to itself.
- Pointer first points to first sorted list
  * 以便可以從頭開始 merge
- last,next_list,next_next_list
  * 當前 list ,下一個 list ,下下一個 list.
- Merge two list
  * 會先將兩組 list 分別獨立出來，再做 list
- Next to last,next_list,next_next_list
  * 除了可以繼續往下找以外
  * 還負責讓原本斷掉的 list 補回來，例如：最後的 head
- code:
```c
/* Sort elements of queue in ascending order */
void q_sort(struct list_head *head)
{
    if (!head || list_empty(head) || list_is_singular(head))
        return;        
    
    /* Each node itself is a sorted list */
    struct list_head *cur, *safe;
    list_for_each_safe (cur, safe, head)
        cur->prev = cur;
    
    /* Pointer first points to first sorted list */
    struct list_head *first = head->next;
    INIT_LIST_HEAD(head);
    while (first->prev->next != head) {
        struct list_head **last = &first;
        struct list_head *next_list = (*last)->prev->next;
        struct list_head *next_next_list = next_list->prev->next;
        
        while ((*last) != head && next_list != head){
            /* Merge two list */
            (*last)->prev->next = (*last);
            next_list->prev->next = next_list;
            (*last) = merge((*last), next_list);

            /* Next to last,next_list,next_next_list*/
            last = &((*last)->prev->next);
            *last = next_next_list;
            next_list = (*last)->prev->next;
            next_next_list = next_list->prev->next;
        }
    }
    list_add_tail(head, first);
}
```
- list_del_init
  * 以 list_del 為基礎，但移除的 node 會額外呼叫 INIT_LIST_HEAD 把 prev 和 next 指向自身
  * 簡單來說，就是移除該點並初始化，並變為另一個 link list 的 head
- cmp
  * 用來取得比較值，主要是要找比較小的值
  * <=0 是為了保持排序的穩定性
- chosen(相當重要)
  * 會根據 cmp 選擇 point to left or right
  * 也是專門移動的關鍵 pointer (可以讓 comparasion 持續下去)
- remain
  * 會保留剩下的 link list
  * tail 會 point to head->prev 為 new link list 中最大的那個。
- 最後底下4步驟
  * 將 new_list 與 remain_list 串街起來
- code:
```c
/* Merge Alogorithm */
struct list_head *merge(struct list_head *left, struct list_head *right)
{
    struct list_head *head;
    
    int cmp = strcmp(list_entry(left, element_t, list)->value,
                     list_entry(right, element_t, list)->value);
    struct list_head **chosen = 
        cmp <= 0 ? &left : &right;
    head = *chosen;
    (*chosen) = (*chosen)->next;
    list_del_init(head);

    while (left->next != head || right->next != head) {
        cmp = strcmp(list_entry(left, element_t, list)->value,
                     list_entry(right, element_t, list)->value);
        chosen = cmp <=0 ? &left : &right;
        list_move_tail((*chosen = (*chosen)->next)->prev, head);
    }
    struct list_head *remain = left->next != head ? left : right;
    struct list_head *tail = head->prev;
    
    head->prev = remain->prev;
    head->prev->next = head;
    remain->prev = tail;
    remain->prev->next = remain;

    return head;
}
```
### q_shuffle
- 主要目的是希望可以讓 queue 隨機
- srand(time(NULL))
  * 會隨著時間產生不同的隨機值
- rnd = dir ? n - i - rnd : rnd
  * 主要是讓 rnd 值大於0
  * 如果剛好是0，不就根本隨機了嘛
```c
int rnd = rand() % (n - i);
int dir = rnd > (n - i) / 2 ? 0 : 1;
rnd = dir ? n - i - rnd : rnd;
```
- rnd 的用意就是讓第 rnd 個 移動
- coed:
```c
/* Random queue */
void q_shuffle(struct list_head *head)
{
    if (!head || list_empty(head))
        return;

    srand(time(NULL));
    int n = q_size(head);

    struct list_head *first = head->next;
    list_del_init(head);

    for (int i = 0; i < n - 1; i++) {
        int rnd = rand() % (n - i);
        int dir = rnd > (n - i) / 2 ? 0 : 1;
        rnd = dir ? n - i - rnd : rnd;
        for (int j = 0; j < rnd; j++) {
            first = dir ? first->prev : first->next;
        }
        list_move(first->next, head);
    }
    list_move(first, head);
}
```
## 研讀 lib/list_sort.c
:::info
1. 分析 lib/list_sort.c
2. 比較 list_sort.c 與 qsort 效能
3. 最終，找到改進方案
:::
- [source code](https://github.com/torvalds/linux/blob/master/lib/list_sort.c)
### lib/list_sort.c 圖解
- 簡單了解 list_sort 運作

|count decimal|count binary|merge|before merge|after merge
| -------- | -------- | -------- |---|---|
0|0000 	|No|	NULL|	X
1|0001 	|No|	1	|X
2|0010 	|Yes|	1-1	|2
3|0011 	|No|	1-2	|X
4|0100 	|Yes|	1-1-2	|2-2
5|0101 	|Yes|	1-2-2	|1-4
6|0110 	|Yes|	1-1-4	|2-4
7|0111 	|No|	1-2-4	|X
8|1000 	|Yes|	1-1-2-4	|2-2-4
9|1001 	|Yes|	1-2-2-4	|1-4-4
10|1010 	|Yes	| 1-1-4-4|	2-4-4
11|1011 	|Yes	| 1-2-4-4|	1-2-8
12|1100 	|Yes| 	1-1-2-8|	2-2-8
13|1101 	|Yes	| 1-2-2-8	|1-4-8
14|1110|Yes	| 1-1-4-8	|2-4-8
15|1111 	|No	| 1-2-4-8	|X
16|10000 	|Yes| 	1-1-2-4-8|	2-2-4-8

- 圖解了解
  * ![](https://i.imgur.com/fGhBLxh.png)
  * 解說 0b0000：
    * 初始階段，list 會指向 第一個 node
    * count 用意為紀錄 pending(sorted list) 的 node 數
    * 所以，此時 count = 0 且 pending 與 tail 都 pointer to NULL
    * 結束階段，因為 pending 為 NULL ，所以 list->prev = NULL
    * pending 又來到 list 的目前位置
    * list 再往前一個
    * 讓 pending->next = NULL 用意主要是讓pending 獨立出來。
    * 結束階段的實作主要是要希望往前一個點，並讓一個點加入排序內
  * 解說 0b0001：
    * tail point to  pending
    * count = 1 ，tail = &(*tail)->prev
    * 所以， tail point to 5->prev 即 NULL
  * ![](https://i.imgur.com/TiM0J8I.png)
  * 解說 0b0010：
    * 合併前：
    * 因為 count = 0b0010 ，所以 tail 不動
    * 但，可以 merge
    * 所以， a point to the address which tail point to 且 b point to a->prev
    * 這裡 b 的用意就是找另一個已經 sorted list 與自己實作的 qsort 想法類似(在b以前可能還有其他sorted list)
    * 合併後：
    * a point to sorted list 的 head 處
    * b 此時仍然 point to 原本的 sorted list 位置
    * a->prev = b->prev 主要用意是讓 final sorted list(a) 連接到 另一個還未合併的 sorted list(b->prev)
    * 最後：
    * list->prev = pending 讓 list prev 之後可以直接連到 sorted list 的頭
  * 解說 0b0011：
    *  因為 count = 0b0011，所以 tail 會 point to 4->prev(NULL)
    *  又因為最後 bit 為0，所以不做
  * ![](https://i.imgur.com/MI1Rp03.png)
  * 解說 0b0100：
    * 其實主要與0b0010類似，所以先不贅述
  * ![](https://i.imgur.com/bu0DOSY.png)
  * 解說 0b0101：
    * 合併前：
    * count = 0b0101，所以 tail 會 point to 7->prev(2)
    * a 與 b 分別 point to tail 與 tail->prev 兩個 sorted list
    * 因為，bits 最後不為0，所以合併
    * 後面又與之前一樣，所以不再贅述
- 結論：
  * 第一階段： 將節點一個一個讀取，如果確定合併的大小不會超過整體的特定比例，就將之前的串列合併。因為本質還是 bottom up merge sort ，所以此階段的合併都是同大小，所有子串列也都會是二的冪。
  * 第二階段： 當所有節點都讀進並盡可能的同大小合併後，就來到必須合併不同大小的階段。這個階段會將子串列由小大到一個接一個合併。
    * 並且合併比例限制在2:1，避免合併過大的sorted list，導致增加太多比較次數。 
    * depth-first order ，目的是為了避免 thrashing
    * 如果採用 depth-first order，就可以趁相近的子串列都還在 L1 cache 時，盡量將大小相同的兩者合併，進而打造出 cache friendly 的實作方法。
  * 上面的例子都是在講述第一階段
### lib/list_sort.c trace code
- tail 是專門串接 a 與 b 的 node
- code:
```c
__attribute__((nonnull(2,3,4)))
static struct list_head *merge(void *priv, list_cmp_func_t cmp,
				struct list_head *a, struct list_head *b)
{
	struct list_head *head, **tail = &head;

	for (;;) {
		/* if equal, take 'a' -- important for sort stability */
		if (cmp(priv, a, b) <= 0) {
			*tail = a;
			tail = &a->next;
			a = a->next;
			if (!a) {
				*tail = b;
				break;
			}
		} else {
			*tail = b;
			tail = &b->next;
			b = b->next;
			if (!b) {
				*tail = a;
				break;
			}
		}
	}
	return head;
}
```
- likely 用以提高 cache hit
  * code：
  ```c 
  #define likely(x)	__builtin_expect(!!(x), 1)
  ```
  * __builtin_expect: 將 branch 的相關資訊提供給 compiler，告訴 compiler 設計者期望的比較結果，以便對程式碼改變分支順序來進行優化。
  * !!(x): 透過兩次 NOT op來確保值一定是 0 或 1，因為 if 內有可能是 0 或是 其他整數，透過兩次 NOT 來排除這種不確定性。
- 第一次 merge:
  * pending 為 sorted list
  * count 是用來計算 pending 的數量
  * 透過 bits 來控制 該對哪兩個 sorted list 做 merge
  * merge 完會讓sorted list 內部以 next link 連接
  * a->prev = b->prev 可以讓 merge完的 sorted list 再去連接其他 sorted list，且此時sorted list 內部並沒有完全透過 prev 連接彼此
  * 在整個 sorted list 的連接過程，其實與 qsort 內部 sorted list 的連接方法是一樣的，且這個方法也比 qsort 減少許多不必要的 link 過程
  * 這裡的 merge 主要是希望讓 sorted list 可以保持2的冪次
- 第二次 merge:
  * 為了要 depth-first order(new data 優先 merge)，可以想像一下玩2048
  * 從最後面處理 merge 的開始做第二次 merge ，這裡的 merge 主要是把整個sorted list 做完整的合併且合併的過程限制於2:1的比例，以避免不必要的comparasion。
  * 值得一題的是，最後一次不會做 merge ，是要避免到時候無法把 prev 完原回去。
- 這些內容要配合上面結論一起看
- code:
```c
__attribute__((nonnull(2,3)))
void list_sort(void *priv, struct list_head *head, list_cmp_func_t cmp)
{
	struct list_head *list = head->next, *pending = NULL;
	size_t count = 0;	/* Count of pending */

	if (list == head->prev)	/* Zero or one elements */
		return;

	/* Convert to a null-terminated singly-linked list. */
	head->prev->next = NULL;

	do {
		size_t bits;
		struct list_head **tail = &pending;

		/* Find the least-significant clear bit in count */
		for (bits = count; bits & 1; bits >>= 1)
			tail = &(*tail)->prev;
		/* Do the indicated merge */
		if (likely(bits)) {
			struct list_head *a = *tail, *b = a->prev;

			a = merge(priv, cmp, b, a);
			/* Install the merged result in place of the inputs */
			a->prev = b->prev;
			*tail = a;
		}

		/* Move one element from input list to pending */
		list->prev = pending;
		pending = list;
		list = list->next;
		pending->next = NULL;
		count++;
	} while (list);

	/* End of input; merge together all the pending lists. */
	list = pending;
	pending = pending->prev;
	for (;;) {
		struct list_head *next = pending->prev;

		if (!next)
			break;
		list = merge(priv, cmp, pending, list);
		pending = next;
	}
	/* The final merge, rebuilding prev links */
	merge_final(priv, cmp, head, pending, list);
}
EXPORT_SYMBOL(list_sort);
```
- 最後一次 merge:
  * 這裡除了是把最後兩個 sorted list 合併外，還要讓 sorted list 內部具有 prev link，這樣才是一個 circular link list
  * 其中，值得注意的一點， if (!++count)cmp(priv, b, b);
    * 因為這個較長的 list 會不斷的重複還原 prev link
    * 為了避免這個程式長期佔用 cpu ，所以多了此機制
    * 當 count overflow 會呼叫 cmp()
    * 在 cmp 中呼叫 cond_resched，來讓其他 process 有機會可以搶佔，這是讓使用者彈性
- code:
```c
__attribute__((nonnull(2,3,4,5)))
static void merge_final(void *priv, list_cmp_func_t cmp, struct list_head *head,
			struct list_head *a, struct list_head *b)
{
	struct list_head *tail = head;
	u8 count = 0;

	for (;;) {
		/* if equal, take 'a' -- important for sort stability */
		if (cmp(priv, a, b) <= 0) {
			tail->next = a;
			a->prev = tail;
			tail = a;
			a = a->next;
			if (!a)
				break;
		} else {
			tail->next = b;
			b->prev = tail;
			tail = b;
			b = b->next;
			if (!b) {
				b = a;
				break;
			}
		}
	}

	/* Finish linking remainder of list b on to tail */
	tail->next = b;
	do {
		if (unlikely(!++count))
			cmp(priv, b, b);
		b->prev = tail;
		tail = b;
		b = b->next;
	} while (b);

	/* And the final links to make a circular doubly-linked list */
	tail->next = head;
	head->prev = tail;
}
```
### 效能分析
- cpu time
  * 根據 Welch’s t-test
  * 比較 qsort 與 list_sort 在 隨機 270000個 node 下
  * qsort：0.562 second
  * list_sort：0.2776 second
  * 最終 p-valuev 小於 0.05，顯然有明顯差異
- comparison 次數
  * 在不同的 merge sort 實作，number of comparison 可以用以下通式
    * $C = n*\log_{2}(n) - K*n + O(1)$
      * C 為 number of comparison
      * n 為 data size
      * K 為一個週期函數
    * 其中，K 最倍受關注，因為當 N 是固定的時候， K 將大大影響比較次數
    * 所以，許多人會將這個 K 當作他們實做的評估
    * 最後，這裡也可以反推 K 之公式得以下式子：
    * $K = log_{2}(n) - C/n$
  * 為了避免 K 值受週期影響有大有小，所以在計算 K 值：
    * 從一個整數變數到另一個整數變數，產生的值取平均
    * 又因為$log_{2}(n)$的關係，整數以2的冪次為主
  * code 可以到 qtest 中的 average_K 查看
  * 實驗：
    * size = 512 --- 1024-1
    * ![](https://i.imgur.com/0kD9ALz.png)
    * size = 1024 --- 2048-1
    * ![](https://i.imgur.com/0gGbc7C.png)
  * 討論：
    * list_sort 的 K 值穩定
    * qsort 的 K 值振幅非常大
    * list_sort 將每次的合併強制限制比例為2:1，因此沒有過多的比較
    * 但 qsort 完全沒有考慮的這種情形
  * 改進：
    * paper:[Queue-mergesort](https://www.sciencedirect.com/science/article/pii/002001909390088Q?via%3Dihub)
    * there are an odd number of lists - we do not leave the last list by itself but merge it with the new list that was just created when the second and third to last lists on that pass were merged.
    * 可以嘗試使用 size=13 trace code ，可以發現最大的差別在於"最後一次合併且 sorted list 正好 odd 個數"，如果不用改進的方法，在合併的過程中將會產生過多的比較
    * 實驗結果：
      * ![](https://i.imgur.com/lKX8ESm.png)
 * cache miss:
   * 利用 valgrind cachegrind 分析各個函式 cache miss
   * 主要觀察 LL(last-level) cache ，因為他是最後一個 cache
   * 實驗結果：
     * ![](https://i.imgur.com/FzBd57e.png)
   * 討論：
     * strcmp 很高：因為我們的 element_new 是採用2個 malloc 但這樣就變成是2個彼此不連續的位址，所以如過改慘用1個或許會比較好
     * merge and sort 都是 list_sort 比較好：因為 list_sort 採深度優先，因此可以達到比較好cache hit
     * 因此，將 element_new 得實作方式改寫
   * 實驗結果:
     * ![](https://i.imgur.com/bDVZpir.png)






 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
   




