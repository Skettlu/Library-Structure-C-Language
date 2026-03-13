/* =========================
   Κοινά & βοηθητικά
   ========================= */
#ifndef LIB_DS_H
#define LIB_DS_H

#include <stddef.h>

#define TITLE_MAX 128
#define NAME_MAX  64

#define REC_CAP 64

/* Παγκόσμια θέση προβολών (ορίζεται από την εντολή S <slots>) */
extern int SLOTS;

/* -----------------------------------------
   LOAN: ενεργός δανεισμός (unsorted, O(1) insert/remove)
   Λίστα ανά Member με sentinel head.
   ----------------------------------------- */
typedef struct loan {
    int sid;            /* member id (ιδιοκτήτης της λίστας) */
    int bid;            /* book id που έχει δανειστεί */
    struct loan *next;  /* επόμενος δανεισμός του μέλους */
} loan_t;

/* -----------------------------------------
   BOOK: βιβλίο
   - Ανήκει σε ακριβώς ένα Genre (gid)
   - Συμμετέχει στη διπλά συνδεδεμένη λίστα του Genre,
     ταξινομημένη φθίνοντα κατά avg.
   ----------------------------------------- */
typedef struct book {
    int  bid;                         /* book id (μοναδικό) */
    int  gid;                         /* genre id (ιδιοκτησία λίστας) */
    char title[TITLE_MAX];

    /* Στατιστικά δημοτικότητας */
    int sum_scores;                   /* άθροισμα έγκυρων βαθμολογιών */
    int n_reviews;                    /* πλήθος έγκυρων βαθμολογιών */
    int avg;                          /* cache: floor(sum_scores / n_reviews); 0 αν n_reviews=0 */
    int lost_flag;                    /* 1 αν δηλωμένο lost, αλλιώς 0 */

    /* Διπλά συνδεδεμένη λίστα του genre, ταξινομημένη κατά avg (desc).
       — ΕΙΝΑΙ κυκλική με sentinel: δείκτες head <-> head στο genre. */
    struct book *prev_in_genre;
    struct book *next_in_genre;

    /* Προαιρετικό: συνδέσεις σε global ευρετήρια αν κρατήσετε (όχι απαραίτητο) */
    struct book *next_global;         /* π.χ. unsorted λίστα όλων των βιβλίων */

     /* ΝΕΟ για φάση 2 */
    int heap_pos;                /* θέση μέσα στο RecHeap, -1 αν δεν υπάρχει */
} book_t;


/* -----------------------------------------
   MEMBER: μέλος βιβλιοθήκης
   - Κρατά unsorted λίστα ενεργών δανεισμών (loan_t) με sentinel
   ----------------------------------------- */
typedef struct member {
    int  sid;                         /* member id (μοναδικό) */
    char name[NAME_MAX];

    /* Λίστα ενεργών δανεισμών:
       Uns. singly-linked με sentinel (dummy head node):
       - Εισαγωγή: O(1) push-front
       - Διαγραφή γνωστού bid: O(1) αν κρατάτε prev pointer στη σάρωση */
    loan_t loans_head;                /* sentinel κόμβος: loans_head.next -> πρώτο loan ή NULL */

    /* Διπλά συνδεδεμένη λίστα όλων των μελών (προαιρετικό global index) */
    struct member *prev_all;
    struct member *next_all;

    /* ΝΕΟ για φάση 2 */
    struct MemberActivity *activity;  /* δείκτης στον κόμβο δραστηριότητας */
} member_t;


/* =========================
   ΦΑΣΗ 2η
   ========================= */

   typedef struct BookNode{

      char title[TITLE_MAX];
      book_t *book;
      struct BookNode *lc;
      struct BookNode *rc;
      int height;

   } BookNode_t;

   typedef struct RecHeap {
      book_t *heap[REC_CAP];
      int size;
   } RecHeap;

   typedef struct MemberActivity {
      int sid;
      int loans_count;
      int reviews_count;
      int score_sum;
      struct MemberActivity *next;
   } MemberActivity;

   /*-------------------*/

/* -----------------------------------------
   GENRE: κατηγορία βιβλίων
   - Κρατά ΔΙΠΛΑ συνδεδεμένη λίστα ΒΙΒΛΙΩΝ ταξινομημένη κατά avg (desc)
   - Η λίστα είναι ΚΥΚΛΙΚΗ με SENTINEL (book_sentinel)
   - Κρατά και το αποτέλεσμα της τελευταίας D (display) για εκτύπωση PD
   ----------------------------------------- */
typedef struct genre {
    int  gid;                         /* genre id (μοναδικό) */
    char name[NAME_MAX];
    int points;                       /* προσωρινό πεδίο για D: συνολικοί βαθμοί βιβλίων */

    /* Κυκλική διπλά συνδεδεμένη λίστα βιβλίων ταξινομημένη κατά avg φθίνουσα.
       Sentinel: book_sentinel.prev_in_genre & next_in_genre δείχνουν στον εαυτό τους
       όταν η λίστα είναι άδεια. */
    book_t book_sentinel;

    /* Αποτέλεσμα τελευταίας κατανομής D: επιλεγμένα βιβλία για προβολή.
       Αποθηκεύουμε απλώς pointers στα book_t (δεν αντιγράφουμε δεδομένα). */
    int     display_count;            /* πόσα επιλέχθηκαν σε αυτό το genre */
    book_t **display;                 /* δυναμικός πίνακας με pointers στα επιλεγμένα */

    /* Μονοσυνδεδεμένη λίστα όλων των genres ταξινομημένη κατά gid (για εύκολη σάρωση). */
    struct genre *next_by_gid;

    /* ΝΕΟ για φάση 2 */
    BookNode_t *bookIndex;  /* ρίζα AVL για τα βιβλία αυτής της κατηγορίας */
} genre_t;

/* -----------------------------------------
   LIBRARY: κεντρικός "ρίζας"
   - Κρατά λίστα Genres (sorted by gid) και προαιρετικά ευρετήρια
   ----------------------------------------- */
typedef struct library {
    genre_t  *genres;     /* κεφαλή λίστας genres (sorted by gid) */
    member_t *members;    /* διπλά συνδεδεμένη λίστα μελών (sorted by sid) — προαιρετικό */
    book_t   *books;      /* unsorted λίστα όλων των books (ευκολία αναζήτησης) — προαιρετικό */

    /* ΝΕΑ για φάση 2 */
    RecHeap *recommendations;        /* pointer στον RecommendationHeap */
    MemberActivity *activity_head;   /* μη ταξινομημένη λίστα δραστηριότητας (head) */

    BookNode_t *globalIndex;   // νέο global AVL index για O(log n) αναζήτηση [δεν το αναφερεί η εκφώνηση]
} library_t;

extern library_t LIB;

/* =========================
   ΒΟΗΘΗΤΙΚΕΣ ΣΥΜΒΑΣΕΙΣ & INVARIANTS
   =========================

   GENRE.book_sentinel:
   - Είναι ΠΑΝΤΑ έγκυρος κόμβος-φρουρός (δεν αντιστοιχεί σε πραγματικό βιβλίο).
   - Αρχικοποίηση:
       g->book_sentinel.prev_in_genre = &g->book_sentinel;
       g->book_sentinel.next_in_genre = &g->book_sentinel;
       g->book_sentinel.gid = g->gid; // για ομοιομορφία
   - Κενή λίστα: sentinel.next == sentinel.prev == &sentinel.

   ΕΙΣΑΓΩΓΗ βιβλίου σε genre (μετά από αλλαγή avg):
   - Επανατοποθέτηση με τοπικές αλλαγές δεικτών (O(hops)):
     1) Αφαιρείς το βιβλίο από την τωρινή θέση (splice-out).
     2) Σαρώνεις προς τα πάνω/κάτω μέχρι να βρεις τοπική θέση ως προς avg.
     3) Εισάγεις (splice-in) πριν από τον πρώτο κόμβο με avg < δικό μου.

   MEMBER.loans_head:
   - Είναι sentinel με sid=member.sid, bid=-1.
   - Αρχικοποίηση: loans_head.next = NULL.
   - Εισαγωγή: push-front (new->next = head.next; head.next = new).

   DISPLAY (αποτέλεσμα D):
   - Πριν τρέξει νέα D, απελευθερώνεις την προηγούμενη μνήμη (free genre->display),
     και θέτεις display=NULL, display_count=0 σε όλα τα genres.
   - Η D ΓΕΜΙΖΕΙ τον πίνακα display ανά genre με έως seats(g) pointers.

   ΠΟΛΥΠΛΟΚΟΤΗΤΕΣ:
   - Insert loan: O(1)
   - Return book με έγκυρο score: ενημέρωση sum/n/avg (O(1)) + επανατοποθέτηση σε genre (O(hops))
   - Υπολογισμός D: μία σάρωση όλης της λίστας κάθε genre για points (O(#books_in_genre))
     + ταξινόμηση remainders (O(#genres log #genres))
     + επιλογή κορυφαίων ανά genre (γραμμική στα seats(g)).
*/


#endif /* LIB_DS_H */;
