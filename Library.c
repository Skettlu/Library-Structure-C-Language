#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "Library.h"

// vars
int SLOTS = 0;
library_t LIB = {
    .genres = NULL,
    .books = NULL,
    .members = NULL,
    .globalIndex = NULL,
    .recommendations = NULL,
    .activity_head = NULL
};

// 2H Phase additions
/* AVL functions */

static int max_int(int a, int b) { return (a > b) ? a : b; }

static int node_height(BookNode_t *n)
{
    return n ? n->height : 0;
}

static void update_height(BookNode_t *n)
{
    if (n)
        n->height = 1 + max_int(node_height(n->lc), node_height(n->rc));
}

static BookNode_t *rotate_right(BookNode_t *y)
{
    BookNode_t *x = y->lc;
    BookNode_t *T2 = x->rc;
    x->rc = y;
    y->lc = T2;
    update_height(y);
    update_height(x);
    return x;
}

static BookNode_t *rotate_left(BookNode_t *x)
{
    BookNode_t *y = x->rc; 
    BookNode_t *T2 = y->lc; 
    y->lc = x;
    x->rc = T2;
    update_height(x);
    update_height(y);
    return y;
}

static int bfactor(BookNode_t *n)
{
    if (!n)
        return 0;
    return node_height(n->lc) - node_height(n->rc);
}

/*compare by title lexicographically */
static int title_cmp(const char *a, const char *b)
{
    return strcmp(a, b);
}

void free_avl(BookNode_t *root)
{
    if (!root)
        return;
    free_avl(root->lc);
    free_avl(root->rc);
    free(root);
}

/*insert by title. if title exists, do nothing and return original root. returns new root. on success sets node->book pointer. */
BookNode_t *avl_insert(BookNode_t *root, book_t *book)
{
    if (!root)
    {
        BookNode_t *n = malloc(sizeof(BookNode_t));
        if (!n)
        {
            fprintf(stderr, "malloc failed avl node\n");
            return NULL;
        }
        strncpy(n->title, book->title, TITLE_MAX - 1);
        n->title[TITLE_MAX - 1] = '\0';
        n->book = book;
        n->lc = n->rc = NULL;
        n->height = 1;
        return n;
    }
    
    int cmp = title_cmp(book->title, root->title);
    if (cmp == 0)
    {
        /* duplicate title, do nothing (IGNORED) */
        return root;
    }
    else if (cmp < 0)
    {
        root->lc = avl_insert(root->lc, book);
    }
    else
    {
        root->rc = avl_insert(root->rc, book);
    }
    update_height(root);
    int b = bfactor(root);

    /* rotations */
    if (b > 1 && title_cmp(book->title, root->lc->title) < 0)
        return rotate_right(root);
    if (b < -1 && title_cmp(book->title, root->rc->title) > 0)
        return rotate_left(root);
    if (b > 1 && title_cmp(book->title, root->lc->title) > 0)
    {
        root->lc = rotate_left(root->lc);
        return rotate_right(root);
    }
    if (b < -1 && title_cmp(book->title, root->rc->title) < 0)
    {
        root->rc = rotate_right(root->rc);
        return rotate_left(root);
    }
    return root;
}

/* search by title */
BookNode_t *avl_search(BookNode_t *root, const char *title)
{
    BookNode_t *cur = root;
    while (cur)
    {
        int c = title_cmp(title, cur->title);
        if (c == 0)
            return cur;
        if (c < 0)
            cur = cur->lc;
        else
            cur = cur->rc;
    }
    return NULL;
}

/* Find minimum node in subtree */
static BookNode_t *avl_minval(BookNode_t *node)
{
    BookNode_t *cur = node;
    while (cur->lc)
        cur = cur->lc;
    return cur;
}

/*delete a node by title, returns new root, caller must free removed node. */
BookNode_t *avl_delete(BookNode_t *root, const char *title)
{
    if (!root)
        return NULL;
    int cmp = title_cmp(title, root->title);
    if (cmp < 0)
    {
        root->lc = avl_delete(root->lc, title);
    }
    else if (cmp > 0)
    {
        root->rc = avl_delete(root->rc, title);
    }
    else
    {
        /* found */
        if (!root->lc || !root->rc)
        {
            BookNode_t *tmp = root->lc ? root->lc : root->rc;
            if (!tmp)
            {
                /* no child */
                free(root);
                return NULL;
            }
            else
            {
                /* one child */
                BookNode_t *r = tmp;
                free(root);
                return r;
            }
        }
        else
        {
            /* two children: get inorder successor */
            BookNode_t *succ = avl_minval(root->rc);
            /* copy succ data */
            strncpy(root->title, succ->title, TITLE_MAX - 1);
            root->title[TITLE_MAX - 1] = '\0';
            root->book = succ->book;
            root->rc = avl_delete(root->rc, succ->title);
        }
    }
    update_height(root);
    int b = bfactor(root);
    if (b > 1 && bfactor(root->lc) >= 0)
        return rotate_right(root);
    if (b > 1 && bfactor(root->lc) < 0)
    {
        root->lc = rotate_left(root->lc);
        return rotate_right(root);
    }
    if (b < -1 && bfactor(root->rc) <= 0)
        return rotate_left(root);
    if (b < -1 && bfactor(root->rc) > 0)
    {
        root->rc = rotate_right(root->rc);
        return rotate_left(root);
    }
    return root;
}
/* ----------------------------------------------------- */

/* recheap */

static int rec_cmp(book_t *a, book_t *b) {
    if (a->avg != b->avg) return (a->avg > b->avg) ? 1 : -1;
    /* equal avg: smaller bid has priority */
    if (a->bid != b->bid) return (a->bid < b->bid) ? 1 : -1;
    return 0;
}

RecHeap* recheap_create(void) {
    RecHeap *h = malloc(sizeof(RecHeap));
    if (!h) return NULL;
    h->size = 0;
    for (int i = 0; i < REC_CAP; i++) h->heap[i] = NULL;
    return h;
}

static void rec_swap(RecHeap *h, int i, int j) {
    book_t *tmp = h->heap[i];
    h->heap[i] = h->heap[j];
    h->heap[j] = tmp;
    if (h->heap[i]) h->heap[i]->heap_pos = i;
    if (h->heap[j]) h->heap[j]->heap_pos = j;
}

static void rec_heapify_up(RecHeap *h, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (rec_cmp(h->heap[idx], h->heap[parent]) > 0) {
            rec_swap(h, idx, parent);
            idx = parent;
        } else break;
    }
}

static void rec_heapify_down(RecHeap *h, int idx) {
    int n = h->size;
    while (1) {
        int l = 2*idx + 1;
        int r = 2*idx + 2;
        int best = idx;
        if (l < n && rec_cmp(h->heap[l], h->heap[best]) > 0) best = l;
        if (r < n && rec_cmp(h->heap[r], h->heap[best]) > 0) best = r;
        if (best != idx) {
            rec_swap(h, idx, best);
            idx = best;
        } else break;
    }
}

/*insert book into heap (if capacity allows) or replace worst if better. if book is already in heap, adjust position. */
void recheap_insert_or_update(RecHeap *h, book_t *b) {
    if (!h || !b || b->lost_flag) return;
    if (b->heap_pos >= 0 && b->heap_pos < h->size) {
        /* existing: just bubble up/down */
        rec_heapify_up(h, b->heap_pos);
        rec_heapify_down(h, b->heap_pos);
        return;
    }
    /* not present */
    if (h->size < REC_CAP) {
        int idx = h->size++;
        h->heap[idx] = b;
        b->heap_pos = idx;
        rec_heapify_up(h, idx);
        return;
    }
    /* heap full: find worst (smallest) element in heap: linear scan - REC_CAP small */
    int worst = 0;
    for (int i = 1; i < h->size; i++) {
        if (rec_cmp(h->heap[i], h->heap[worst]) < 0) worst = i;
    }
    if (rec_cmp(b, h->heap[worst]) > 0) {
        /* replace worst */
        h->heap[worst]->heap_pos = -1;
        h->heap[worst] = b;
        b->heap_pos = worst;
        /* fix heap from position worst */
        rec_heapify_up(h, worst);
        rec_heapify_down(h, worst);
    }
}

/*remove a book from heap if present*/
void recheap_remove(RecHeap *h, book_t *b) {
    if (!h || !b || b->heap_pos < 0) return;
    int pos = b->heap_pos;
    int last = h->size - 1;
    if (pos == last) {
        h->heap[pos] = NULL;
        h->size--;
        b->heap_pos = -1;
        return;
    }
    /* replace pos with last */
    h->heap[pos] = h->heap[last];
    h->heap[pos]->heap_pos = pos;
    h->heap[last] = NULL;
    h->size--;
    b->heap_pos = -1;
    /* fix ordering */
    rec_heapify_up(h, pos);
    rec_heapify_down(h, pos);
}

/* get sorted array of pointers (descending priority) - caller frees array */
book_t** recheap_get_sorted(RecHeap *h, int *out_count) {
    if (!h) { *out_count = 0; return NULL; }
    int n = h->size;
    book_t **arr = malloc(sizeof(book_t*) * n);
    for (int i = 0; i < n; i++) arr[i] = h->heap[i];
    /* simple sort using comparator (n <= 64) */
    for (int i = 0; i < n; i++) {
        for (int j = i+1; j < n; j++) {
            if (rec_cmp(arr[j], arr[i]) > 0) {
                book_t *t = arr[i]; arr[i] = arr[j]; arr[j] = t;
            }
        }
    }
    *out_count = n;
    return arr;
}

/*---------------------------------*/


// Functions
/* old: void ReturnLoanAction(char *action, book_t *b, char *score) */
void ReturnLoanAction(member_t *m, char *action, book_t *b, char *score)
{
    if (!m) { printf("IGNORED\n"); return; }

    if (strcmp(action, "lost") == 0)
    {
        b->lost_flag = 1;
        if (LIB.recommendations) recheap_remove(LIB.recommendations, b);

        if (LIB.globalIndex)
            LIB.globalIndex = avl_delete(LIB.globalIndex, b->title);

        printf("DONE\n");
        return;
    }
    else if (strcmp(action, "ok") == 0)
    {
        if (strcmp(score, "NA") != 0)
        {
            int sc = atoi(score);
            if (sc < 0 || sc > 10) { printf("IGNORED\n"); return; }

            int old_avg = b->avg;

            b->sum_scores += sc;
            b->n_reviews += 1;
            b->avg = b->sum_scores / b->n_reviews; /* integer division is OK */

            /* update member activity */
            if (m->activity) {
                m->activity->reviews_count++;
                m->activity->score_sum += sc;
            }

            /* reposition in genre list if avg changed */
            if (b->avg != old_avg)
            {
                /* splice out */
                b->prev_in_genre->next_in_genre = b->next_in_genre;
                b->next_in_genre->prev_in_genre = b->prev_in_genre;

                /* find new position */
                genre_t *g = LIB.genres;
                while (g && g->gid != b->gid) g = g->next_by_gid;
                if (!g) { fprintf(stderr, "Genre not found during repositioning.\n"); return; }

                book_t *cur = g->book_sentinel.next_in_genre;
                while (cur != &g->book_sentinel && cur->avg >= b->avg) cur = cur->next_in_genre;

                /* insert before cur */
                b->next_in_genre = cur;
                b->prev_in_genre = cur->prev_in_genre;
                cur->prev_in_genre->next_in_genre = b;
                cur->prev_in_genre = b;

                if (!LIB.recommendations) LIB.recommendations = recheap_create();
                recheap_insert_or_update(LIB.recommendations, b);
            }

            printf("DONE\n");
        }
        else
        {
            /* score == NA */
            printf("DONE\n");
        }
    }
    else
    {
        printf("IGNORED\n");
    }
}


// parameters settings
void S(int slots) { SLOTS = slots; }

// creation of entities
void G(int gid, const char *name)
{
    genre_t *prev = NULL, *cur = LIB.genres;
    while (cur && cur->gid < gid)
    {
        prev = cur;
        cur = cur->next_by_gid;
    }

    if (cur && cur->gid == gid)
    {
        printf("IGNORED\n");
        return;
    }

    genre_t *new_genre = malloc(sizeof(genre_t));
    if (!new_genre)
    {
        fprintf(stderr, "Memory allocation failed for new genre.\n");
        return;
    }

    new_genre->gid = gid;
    new_genre->points = 0;
    new_genre->display = NULL;
    new_genre->display_count = 0;

    strncpy(new_genre->name, name, NAME_MAX - 1);
    new_genre->name[NAME_MAX - 1] = '\0';
    new_genre->next_by_gid = cur;

    // init sentinel list for books
    new_genre->book_sentinel.next_in_genre = &new_genre->book_sentinel;
    new_genre->book_sentinel.prev_in_genre = &new_genre->book_sentinel;

    if (prev == NULL)
        LIB.genres = new_genre;
    else
        prev->next_by_gid = new_genre;

    printf("DONE\n");
}

void BK(int bid, int gid, const char *title)
{
    // 0) quick sanity: need genre
    genre_t *g = LIB.genres;
    while (g && g->gid != gid)
        g = g->next_by_gid;
    if (!g) { printf("IGNORED\n"); return; }

    // 1) check duplicate bid
    book_t *bcur = LIB.books;
    while (bcur) {
        if (bcur->bid == bid) { printf("IGNORED\n"); return; }
        bcur = bcur->next_global;
    }

    // 2) UNIQUE TITLE CHECK using globalIndex (O(log n))
    if (LIB.globalIndex && avl_search(LIB.globalIndex, title)) {
        printf("IGNORED\n");
        return;
    }

    // 3) allocate and initialize new book
    book_t *new_book = malloc(sizeof(book_t));
    if (!new_book) { fprintf(stderr, "Memory allocation failed for new book\n"); return; }

    new_book->bid = bid;
    new_book->gid = gid;
    new_book->sum_scores = 0;
    new_book->n_reviews = 0;
    new_book->avg = 0;
    new_book->lost_flag = 0;
    new_book->heap_pos = -1;

    strncpy(new_book->title, title, TITLE_MAX - 1);
    new_book->title[TITLE_MAX - 1] = '\0';

    // insert into global book list (head)
    new_book->next_global = LIB.books;
    LIB.books = new_book;

    // insert into genre's circular doubly-linked list (ordered by avg desc, then bid)
    book_t *sent = &g->book_sentinel;
    book_t *cur = sent->next_in_genre;

    while (cur != sent &&
           (cur->avg > new_book->avg ||
            (cur->avg == new_book->avg && cur->bid < new_book->bid)))
    {
        cur = cur->next_in_genre;
    }

    new_book->next_in_genre = cur;
    new_book->prev_in_genre = cur->prev_in_genre;
    cur->prev_in_genre->next_in_genre = new_book;
    cur->prev_in_genre = new_book;

    /* insert to per-genre AVL */
    g->bookIndex = avl_insert(g->bookIndex, new_book);

    /* insert to GLOBAL AVL for O(log n) find / uniqueness */
    LIB.globalIndex = avl_insert(LIB.globalIndex, new_book);

    printf("DONE\n");
}


void M(int sid, const char *name)
{
    member_t *prev = NULL, *cur = LIB.members;
    while (cur && cur->sid < sid)
    {
        prev = cur;
        cur = cur->next_all;
    }

    if (cur && cur->sid == sid)
    {
        printf("IGNORED\n");
        return;
    }

    member_t *new_member = malloc(sizeof(member_t));
    if (!new_member)
    {
        fprintf(stderr, "Memory allocation failed for new member\n");
        return;
    }

    new_member->sid = sid;

    strncpy(new_member->name, name, NAME_MAX - 1);
    new_member->name[NAME_MAX - 1] = '\0';

    // insert sorted
    new_member->next_all = cur;
    if (prev == NULL)
        LIB.members = new_member;
    else
        prev->next_all = new_member;

    new_member->prev_all = prev;
    if (cur)
        cur->prev_all = new_member;

    // init loan list
    new_member->loans_head.next = NULL;

    /*2η Φάση- member activity  */

    MemberActivity *ma = malloc(sizeof(MemberActivity));
    if (!ma)
    {
        fprintf(stderr, "Memory allocation failed for member activity\n");
        return;
    }

    ma->sid = sid;
    ma->loans_count = 0;
    ma->reviews_count = 0;
    ma->score_sum = 0;

    /* link activity list */
    ma->next = LIB.activity_head;
    LIB.activity_head = ma;

    new_member->activity = ma;

    printf("DONE\n");
}

// borrow & return
void L(int sid, int bid)
{

    member_t *m = LIB.members;
    book_t *b = LIB.books;

    // Find the member (sid) and book (bid), if not found IGNORED
    while (m && m->sid != sid)
        m = m->next_all;
    if (!m)
    {
        printf("IGNORED\n");
        return;
    }

    while (b && b->bid != bid)
        b = b->next_global;
    if (!b)
    {
        printf("IGNORED\n");
        return;
    }
    // end find

    // Check if already borrowed (scan member’s unsorted loan list)
    loan_t *cur = m->loans_head.next;
    while (cur)
    {
        if (cur->bid == bid)
        {
            printf("IGNORED\n");
            return;
        }
        cur = cur->next;
    }

    // end check
    loan_t *new_loan = malloc(sizeof(loan_t));
    if (!new_loan)
    {
        fprintf(stderr, "Memory allocation failed for new loan\n");
        return;
    }

    new_loan->sid = sid;
    new_loan->bid = bid;
    // insert at front of member’s loan list
    new_loan->next = m->loans_head.next;
    m->loans_head.next = new_loan;

    /*2η φαση- activity update*/
    if (m->activity)
    m->activity->loans_count++;

    printf("DONE\n");
}

void R(int sid, int bid, char *score, char *status)
{
    member_t *m = LIB.members;
    book_t *b = LIB.books;

    while (m && m->sid != sid) m = m->next_all;
    if (!m) { printf("IGNORED\n"); return; }

    while (b && b->bid != bid) b = b->next_global;
    if (!b) { printf("IGNORED\n"); return; }

    /* Find loan node (but do NOT remove yet) */
    loan_t *prev_loan = &m->loans_head;
    loan_t *cur_loan = m->loans_head.next;
    while (cur_loan && cur_loan->bid != bid) {
        prev_loan = cur_loan;
        cur_loan = cur_loan->next;
    }
    if (!cur_loan) { printf("IGNORED\n"); return; }

    /* Validate score BEFORE removing the loan so invalid scores DON'T remove the loan */
    if (strcmp(status, "ok") == 0 && strcmp(score, "NA") != 0)
    {
        int sc = atoi(score);
        if (sc < 0 || sc > 10) { printf("IGNORED\n"); return; }
    }
    /* status could be "lost" — no score validation required if NA or lost */

    /* Now remove the loan (we validated already) */
    prev_loan->next = cur_loan->next;
    free(cur_loan);

    /* Call ReturnLoanAction passing member pointer so it can update activity */
    ReturnLoanAction(m, status, b, score);
}


// structure display & title selection
void D(void)
{
    if (SLOTS <= 0)
    {
        // zero spots, no display
        // clear_all_displays(); --never got around to implementing this
        printf("DONE\n");
        return;
    }

    // first scan: compute VALID and genre points [points(g)]
    int VALID = 0;
    for (genre_t *g = LIB.genres; g; g = g->next_by_gid)
    {

        // clear previous display
        if (g->display)
        {
            free(g->display);
            g->display = NULL;
            g->display_count = 0;
        }

        int sum = 0;
        // traverse books in genre
        book_t *sent = &g->book_sentinel;
        book_t *b = sent->next_in_genre;

        // correct use of sentinel
        while (b != sent)
        {
            if (b->n_reviews > 0 && b->lost_flag == 0)
            {
                sum += b->sum_scores;
            }
            b = b->next_in_genre;
        }
        // save temporary points
        g->display_count = 0; // προς το παρόν
        g->display = NULL;
        // made-up field for points
        g->points = sum;
        VALID += sum;
    }

    // if VALID == 0 => no valid books in any genre
    if (VALID == 0)
    {
        // clear_all_displays();
        printf("DONE\n");
        return;
    }

    // calculate quota
    int quota = VALID / SLOTS; // floor
    if (quota == 0)
    {
        // clear_all_displays();
        printf("DONE\n");
        return;
    }

    // initial allocation of seats per genre
    int sum_seats = 0;
    int Cgen = 0;
    // count genres
    for (genre_t *g = LIB.genres; g; g = g->next_by_gid)
    {
        Cgen++;
    }

    // arrays for seats, rems, genre pointers
    int *seats = malloc(Cgen * sizeof(int));
    double *rems = malloc(Cgen * sizeof(double));
    genre_t **gs = malloc(Cgen * sizeof(genre_t *));

    int index = 0;
    for (genre_t *g = LIB.genres; g; g = g->next_by_gid)
    {
        gs[index] = g;
        int pts = g->points;
        seats[index] = pts / quota;
        rems[index] = pts - seats[index] * quota;
        sum_seats += seats[index];
        index++;
    }

    int remaining = SLOTS - sum_seats;

    // distribute remaining seats based on largest remainders
    while (remaining > 0)
    {
        // find i with largest rems[i]
        int best_i = -1;
        for (int i = 0; i < Cgen; i++)
        {
            // if seats[i] == 0 and points == 0, skip
            if (gs[i]->points == 0)
                continue;
            if (best_i < 0 || rems[i] > rems[best_i] || (rems[i] == rems[best_i] && gs[i]->gid < gs[best_i]->gid))
            {
                best_i = i;
            }
        }
        if (best_i < 0)
            break;
        seats[best_i]++;
        rems[best_i] = -1; // so it won't be chosen again
        remaining--;
    }

    // 6. Για κάθε κατηγορία, επίλεξε κορυφαία books
    for (int i = 0; i < Cgen; i++)
    {
        genre_t *g = gs[i];
        int k = seats[i];
        if (k <= 0)
        {
            g->display = NULL;
            g->display_count = 0;
            continue;
        }
        // allocate array
        g->display = malloc(sizeof(book_t *) * k);
        g->display_count = 0;
        book_t *sent = &g->book_sentinel;
        book_t *b = sent->next_in_genre;

        // correct use of sentinel
        while (b != sent && g->display_count < k)
        {
            if (b->lost_flag == 0 && b->n_reviews > 0)
            {
                g->display[g->display_count++] = b;
            }
            b = b->next_in_genre;
        }
    }

    // καθάρισε προσωρινές δομές
    free(seats);
    free(rems);
    free(gs);

    printf("DONE\n");
}

// print functions
void PG(int gid)
{
    genre_t *g = LIB.genres;
    while (g && g->gid != gid)
        g = g->next_by_gid;
    if (!g)
    {
        printf("IGNORED\n");
        return;
    }

    // printf("GENRE %d %s\n", g->gid, g->name);
    book_t *sent = &g->book_sentinel;
    book_t *b = sent->next_in_genre;

    // again correct use of sentinel
    while (b != sent)
    {
        printf("%d %d\n", b->bid, b->avg);
        b = b->next_in_genre;
    }
}

void PM(int sid)
{
    member_t *m = LIB.members;
    while (m && m->sid != sid)
        m = m->next_all;
    if (!m)
    {
        printf("IGNORED\n");
        return;
    }

    // printf("MEMBER %d %s\n", m->sid, m->name);
    loan_t *cur = m->loans_head.next;
    printf("Loans:\n");
    while (cur)
    {
        printf("%d\n", cur->bid);
        cur = cur->next;
    }
}

void PD(void)
{
    genre_t *g = LIB.genres;
    printf("Display:\n");
    while (g)
    {
        printf("%d:\n", g->gid);
        for (int i = 0; i < g->display_count; i++)
        {
            book_t *b = g->display[i];
            printf("%d, %d\n", b->bid, b->avg);
        }
        g = g->next_by_gid;
    }
}

void PS(void)
{
    member_t *m = LIB.members;
    printf("Members:\n");
    while (m)
    {
        printf("%d, %s\n", m->sid, m->name);
        m = m->next_all;
    }
}

// 2η ΦΑΣΗ Εντολές

void F(const char *title)
{
    if (!LIB.globalIndex) {
        printf("NOT FOUND\n");
        return;
    }

    BookNode_t *node = avl_search(LIB.globalIndex, title);
    if (!node) {
        printf("NOT FOUND\n");
        return;
    }

    book_t *b = node->book;
    printf("Book %d \"%s\" avg=%d\n", b->bid, b->title, b->avg);
}


void TOP(int k)
{
    if (!LIB.recommendations || LIB.recommendations->size == 0) {
        printf("NO RECOMMENDATIONS\n");
        return;
    }

    int h = LIB.recommendations->size;
    /* if user asks for more than size, we limit*/
    int to_print = (k < h) ? k : h;

    /*make temp array*/
    book_t **arr = malloc(sizeof(book_t*) * h);
    if (!arr) {
        printf("NO RECOMMENDATIONS\n");
        return;
    }
    for (int i = 0; i < h; i++)
        arr[i] = LIB.recommendations->heap[i];

    /* simple build heap: turn array into maxheap */
    for (int i = (h/2) - 1; i >= 0; i--)
        rec_heapify_down(LIB.recommendations, i);

    printf("Top Books:\n");
    for (int i = 0; i < to_print; i++) {
        book_t *b = arr[0];

        /*print */
        printf("%d \"%s\" avg=%d\n", b->bid, b->title, b->avg);

        /* remove root from heap: swap with last & heapify down */
        arr[0] = arr[h - 1];
        h--;
        if (h > 0)
            rec_heapify_down(LIB.recommendations, 0);
    }

    free(arr);
}

/*Active Members*/
void AM()
{
    MemberActivity *ma = LIB.activity_head;
    int found = 0;

    printf("Active Members:\n");

    while (ma)
    {
        int total = ma->loans_count + ma->reviews_count;

        if (total > 0)
        {
            member_t *m = LIB.members;
            while (m && m->sid != ma->sid)
                m = m->next_all;

            if (m)
        
                if (ma->reviews_count != 0 && ma->score_sum != 0)

                printf("%d %s loans=%d reviews=%d\n",
                       m->sid, m->name,
                       ma->loans_count,
                       (ma->score_sum/ma->reviews_count));

                else
                printf("%d %s loans=%d reviews=0\n",
                       m->sid, m->name,
                       ma->loans_count);

            found = 1;
        }

        ma = ma->next;
    }

    if (!found)
        printf("NO ACTIVE MEMBERS\n");
}

void U(int bid, char *new_title)
{
    book_t *b = LIB.books;
    while (b && b->bid != bid)
        b = b->next_global;

    if (!b)
    {
        printf("IGNORED\n");
        return;
    }

    /* uniqueness check on global index */
    if (LIB.globalIndex && avl_search(LIB.globalIndex, new_title))
    {
        printf("IGNORED\n");
        return;
    }

    /* find genre */
    genre_t *g = LIB.genres;
    while (g && g->gid != b->gid)
        g = g->next_by_gid;
    if (!g) { printf("IGNORED\n"); return; }

    /* remove old title from both AVL trees */
    g->bookIndex = avl_delete(g->bookIndex, b->title);
    LIB.globalIndex = avl_delete(LIB.globalIndex, b->title);

    /* update title */
    strncpy(b->title, new_title, TITLE_MAX - 1);
    b->title[TITLE_MAX - 1] = '\0';

    /* reinsert into AVLs */
    g->bookIndex = avl_insert(g->bookIndex, b);
    LIB.globalIndex = avl_insert(LIB.globalIndex, b);

    printf("DONE\n");
}


void X()
{
    int books = 0;
    int members = 0;
    int loans = 0;
    int reviews = 0;
    int score_sum = 0;

    /* Books + ratings */
    book_t *b = LIB.books;
    while (b)
    {
        books++;

        reviews += b->n_reviews;
        score_sum += b->sum_scores;

        b = b->next_global;
    }

    /* Members */
    member_t *m = LIB.members;
    while (m)
    {
        members++;
        m = m->next_all;
    }

    /* Loans από MemberActivities */
    MemberActivity *ma = LIB.activity_head;
    while (ma)
    {
        loans += ma->loans_count;
        ma = ma->next;
    }

    double avg = 0.0;
    if (reviews > 0)
        avg = (double)score_sum / reviews;

    printf("Stats: books=%d members=%d loans=%d avg_rating=%.2f\n",
           books, members, loans, avg);
}

void BF()
{
    /* Free AVL σε κάθε genre */
    genre_t *g = LIB.genres;
    while (g)
    {
        free_avl(g->bookIndex);

        if (g->display)
            free(g->display);

        genre_t *next_g = g->next_by_gid;
        free(g);
        g = next_g;
    }

    /* Free books */
    book_t *b = LIB.books;
    while (b)
    {
        book_t *next = b->next_global;
        free(b);
        b = next;
    }

    /* Free loans + members */
    member_t *m = LIB.members;
    while (m)
    {
        loan_t *l = m->loans_head.next;
        while (l)
        {
            loan_t *next_l = l->next;
            free(l);
            l = next_l;
        }

        member_t *next_m = m->next_all;
        free(m);
        m = next_m;
    }

    /* Free MemberActivity */
    MemberActivity *ma = LIB.activity_head;
    while (ma)
    {
        MemberActivity *next = ma->next;
        free(ma);
        ma = next;
    }

    /* Free RecHeap */
    if (LIB.recommendations)
        free(LIB.recommendations);
        
    /* free global AVL */
    if (LIB.globalIndex) {
        free_avl(LIB.globalIndex);
        LIB.globalIndex = NULL;
    }


    /* Reset LIB */
    LIB.genres = NULL;
    LIB.books = NULL;
    LIB.members = NULL;
    LIB.activity_head = NULL;
    LIB.recommendations = NULL;
    LIB.globalIndex = NULL;

    printf("DONE\n");
}



// Helper functions

// helper parsing: safe trim newline from end of string, line becomes mutable
void trim_newline_all(char *str)
{
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r'))
    {
        str[--len] = '\0';
    }
}

// PLAN:
/* Grabs line and seperates into command and arguments [*cmd, *args]
   *cmd = pointer to first token[or NULL]
   *args = pointer to the rest of the line AKA arguments [or NULL]
   line gets modified (adds '\0' after cmd). */

void getCMD_safe(char *line, char **cmd, char **args)
{
    *cmd = NULL;
    *args = NULL;

    /* skip leading whitespace */
    char *p = line;
    while (*p && isspace((unsigned char)*p))
        p++;
    if (*p == '\0')
        return; // empty line

    /* find end of first token */
    char *q = p;
    while (*q && !isspace((unsigned char)*q))
        q++;

    if (*q)
    {
        // there is args
        *q = '\0'; // terminate command
        q++;
        while (*q && isspace((unsigned char)*q))
            q++; // skip spaces to args
        if (*q != '\0')
            *args = q;
        else
            *args = NULL;
    }
    else
    {
        // whole line is command only
        *args = NULL;
    }

    *cmd = p;
}

// For multiple arguments, breaks them into seperate strings
char **getArguments(const char *args, int *argCount)
{
    char **argArray = NULL;
    int count = 0;
    int i = 0;

    if (!args)
    {
        *argCount = 0;
        return NULL;
    }

    while (args[i])
    {
        /* skip whitespace */
        while (args[i] == ' ' || args[i] == '\t')
            i++;

        if (!args[i])
            break;

        char buffer[512];
        int j = 0;

        /* quoted string */
        if (args[i] == '"')
        {
            i++; /* skip opening " */

            while (args[i] && args[i] != '"')
                buffer[j++] = args[i++];

            if (args[i] == '"')
                i++; /* skip closing " */
        }
        else
        {
            /* normal token */
            while (args[i] && args[i] != ' ' && args[i] != '\t')
                buffer[j++] = args[i++];
        }

        buffer[j] = '\0';

        char **temp = realloc(argArray, sizeof(char *) * (count + 1));
        if (!temp)
        {
            for (int k = 0; k < count; k++)
                free(argArray[k]);
            free(argArray);
            *argCount = 0;
            return NULL;
        }

        argArray = temp;
        argArray[count] = strdup(buffer);

        if (!argArray[count])
        {
            for (int k = 0; k < count; k++)
                free(argArray[k]);
            free(argArray);
            *argCount = 0;
            return NULL;
        }

        count++;
    }

    *argCount = count;
    return argArray;
}


void readFile(FILE *fp)
{
    char line[1024];
    char *cmd = NULL, *args = NULL;
    size_t lineno = 0;

    char **argsArray = NULL;
    int argCount = 0;

    // READS LINE BY LINE
    while (fgets(line, sizeof(line), fp))
    {
        lineno++;

        // removal of \r,\n */
        trim_newline_all(line);

        // ignore empty lines and comments
        char *tmp = line;
        while (*tmp && isspace((unsigned char)*tmp))
            tmp++;
        if (*tmp == '\0')
            continue;
        if (*tmp == '#')
            continue; // coment line

        // parse to cmd + args
        getCMD_safe(line, &cmd, &args);
        if (!cmd)
            continue; // safety check

        // SAFE DEBUG PRINTS
        // printf("Line %zu: Command: [%s]\n", lineno, cmd);s
        // if (args) printf("         Args:    [%s]\n", args);
        // else printf("         Args:    (none)\n");

        // CMD HANDLER
        if (strcmp(cmd, "S") == 0)
        {
            int slots = atoi(args);
            S(slots);
        }
        else if (strcmp(cmd, "G") == 0)
        {
            int gid;
            char name[NAME_MAX];

            argsArray = getArguments(args, &argCount);
            if (argCount < 2)
            {
                fprintf(stderr, "Error on line %zu: Not enough arguments for G command.\n", lineno);
                for (int i = 0; i < argCount; i++)
                    free(argsArray[i]);
                free(argsArray);
                continue;
            }

            gid = atoi(argsArray[0]);
            strncpy(name, argsArray[1], NAME_MAX - 1);
            name[NAME_MAX - 1] = '\0';

            for (int i = 0; i < argCount; i++)
                free(argsArray[i]);
            free(argsArray);

            G(gid, name);
        }
        else if (strcmp(cmd, "BK") == 0)
        {
            // Implement BK command parsing and function call
            int bid;
            int gid;
            char title[TITLE_MAX];

            argsArray = getArguments(args, &argCount);
            if (argCount < 3)
            {
                fprintf(stderr, "Error on line %zu: Not enough arguments for BK command.\n", lineno);
                for (int i = 0; i < argCount; i++)
                    free(argsArray[i]);
                free(argsArray);
                continue;
            }

            bid = atoi(argsArray[0]);
            gid = atoi(argsArray[1]);
            strncpy(title, argsArray[2], TITLE_MAX - 1);
            title[TITLE_MAX - 1] = '\0';

            for (int i = 0; i < argCount; i++)
                free(argsArray[i]);
            free(argsArray);

            BK(bid, gid, title);
        }
        else if (strcmp(cmd, "M") == 0)
        {
            // Implement M command parsing and function call
            int sid;
            char name[NAME_MAX];

            argsArray = getArguments(args, &argCount);
            if (argCount < 2)
            {
                fprintf(stderr, "Error on line %zu: Not enough arguments for M command.\n", lineno);
                for (int i = 0; i < argCount; i++)
                    free(argsArray[i]);
                free(argsArray);
                continue;
            }

            sid = atoi(argsArray[0]);
            strncpy(name, argsArray[1], NAME_MAX - 1);
            name[NAME_MAX - 1] = '\0';

            for (int i = 0; i < argCount; i++)
                free(argsArray[i]);
            free(argsArray);

            M(sid, name);
        }
        else if (strcmp(cmd, "L") == 0)
        {
            int sid;
            int bid;

            argsArray = getArguments(args, &argCount);
            if (argCount < 2)
            {
                fprintf(stderr, "Error on line %zu: Not enough arguments for L command.\n", lineno);
                for (int i = 0; i < argCount; i++)
                    free(argsArray[i]);
                free(argsArray);
                continue;
            }

            sid = atoi(argsArray[0]);
            bid = atoi(argsArray[1]);

            L(sid, bid);

            for (int i = 0; i < argCount; i++)
                free(argsArray[i]);
            free(argsArray);
        }
        else if (strcmp(cmd, "R") == 0)
        {
            int sid;
            int bid;
            char *score;
            char *status;

            argsArray = getArguments(args, &argCount);
            if (argCount < 4)
            {
                fprintf(stderr, "Error on line %zu: Not enough arguments for R command.\n", lineno);
                for (int i = 0; i < argCount; i++)
                    free(argsArray[i]);
                free(argsArray);
                continue;
            }

            sid = atoi(argsArray[0]);
            bid = atoi(argsArray[1]);
            score = argsArray[2];
            status = argsArray[3];

            R(sid, bid, score, status);

            for (int i = 0; i < argCount; i++)
                free(argsArray[i]);
            free(argsArray);
        }
        else if (strcmp(cmd, "D") == 0)
            D();
        else if (strcmp(cmd, "PG") == 0)
        {
            int gid;

            argsArray = getArguments(args, &argCount);
            if (argCount < 1)
            {
                fprintf(stderr, "Error on line %zu: Not enough arguments for PG command.\n", lineno);
                for (int i = 0; i < argCount; i++)
                    free(argsArray[i]);
                free(argsArray);
                continue;
            }

            gid = atoi(argsArray[0]);

            PG(gid);

            for (int i = 0; i < argCount; i++)
                free(argsArray[i]);
            free(argsArray);
        }
        else if (strcmp(cmd, "PM") == 0)
        {
            int sid;

            argsArray = getArguments(args, &argCount);
            if (argCount < 1)
            {
                fprintf(stderr, "Error on line %zu: Not enough arguments for PM command.\n", lineno);
                for (int i = 0; i < argCount; i++) free(argsArray[i]);
                free(argsArray);
                continue;
            }

            sid = atoi(argsArray[0]);

            PM(sid);

            for (int i = 0; i < argCount; i++)
                free(argsArray[i]);
            free(argsArray);
        }
        else if (strcmp(cmd, "PD") == 0)
            PD();
        else if (strcmp(cmd, "PS") == 0)
            ; // PS(); -- WASNT IMPLEMENTED
        else if (strcmp(cmd, "F") == 0)
        {
            char title[TITLE_MAX];
            argsArray = getArguments(args, &argCount);
            if (argCount < 1)
            {
                fprintf(stderr, "Error on line %zu: Not enough arguments for F command.\n", lineno);
                for (int i = 0; i < argCount; i++) free(argsArray[i]);
                free(argsArray);
                continue;
            }
            strncpy(title, argsArray[0], TITLE_MAX - 1);
            title[TITLE_MAX - 1] = '\0';

            /* search all genres' AVL */
            F(title);

            for (int i = 0; i < argCount; i++)
                free(argsArray[i]);
            free(argsArray);
        }
        else if( strcmp(cmd, "TOP") == 0)
        {
            int k;

            argsArray = getArguments(args, &argCount);
            if (argCount < 1)
            {
                fprintf(stderr, "Error on line %zu: Not enough arguments for TOP command.\n", lineno);
                for (int i = 0; i < argCount; i++)
                    free(argsArray[i]);
                free(argsArray);
                continue;
            }

            k = atoi(argsArray[0]);

            TOP(k);

            for (int i = 0; i < argCount; i++)
                free(argsArray[i]);
            free(argsArray);
        }
        else if (strcmp(cmd, "AM") == 0)
            AM();
        else if (strcmp(cmd, "U") == 0)
        {
            int bid;
            char new_title[TITLE_MAX];

            argsArray = getArguments(args, &argCount);
            if (argCount < 2)
            {
                fprintf(stderr, "Error on line %zu: Not enough arguments for U command.\n", lineno);
                for (int i = 0; i < argCount; i++)
                    free(argsArray[i]);
                free(argsArray);
                continue;
            }

            bid = atoi(argsArray[0]);
            strncpy(new_title, argsArray[1], TITLE_MAX - 1);
            new_title[TITLE_MAX - 1] = '\0';

            for (int i = 0; i < argCount; i++)
                free(argsArray[i]);
            free(argsArray);

            U(bid, new_title);
        }
        else if (strcmp(cmd, "X") == 0)
            X();
        else if (strcmp(cmd, "BF") == 0)
            BF();
        else
        {
            fprintf(stderr, "Error on line %zu: Unknown command [%s].\n", lineno, cmd);
        }
    }
}

// Main and file reading

int main(int argc, char **argv)
{

    FILE *f = NULL;
    char filename[1024];

    if (argc >= 2)
    {
        f = fopen(argv[1], "r");
        if (!f)
        {
            perror("Error opening file");
            return EXIT_FAILURE;
        }

        readFile(f);

        if (f != stdin && f)
            fclose(f);
        return EXIT_SUCCESS;
    }
    else
    {
        printf("No input file provided.");
    }
}
