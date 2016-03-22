#ifndef PERMUTERM_H
#define PERMUTERM_H

#define BTREE_DEGREE 9

struct isr3_word_entry;

struct isr3_permuterm_key {
	char* key;
	int key_len;
	struct isr3_word_entry* value;
};

struct isr3_permuterm_node {
	int is_leaf, num_keys;
	struct isr3_permuterm_key* keys[BTREE_DEGREE - 1];
	struct isr3_permuterm_node* children[BTREE_DEGREE];
};

struct isr3_permuterm_index {
	struct isr3_permuterm_node* root;
};

struct isr3_permuterm_index* isr3_permuterm_index_create(void);
void isr3_permuterm_index_free(struct isr3_permuterm_index* ptr);

void isr3_permuterm_index_insert(struct isr3_permuterm_index* ptr, char* key, int key_len, struct isr3_word_entry* value);
void isr3_permuterm_index_search(struct isr3_permuterm_index* ptr, char* query, int query_len, void (*callback)(struct isr3_word_entry* value));

#endif
