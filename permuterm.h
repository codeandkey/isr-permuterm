#ifndef PERMUTERM_H
#define PERMUTERM_H

#define BTREE_NUM_KEYS 7
#define BTREE_NUM_CHILDREN 8

struct isr3_word_entry;

struct isr3_permuterm_key {
	char* key;
	int key_len;
};

struct isr3_permuterm_node {
	int is_leaf, num_keys;
	struct isr3_permuterm_key* keys[BTREE_NUM_KEYS];
	struct isr3_permuterm_node* children[BTREE_NUM_CHILDREN];
};

struct isr3_permuterm_index {
	struct isr3_permuterm_node* root;
};

struct isr3_permuterm_index* isr3_permuterm_index_create(void);
void isr3_permuterm_index_free(struct isr3_permuterm_index* ptr);

void isr3_permuterm_index_insert(struct isr3_permuterm_index* ptr, char* key, int key_len, struct isr3_word_entry* value);
void isr3_permuterm_index_search(struct isr3_permuterm_index* ptr, char* query, int query_len, void (*callback)(struct isr3_word_entry* value));

#endif
