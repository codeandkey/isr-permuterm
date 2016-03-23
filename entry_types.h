#ifndef ISR3_ENTRY_TYPES
#define ISR3_ENTRY_TYPES

typedef struct isr3_ref_entry isr3_ref_entry;

struct isr3_ref_entry {
        unsigned int ref_id;
        isr3_ref_entry* next;
};

typedef struct isr3_word_entry isr3_word_entry;

struct isr3_word_entry {
        char* word; // We'll have to use dynamically allocated strings!
        int word_len;
        isr3_ref_entry* ref_list_head, *ref_list_tail; // List of which files reference this word.
        isr3_word_entry* next, *global_next; // The usage of two `next` members is explained in main() and in the file header comments.
};

#endif
