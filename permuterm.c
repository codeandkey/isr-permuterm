#include "permuterm.h"

static int cmp_permuterm_node(char* query, int query_len, struct isr3_permuterm_key* key);
static int cmp_permuterm_prefix(char* query, int query_len, struct isr3_permuterm_key* key);
static void isr3_permuterm_node_search(struct isr3_permuterm_node* node, char* query, int query_len, void (*callback)(struct isr3_word_entry* value));

struct isr3_permuterm_index* isr3_permuterm_index_create(void) {
	struct isr3_permuterm_index* output = malloc(sizeof *output);

	if (!output) {
		return NULL;
	}

	output->buffer = NULL;
	output->buflen = 0;

	return output;
}

void isr3_permuterm_index_free(struct isr3_permuterm_index* ptr) {
	for (int i = 0; i < ptr->buflen; ++i) {
		free(ptr->buffer[i]);
	}

	free(ptr->buffer);
	free(ptr);
}

void isr3_permuterm_index_insert(struct isr3_permuterm_index* ptr, char* key, int key_len, struct isr3_world_entry* value) {
	struct isr3_permuterm_key* new_key = malloc(sizeof *new_key);

	new_key->key = malloc(key_len);
	memcpy(new_key->key, key, key_len);

	new_key->key_len = key_len;
	new_key->value = value;

	if (!ptr->root) {
		ptr->root = malloc(sizeof *(ptr->root));
		ptr->root->is_leaf = 1;
		ptr->root->keys[0] = 
		ptr->root->num_keys = 1;
	}
}

void isr3_permuterm_index_search(struct isr3_permuterm_index* ptr, char* query, int query_len, void (*callback)(struct isr3_word_entry* value)) {
	isr3_permuterm_node_search(ptr->root, query, query_len, callback);
}

int isr3_permuterm_node_search(struct isr3_permuterm_node* node, char* query, int query_len, void (*callback)(struct isr3_word_entry* value)) {
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

	int result;

	for (int i = 0; i < node->num_keys; ++i) {
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
			return isr3_permuterm_node_search(node->children[BTREE_NUM_CHILDREN - 1], query, query_len, callback);
		}
	}

	if (result < 0) {
		/* We found a node that is greater than the key. The chain could begin in the left leaf (or on this node). */
		isr3_permuterm_node_search(node->children[i], query, query_len, callback);
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
			callback(node->keys[i]->value);
		} else {
			result = 0;
			break;
		}

		if (!node->is_leaf) {
			if (!isr3_permuterm_node_search(node->children[i + 1], query, query_len, callback)) {
				result = 0;
				break;
			}
		}
	}

	return result;
}

int cmp_permuterm_node(char* query, int query_len, struct isr3_permuterm_key* key) {
	int min_length = query_len > key->key_len ? key->key_len : query_len;
	int result = memcmp(query, key, min_length);

	if (result < 0) {
		return -1;
	} else if (result > 0) {
		return 1;
	}

	if (query_len > key->key_len) {
		return 1;
	} else if (query_len < key->key_len) {
		return -1;
	}

	return 0;
}

int cmp_permuterm_prefix(char* query, int query_len, struct isr3_permuterm_key* key) {
	if (query_len > key->key_len) {
		return 0;
	}

	if (memcmp(query, key->key, query_len)) {
		return 0;
	}

	return 1;
}
