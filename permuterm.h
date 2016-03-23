#ifndef PERMUTERM_H
#define PERMUTERM_H

#define BTREE_DEGREE 9
#define BTREE_NUM_KEYS BTREE_DEGREE - 1
#define BTREE_NUM_CHILDREN BTREE_DEGREE

#include "debug.h"
#include "entry_types.h"

/* There are more children than keys, but we add an extra child and an extra key to allow for temporary overflows by one element. */

struct isr3_permuterm_key {
	char* key;
	int key_len;
	struct isr3_word_entry* value;
};

struct isr3_permuterm_node {
	int is_leaf, num_keys;
	struct isr3_permuterm_key* keys[BTREE_NUM_KEYS + 1];
	struct isr3_permuterm_node* children[BTREE_NUM_CHILDREN + 1];
};

struct isr3_permuterm_index {
	struct isr3_permuterm_node* root;
};

struct isr3_permuterm_index* isr3_permuterm_index_create(void);
void isr3_permuterm_index_free(struct isr3_permuterm_index* ptr);

void isr3_permuterm_index_insert(struct isr3_permuterm_index* ptr, char* key, int key_len, struct isr3_word_entry* value);
void isr3_permuterm_index_search(struct isr3_permuterm_index* ptr, char* query, int query_len, int search_id, void (*callback)(struct isr3_word_entry* value, int search_id));

void isr3_permuterm_index_dump(struct isr3_permuterm_index* ptr);

#endif
