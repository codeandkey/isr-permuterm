#include "permuterm.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int cmp_permuterm_node(char* query, int query_len, struct isr3_permuterm_key* key);
static int cmp_permuterm_prefix(char* query, int query_len, struct isr3_permuterm_key* key);
static int isr3_permuterm_node_search(struct isr3_permuterm_node* node, char* query, int query_len, int search_id, void (*callback)(struct isr3_word_entry* value, int search_id));

static void isr3_permuterm_node_insert_root(struct isr3_permuterm_node** root, struct isr3_permuterm_key* value);
static int isr3_permuterm_node_insert_mid(struct isr3_permuterm_node* parent, int index, struct isr3_permuterm_key* value);
static int isr3_permuterm_node_insert_leaf(struct isr3_permuterm_node* parent, int index, struct isr3_permuterm_key* value);
static int isr3_permuterm_node_is_full(struct isr3_permuterm_node* node);

struct isr3_permuterm_index* isr3_permuterm_index_create(void) {
	struct isr3_permuterm_index* output = malloc(sizeof *output);

	if (!output) {
		return NULL;
	}

	output->root = NULL;
	return output;
}

void isr3_permuterm_index_free(struct isr3_permuterm_index* ptr) {
	/* TODO: recursively free tree */

	free(ptr);
}

void isr3_permuterm_index_insert(struct isr3_permuterm_index* ptr, char* key, int key_len, struct isr3_word_entry* value) {
	struct isr3_permuterm_key* new_key = malloc(sizeof *new_key);

	if (!new_key) {
		isr3_err("malloc failed with new btree node\n");
		exit(1);
	}

	new_key->key = malloc(key_len);

	if (!new_key->key) {
		isr3_err("malloc failed with new btree key\n");
		exit(1);
	}

	memcpy(new_key->key, key, key_len);

	new_key->key_len = key_len;
	new_key->value = value;

	isr3_debugf("inserting node with key [%.*s], value %p (%.*s)\n", key_len, key, (void*) value, value->word_len, value->word);
	isr3_permuterm_node_insert_root(&ptr->root, new_key);
}

void isr3_permuterm_node_insert_root(struct isr3_permuterm_node** root, struct isr3_permuterm_key* key) {
	if (!*root) {
		*root = malloc(sizeof **root);
		(*root)->is_leaf = 1;
		(*root)->num_keys = 1;
		(*root)->keys[0] = key;

		return;
	}

	if ((*root)->is_leaf) {
		int full = isr3_permuterm_node_is_full(*root), i, result;
		/* This is a leaf node, we insert the key where it belongs while pushing the other elements to the side. */
		i = (*root)->num_keys - 1;

		/* We add another slot to cycle the keys. */
		(*root)->num_keys++;

		while (1) {
			if (i < 0) {
				/* i has crossed into OOB space, and the whole array has shifted -- insert at i + 1. */
				break;
			}

			result = cmp_permuterm_node(key->key, key->key_len, (*root)->keys[i]);

			if (result > 0) {
				/* The node in the key list is no longer greater than the passed key param. The correct location to insert the key is now i + 1. */
				break;
			}

			if (!result) {
				/* -- debug : notify repeated keys -- */
				isr3_debugf("REPEATED KEY %.*s\n", key->key_len, key->key);
				exit(1);
			}

			(*root)->keys[i + 1] = (*root)->keys[i];
			i--;
		}

		(*root)->keys[i + 1] = key;

		if (full) {
			/* root was full and now has to split */

			struct isr3_permuterm_node* new_root = malloc(sizeof *new_root), *right_split = malloc(sizeof *right_split), *left_split = *root;

			new_root->is_leaf = 0;
			new_root->num_keys = 1;
			new_root->keys[0] = left_split->keys[(BTREE_DEGREE - 1) / 2]; /* Median value at pos 5 if key limit is 8. */
			new_root->children[0] = left_split;
			new_root->children[1] = right_split;

			/* Left split is still filled with all of the old data. */
			right_split->is_leaf = 1;
			right_split->num_keys = (BTREE_DEGREE - 1) / 2;

			for (int j = 0; j < (BTREE_DEGREE - 1) / 2; ++j) {
				right_split->keys[j] = left_split->keys[j + 1 + (BTREE_DEGREE - 1) / 2]; /* We have to add two to skip the median. */
			}

			left_split->num_keys = (BTREE_DEGREE - 1) / 2;

			*root = new_root;
		}
	} else {
		int i, result;

		for (i = 0; i < (*root)->num_keys; ++i) {
			result = cmp_permuterm_node(key->key, key->key_len, (*root)->keys[i]);

			if (result <= 0) {
				break;
			}
		}

		/* Appropriate child is at index i. */
		result = 0;

		if ((*root)->children[i]->is_leaf) {
			result = isr3_permuterm_node_insert_leaf(*root, i, key);

			if (result) {
				/* the child leaf was full and after splitting, we are also full. perform a root split. */
				isr3_debug("split root (leaf child overflow)\n");
			}
		} else {
			result = isr3_permuterm_node_insert_mid(*root, i, key);

			if (result) {
				/* something down the line overflowed and we are also full. perform a root split. */
				isr3_debug("split root (mid child overflow)\n");
			}
		}

		if (result) {
			/* a root split is required, except we also have to copy children in the node to the right split */
			struct isr3_permuterm_node* new_root = malloc(sizeof *new_root), *right_split = malloc(sizeof *right_split), *left_split = *root;

			new_root->is_leaf = 0;
			new_root->num_keys = 1;
			new_root->keys[0] = left_split->keys[(BTREE_DEGREE - 1) / 2]; /* Median value at pos 5 if key limit is 8. */
			new_root->children[0] = left_split;
			new_root->children[1] = right_split;

			/* Left split is still filled with all of the old data. */
			right_split->is_leaf = 0;
			right_split->num_keys = (BTREE_DEGREE - 1) / 2;

			right_split->children[0] = left_split->children[(BTREE_DEGREE - 1) / 2 + 1]; /* The loop doesn't get to the first child of right_split, so we assign it the child to the right of the median */

			for (int j = 0; j < (BTREE_DEGREE - 1) / 2; ++j) {
				right_split->keys[j] = left_split->keys[j + 1 + (BTREE_DEGREE - 1) / 2]; /* We have to add two to skip the median. */
				right_split->children[j + 1] = left_split->children[j + 2 + (BTREE_DEGREE - 1) / 2]; /* We add two to skip the median and to query the right child. */
			}

			left_split->num_keys = (BTREE_DEGREE - 1) / 2;

			*root = new_root;
		}
	}
}

int isr3_permuterm_node_insert_mid(struct isr3_permuterm_node* parent, int index, struct isr3_permuterm_key* key) {
	struct isr3_permuterm_node* node = parent->children[index], *app_child;
	int i, result, parent_full = isr3_permuterm_node_is_full(parent);

	for (i = 0; i < node->num_keys; ++i) {
		result = cmp_permuterm_node(key->key, key->key_len, node->keys[i]);

		if (result <= 0) {
			break;
		}
	}

	/* -- debug : notify repeated keys -- */
	if (!result) {
		isr3_errf("REPEATED KEY %.*s\n", key->key_len, key->key);
		exit(1);
	}

	/* Appropriate child is at index i */
	app_child = node->children[i];

	if (app_child->is_leaf) {
		result = isr3_permuterm_node_insert_leaf(node, i, key);

		if (result) {
			/* Inserting into the leaf has caused us to overflow. Split! */
			isr3_debug("split mid\n");

			/* if the parent was full before the split, parent has overflowed. */
			if (parent_full) {
				isr3_debug("mid parent overflow\n");
			}
		} else {
			/* We did not overflow from the insert -- our parent will not overflow. */
			return 0;
		}
	} else {
		result = isr3_permuterm_node_insert_mid(node, i, key);

		if (result) {
			/* Something down the line caused us to overflow. Split! */
			isr3_debug("split mid\n");

			/* if the parent was full before the split, parent has overflowed. */
			if (parent_full) {
				isr3_debug("mid parent overflow\n");
			}
		} else {
			/* We did not overflow from the insert -- our parent will not overflow. */
			return 0;
		}
	}

	if (result) {
		/* We will be splitting! We must do everything a root-mid split does, except we also have to ensure correct placement in the parent. */
		struct isr3_permuterm_node* right_split = malloc(sizeof *right_split), *left_split = node;

		/* We'll steal some code from leaf and root. */
		int j = parent->num_keys;
		parent->num_keys++;

		/* in the parent, we need to perform something similar to a leaf shift, although we also need to shift children. */
		while (j > index) {
			parent->keys[j] = parent->keys[j - 1];
			parent->children[j + 1] = parent->children[j]; /* i may have gotten this wrong TODO */
			j--;
		}

		parent->keys[index] = left_split->keys[(BTREE_DEGREE - 1) / 2];
		parent->children[index + 1] = right_split;

		/* Left split is still filled with all of the old data. */
		right_split->is_leaf = 0;
		right_split->num_keys = (BTREE_DEGREE - 1) / 2;

		right_split->children[0] = left_split->children[(BTREE_DEGREE - 1) / 2 + 1]; /* The loop doesn't get to the first child of right_split, so we assign it the child to the right of the median */

		for (int j = 0; j < (BTREE_DEGREE - 1) / 2; ++j) {
			right_split->keys[j] = left_split->keys[j + 1 + (BTREE_DEGREE - 1) / 2]; /* We have to add two to skip the median. */
			right_split->children[j + 1] = left_split->children[j + 2 + (BTREE_DEGREE - 1) / 2]; /* We add two to skip the median and to query the right child. */
		}

		left_split->num_keys = (BTREE_DEGREE - 1) / 2;
	}

	/* if the parent was full before the split, parent has overflowed. */
	if (parent_full) {
		isr3_debug("mid parent overflow\n");
	}

	return parent_full;
}

int isr3_permuterm_node_insert_leaf(struct isr3_permuterm_node* parent, int index, struct isr3_permuterm_key* key) {
	struct isr3_permuterm_node* node = parent->children[index];
	int i, full = isr3_permuterm_node_is_full(node), parent_full = isr3_permuterm_node_is_full(parent), result;

	/* This is a leaf node, we insert the key where it belongs while pushing the other elements to the side. */
	i = node->num_keys - 1;

	/* We add another slot to cycle the keys. */
	node->num_keys++;

	while (1) {
		if (i < 0) {
			/* i has crossed into OOB space, and the whole array has shifted -- insert at i + 1. */
			break;
		}

		result = cmp_permuterm_node(key->key, key->key_len, node->keys[i]);

		if (result > 0) {
			/* The node in the key list is no longer greater than the passed key param. The correct location to insert the key is now i + 1. */
			break;
		}

		if (!result) {
			/* -- debug : notify repeated keys -- */
			isr3_errf("REPEATED KEY %.*s\n", key->key_len, key->key);
			exit(1);
		}

		node->keys[i + 1] = node->keys[i];
		i--;
	}

	node->keys[i + 1] = key;

	/* Now, the key is in the buffer and at the correct location. We still need to check for overflows and split accordingly. */
	if (full) {
		/* If the node was full before it has now overflowed. Split! */

		/*
		 * When a leaf splits, it is a very similar process to a root split -- except it's very important that we know exactly where the new node should go in the parent.
		 */

		isr3_debugf("splitting leaf node at parent index %d\n", index);

		struct isr3_permuterm_node* right_split = malloc(sizeof *right_split), *left_split = node;

		int j = parent->num_keys;
		parent->num_keys++;

		/* in the parent, we need to perform something similar to a leaf shift, although we also need to shift children. */
		while (j > index) {
			parent->keys[j] = parent->keys[j - 1];
			parent->children[j + 1] = parent->children[j]; /* i may have gotten this wrong TODO */
			j--;
		}

		parent->keys[index] = left_split->keys[(BTREE_DEGREE - 1) / 2];
		parent->children[index + 1] = right_split;

		/* Left split is still filled with all of the old data. */
		right_split->is_leaf = 1;
		right_split->num_keys = (BTREE_DEGREE - 1) / 2;

		for (int j = 0; j < (BTREE_DEGREE - 1) / 2; ++j) {
			right_split->keys[j] = left_split->keys[j + 1 + (BTREE_DEGREE - 1) / 2]; /* We have to add two to skip the median. */
		}

		left_split->num_keys = (BTREE_DEGREE - 1) / 2;

		/* If the parent was full before the split, the parent has overflowed. */
		if (parent_full) {
			isr3_debug("leaf parent overflow\n");
		}

		return parent_full;
	}

	/* The node wasn't full before the insert, it is impossible for an overflow to have occurred. */

	return full;
}

void isr3_permuterm_index_search(struct isr3_permuterm_index* ptr, char* query, int query_len, int search_id, void (*callback)(struct isr3_word_entry* value, int search_id)) {
	isr3_permuterm_node_search(ptr->root, query, query_len, search_id, callback);
}

int isr3_permuterm_node_search(struct isr3_permuterm_node* node, char* query, int query_len, int search_id, void (*callback)(struct isr3_word_entry* value, int search_id)) {
	/* The permuterm search process varies heavily from a normal B-tree search in that we need to be able to find all matching keys (any equal prefixes).
	 * To do this, we must find the smallest value greater than the prefix. This is somewhat simple.
	 * After finding this value, we recurse through the lowest tree nodes and keep going "forward" until the prefix no longer matches.
	 * As the tree is sorted, this should work well for enumerating all possible matches.
	 *
	 * Return value:
	 * 1 => There were matching values in sub-calls and the chain continues past this node. (Continue)
	 * 0 => No matches or the chain of matches does not continue past this child.
	 *
	 * To enumerate these values, we recursively search the tree:
	 *  Each key is considered in order -- if the key is greater than the prefix and not a leaf, the chain could start earlier => perform the search on the child to the left of the key and IF the search returns 1 (chain extends past leaf) continue the chain at the key, enumerating all children and future keys until the prefix doesn't match.
	 * if the key is equal to the prefix, the chain starts here -- enumerate all future children and keys until the prefix doesn't match.
	 * If no keys >= and not leaf, search last child.
	 */

	int result, i;

	for (i = 0; i < node->num_keys; ++i) {
		result = cmp_permuterm_node(query, query_len, node->keys[i]);

		if (result <= 0) {
			break;
		}
	}

	if (result == 1) {
		/* Nothing found in keys. If we're not a leaf, the target could still be in the last child. Continuity is passed. */
		if (node->is_leaf) {
			return 0;
		} else {
			return isr3_permuterm_node_search(node->children[i], query, query_len, search_id, callback);
		}
	}

	if (result < 0) {
		/* We found a node that is greater than the key. The chain could begin in the left leaf (or on this node). */
		if (!node->is_leaf) {
			isr3_permuterm_node_search(node->children[i], query, query_len, search_id, callback);
		}

		/* We don't actually need to consider the result -- we will have to check ourselves anyway. */

		if (!cmp_permuterm_prefix(query, query_len, node->keys[i])) {
			return 0; /* Greater than, but not matching prefix. It is impossible for a chain to start. */
		}
	}

	/* At this point, there is a chain which could start here. We start walking to the end of our keys and children. */
	result = 1;

	for (; i < node->num_keys; ++i) {
		/* We check the current node (i) and then the right child (i + 1) */

		if (cmp_permuterm_prefix(query, query_len, node->keys[i])) {
			callback(node->keys[i]->value, search_id);
		} else {
			result = 0;
			break;
		}

		if (!node->is_leaf) {
			if (!isr3_permuterm_node_search(node->children[i + 1], query, query_len, search_id, callback)) {
				result = 0;
				break;
			}
		}
	}

	return result;
}

int cmp_permuterm_node(char* query, int query_len, struct isr3_permuterm_key* key) {
	isr3_debugf("comparing [%.*s] with [%.*s] : ", query_len, query, key->key_len, key->key);

	int min_length = query_len > key->key_len ? key->key_len : query_len;
	isr3_debugf("min length : %d\n", min_length);

	for (int i = 0; i < min_length; ++i) {
		if (query[i] < key->key[i]) {
			isr3_debug("-1 (memcmp)\n");
			return -1;
		} else if (query[i] > key->key[i]) {
			isr3_debug("1  (memcmp)\n");
			return 1;
		}
	}

	if (query_len > key->key_len) {
		isr3_debug("1 (length)\n");
		return 1;
	} else if (query_len < key->key_len) {
		isr3_debug("-1 (length)\n");
		return -1;
	}

	isr3_debug("0 -- equal\n");
	return 0;
}

int cmp_permuterm_prefix(char* query, int query_len, struct isr3_permuterm_key* key) {
	isr3_debugf("testing [%.*s] for prefix [%.*s]: ", key->key_len, key->key, query_len, query);

	if (query_len > key->key_len) {
		isr3_debug("fail length\n");
		return 0;
	}

	for (int i = 0; i < query_len; ++i) {
		if (query[i] != key->key[i]) {
			isr3_debugf("fail mismatch on %d\n", i);
			return 0;
		}
	}

	isr3_debug("pass\n");
	return 1;
}

int isr3_permuterm_node_is_full(struct isr3_permuterm_node* node) {
	return node->num_keys >= BTREE_DEGREE - 1;
}
