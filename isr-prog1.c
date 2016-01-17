/*
 * Justin Stanley
 * Dr. Gray
 * Information Storage and Retrieval - Project 1
 * ---------------------------------------------
 *
 * This project is written in C, and should compile with the cflags `-std=gnu99`.
 * To enable verbose output, compile with `-DISR1_VERBOSE`
 *
 * -- Method --
 *
 * This program uses a binary search tree to store individual words, which heavily reduces lookup costs for words to O(log n) average.
 * To help this lookup cost get closer to the O(log n) average the values should be randomly distributed.
 * However, English words are not distributed evenly (at all).
 * To get around this, I store the words in the tree based on a fast hash () of their value.
 * I use a simple linked list structure to store which files reference the word.
 * Each file is read incrementally to reduce memory usage.
 *
 * One of the larger problems I had to tackle while writing this was the issue of sorting all of the words for the output.
 * It is a very difficult problem to sort a BST, especially when it is indexed by hashes rather than the target value to be sorted.
 * I got around this problem by making each word entry exist in two different linked lists simultaneously. Each word entry has two `next` pointer.
 * This allows me to efficiently keep a contiguous linked list of all the independent word entries with little overhead and no memory penalties.
 * Now that I had a contiguous list of words, I used my word comparison function to implement a mergesort on the linked list.
 *
 * -- Profiling --
 *
 */

/*
 * Constant definitions.
 * We assume the maximum word length is no more than 32.
 */

#define ISR1_WORD_LENGTH 32
#define ISR1_HASH_LENGTH 4

/*
 * Debug output control.
 * If we are not configured to be verbose, we trash the definition of isr1_debug.
 */

#ifdef ISR1_VERBOSE
#define isr1_debug(x, ...) fprintf(stderr, "[%s] " x, __func__, ##__VA_ARGS__)
#else
#define isr1_debug
#endif

#define isr1_err(x, ...) fprintf(stderr, "[%s] " x, __func__, ##__VA_ARGS__)

/*
 * Header includes.
 * We only really need some parts of the stdlib.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

/*
 * Since we are hashing the word values to store them in the tree, we will need to utilize open hashing to keep track of words with the same hash.
 * This leads us to create a 2D linked list: One axis is the target word, the other is which files reference it.
 */

typedef struct isr1_ref_entry isr1_ref_entry;

struct isr1_ref_entry {
	unsigned int ref_id;
	isr1_ref_entry* next;
};

typedef struct isr1_word_entry isr1_word_entry;

struct isr1_word_entry {
	char word[ISR1_WORD_LENGTH];
	int word_len;
	isr1_ref_entry* ref_list; // List of which files reference this word.
	isr1_word_entry* next, *global_next; // The usage of two `next` members is explained in main().
};

/*
 * The tree structure is pretty straightforward.
 * Each node has a hash value and a list of words which hashed to that value.
 * Each word has a list of file references.
 */

typedef struct isr1_tree_node isr1_tree_node;

struct isr1_tree_node {
	isr1_tree_node* left, *right;
	isr1_word_entry* word_list; // List of words corresponding to node_hash[].
	char node_hash[ISR1_HASH_LENGTH];
};

/* Program function declarations. */

int parse_file(const char* filename, unsigned int ref_id, isr1_tree_node** root, isr1_word_entry** global_list, int* largest_word_length); /* Parse a file into the tree. */
int insert_word(char* word_buf, int word_len, unsigned int ref_id, isr1_tree_node** root, isr1_word_entry** global_list); /* Insert a word into the tree. */
isr1_word_entry* sort_list(isr1_word_entry* word_list);
void free_tree(isr1_tree_node* root);

/* Utility functions : hashing and comparing words. */

int hash_word(char* word_buf, int word_len, char* out_buf, int out_len); /* Hash a word into a buffer. */
int word_cmp(char* word_buf1, int word_len1, char* word_buf2, int word_len2); /* Returns 1 if word1 > word2, -1 if word1 < word2, and 0 if word1 = word2. */

/* Internal sorting functions. */

isr1_word_entry* merge_nodes(isr1_word_entry* first, isr1_word_entry* second);
int divide_list(isr1_word_entry* head, isr1_word_entry** first, isr1_word_entry** second);
int list_length(isr1_word_entry* head);

/* Function definitions. */

int main(int argc, char** argv) {
	isr1_debug("Starting ISR1.\n");
	isr1_tree_node* root = NULL;
	isr1_word_entry* word_list_g = NULL;

	int largest_word = 0;

	if (argc > 1) {
		for (int i = 1; i < argc; ++i) {
			isr1_debug("Parsing input file %s..\n", argv[i]);

			if (!parse_file(argv[i], i - 1, &root, &word_list_g, &largest_word)) { /* We just pass `i - 1` as the reference ID. Makes it very eays to ID files in order. */
				isr1_err("Parsing failed for file [%s].\n", argv[i]);
				return 1;
			}
		}
	} else {
		isr1_err("No files passed to program.\n");
		isr1_err("Usage: %s <file1> <file2> <fileN>\n", argv[0]);
		return 1;
	}

	/* At this point, we have a BST of all words and their respective references. However, they are not sorted. */

	/*
	 * While the tree structure is great for accelerating lookup and store speeds, it is really awful for sorting (especially considering it is indexed via hashing)
	 * To approach this problem, the program keeps two lists active during loading.
	 * Each word structure has a second "next" members because they exist in two seperate lists simultaneously -- which reference the same objects.
	 * A global, continguous linked list of words is much easier to sort; we will perform a mergesort to keep it fast.
	 */

	word_list_g = sort_list(word_list_g); /* Sort the wordlist as we prepare to output. */

	/* At this point, the wordlist is complete and we can just walk through it. First, we prepare the legend. This is a bit tedious. */

	fprintf(stdout, "Word");

	for (int i = 0; i < largest_word; ++i) {
		fputc(' ', stdout);
	}

	fprintf(stdout, " Posting\n");

	for (int i = 0; i < largest_word; ++i) {
		fputc('-', stdout);
	}

	fputc(' ', stdout);

	for (int i = 0; i < argc - 1; ++i) {
		fprintf(stdout, "---");
	}

	fputc('\n', stdout);

	while (word_list_g) {
		fprintf(stdout, "%.*s", word_list_g->word_len, word_list_g->word);

		for (int i = 0; i < (largest_word - word_list_g->word_len) + 1; ++i) { /* Extra space to accomadate the column seperator. */
			fputc(' ', stdout);
		}

		isr1_ref_entry* cur_ref = word_list_g->ref_list;

		while (cur_ref) {
			fprintf(stdout, "%02d ", cur_ref->ref_id + 1);
			cur_ref = cur_ref->next;
		}

		fputc('\n', stdout);
		word_list_g = word_list_g->global_next;
	}

	free_tree(root); /* We cleanly exit, returning all memory to the OS. */
	return 0;
}

int parse_file(const char* filename, unsigned int ref_id, isr1_tree_node** root, isr1_word_entry** global_list, int* largest_word_length) {
	FILE* fd = fopen(filename, "r");

	if (!fd) {
		isr1_err("Failed to open [%s] for reading.\n", filename);
		return 0;
	}

	char cur_word[ISR1_WORD_LENGTH] = {0};
	int cur_word_len = 0; /* Instead of using strlen, we just keep a counter. Much faster. */

	char cur_char = 0; /* This stores the most recent character read to make things more readable. */

	while (!feof(fd)) {
		/* First: get rid of any preceding whitespace (but store the last character in the first index of cur_word) */
		while (isspace(cur_char = fgetc(fd)));

		if (cur_char == EOF) {
			break; /* Depending on the file, an EOF may occur here if there is no whitespace between the last word and the EOF. */
		}

		cur_word[0] = cur_char;
		cur_word_len = 1; /* The character which terminated the last loop is the first character of the target word. */

		/* Next: read until the next whitespace character. */
		while (!isspace(cur_char = fgetc(fd)) && !feof(fd)) {
			if (cur_word_len >= ISR1_WORD_LENGTH) {
				isr1_err("Word too large for buffer length! Buffer size : %d\n", ISR1_WORD_LENGTH);
				break;
			}

			cur_word[cur_word_len++] = cur_char; /* If this were in the while condition, it would increment even when fgetc() returned whitespace. */
		}

		isr1_debug("Read word with length %d, data [%.*s]\n", cur_word_len, cur_word_len, cur_word);

		if (largest_word_length && cur_word_len >= *largest_word_length) {
			 *largest_word_length = cur_word_len;
		}

		if (!insert_word(cur_word, cur_word_len, ref_id, root, global_list)) {
			isr1_err("Failed to insert word into tree.\n");
			return 0;
		}

		memset(cur_word, 0, sizeof cur_word / sizeof *cur_word);
		cur_word_len = 0;
	}

	fclose(fd);
	return 1;
}

int insert_word(char* word_buf, int word_len, unsigned int ref_id, isr1_tree_node** root, isr1_word_entry** global_list) {
	if (!word_buf || !root) {
		isr1_err("Invalid input!\n");
		return 0;
	}

	if (word_len > ISR1_WORD_LENGTH) {
		isr1_err("Invalid word length.\n");
		return 0;
	}

	isr1_ref_entry* new_ref_entry = malloc(sizeof *new_ref_entry);

	if (!new_ref_entry) {
		isr1_err("malloc() failed. System may be out of RAM!\n");
		exit(1);
	}

	new_ref_entry->ref_id = ref_id;
	new_ref_entry->next = NULL;

	/* We will have to hash the word, but we won't necessarily have to create a word entry. */
	char word_hash[ISR1_HASH_LENGTH] = {0};
	hash_word(word_buf, word_len, word_hash, sizeof word_hash / sizeof *word_hash);

	isr1_tree_node** cur_node = root;

	while (*cur_node) {
		int result = memcmp(word_hash, (*cur_node)->node_hash, ISR1_HASH_LENGTH);

		if (result > 0) {
			/* Our hash is larger -> We want to continue search from the right child. */
			cur_node = &(*cur_node)->right;
		} else if (result < 0) {
			cur_node = &(*cur_node)->left;
			/* Our hash is smaller -> We want to continue searching from the left child. */
		} else {
			/* The hashes are equal -> We want to locate the target word in this node. */
			/* This will be a generally quick procedure. We scan through the wordlist and insert our ref_id. */
			isr1_word_entry* cur_word_entry = (*cur_node)->word_list;
			int located = 0; /* We keep a small flag to indicate whether we need to insert a new word entry. */

			while (cur_word_entry) {
				if (!word_cmp(cur_word_entry->word, cur_word_entry->word_len, word_buf, word_len)) {
					/* We found our word. Add our refID and set the located flag. */
					isr1_ref_entry* cur_ref = cur_word_entry->ref_list;
					int located_ref = 0;

					while (cur_ref) {
						if (cur_ref->ref_id == ref_id) {
							located_ref = 1;
							break;
						}

						cur_ref = cur_ref->next;
					}

					if (!located_ref) {
						new_ref_entry->next = cur_word_entry->ref_list;
						cur_word_entry->ref_list = new_ref_entry;
					}

					located = 1;
				}

				cur_word_entry = cur_word_entry->next;
			}

			if (!located) {
				/* We didn't find our word in the entry list. Add a new one! */
				isr1_word_entry* new_entry = malloc(sizeof *new_entry);

				if (!new_entry) {
					isr1_err("malloc() failed. System may be out of RAM!\n");
					exit(1);
				}

				memset(new_entry->word, 0, sizeof new_entry->word / sizeof *(new_entry->word));
				memcpy(new_entry->word, word_buf, word_len);

				new_entry->word_len = word_len;

				new_entry->ref_list = new_ref_entry;
				new_entry->next = (*cur_node)->word_list;

				(*cur_node)->word_list = new_entry;

				new_entry->global_next = *global_list;
				*global_list = new_entry;
			}

			return 1; /* Once this is hit, we guarantee the word will be inserted. */
		}
	}

	/*
	 * If *cur_node is ever NULL, it means there is no node where we are search and we never found our own hash.
	 * We can then insert a new node into the tree.
	 */

	isr1_debug("No node found. Inserting new node..\n");

	isr1_tree_node* new_node = malloc(sizeof *new_node);

	if (!new_node) {
		isr1_err("malloc() failed. System may be out of RAM!\n");
		exit(1);
	}

	memcpy(new_node->node_hash, word_hash, ISR1_HASH_LENGTH);
	new_node->left = new_node->right = NULL;

	isr1_word_entry* new_entry = malloc(sizeof *new_entry);

	if (!new_entry) {
		isr1_err("malloc() failed. System may be out of RAM!\n");
		exit(1);
	}

	memset(new_entry->word, 0, sizeof new_entry->word / sizeof *(new_entry->word));
	memcpy(new_entry->word, word_buf, word_len);

	new_entry->word_len = word_len;

	new_entry->ref_list = new_ref_entry;
	new_entry->next = new_node->word_list;

	new_node->word_list = new_entry;
	new_entry->global_next = *global_list;

	*global_list = new_entry;
	*cur_node = new_node;
	return 1;
}

int hash_word(char* word_buf, int word_len, char* hash_buf, int hash_len) {
	/* hash_word implements the SDBM hash algorithm, a small and fast hashing algorithm with an emphasis on performance and minimizing collisions. */

	if (hash_len != ISR1_HASH_LENGTH) {
		isr1_err("Hash output buffer is incorrect size. ISR1 hash length is %d, but buffer is %d.\n", ISR1_HASH_LENGTH, hash_len);
		return 0;
	}

	if (!hash_buf || !word_buf || !word_len) {
		isr1_err("Invalid input to hash procedure.\n");
		return 0;
	}

	if (ISR1_HASH_LENGTH != 4) {
		isr1_err("SDBM must operate with a hash length of 4 bytes.\n");
		return 0;
	}

	uint32_t* hash_value = (uint32_t*) hash_buf;
	*hash_value = 0;

	for (int i = 0; i < word_len; ++i) {
		*hash_value = word_buf[i] + (*hash_value << 6) + (*hash_value << 16) - *hash_value;
	}

	return 1;
}

int word_cmp(char* word_buf1, int word_len1, char* word_buf2, int word_len2) {
	int min_length = word_len1 > word_len2 ? word_len2 : word_len1;
	int result = memcmp(word_buf1, word_buf2, min_length);

	if (result) {
		return (result < 0) ? -1 : 1;
	}

	if (word_len1 > word_len2) {
		return 1;
	} else if (word_len1 < word_len2) {
		return -1;
	} else {
		return 0;
	}
}

void free_tree(isr1_tree_node* root) {
	/* Quick recursive cleanup of the tree and everything allocated inside. */

	if (!root) {
		return;
	}

	free_tree(root->left);
	free_tree(root->right);

	isr1_word_entry* cur_word = root->word_list, *tmp_word = NULL;

	while (cur_word) {
		isr1_ref_entry* cur_ref = cur_word->ref_list, *tmp_ref = NULL;

		while (cur_ref) {
			tmp_ref = cur_ref->next;
			free(cur_ref);
			cur_ref = tmp_ref;
		}

		tmp_word = cur_word->next;
		free(cur_word);
		cur_word = tmp_word;
	}

	free(root);
}

isr1_word_entry* sort_list(isr1_word_entry* head) {
	isr1_word_entry* first = NULL, *second = NULL;

	if (!head || !head->global_next) {
		return head;
	}

	divide_list(head, &first, &second);

	first = sort_list(first);
	second = sort_list(second);

	/* Merge operation.. with linked lists! */
	return merge_nodes(first, second);
}

isr1_word_entry* merge_nodes(isr1_word_entry* first, isr1_word_entry* second) {
	if (!first && second) {
		second->global_next = merge_nodes(first, second->global_next);
		return second;
	} else if (first && !second) {
		first->global_next = merge_nodes(first->global_next, second);
		return first;
	} else if (!first && !second) {
		return NULL;
	}

	int result = word_cmp(first->word, first->word_len, second->word, second->word_len);

	if (result <= 0) { // This is '<=' because we don't need to worry about which item to select if they are the same word.. In fact, we should NEVER encounter the same word anyway.
		first->global_next = merge_nodes(first->global_next, second);
		return first;
	} else {
		second->global_next = merge_nodes(first, second->global_next);
		return second;
	}
}

int divide_list(isr1_word_entry* head, isr1_word_entry** first, isr1_word_entry** second) {
	if (!head || !head->global_next) {
		*first = head;
		(*first)->global_next = NULL;
		*second = NULL;

		isr1_debug("List length is <2, returning single element\n");

		return 1;
	}

	isr1_word_entry* slow = head, *fast = head->global_next;

	while (fast) {
		fast = fast->global_next;

		if (fast) {
			slow = slow->global_next;
			fast = fast->global_next;
		}
	}

	/* We pushed `fast` ahead at 2x the speed of slow, and this places slow one step before the midpoint. */

	*first = head;
	*second = slow->global_next;

	slow->global_next = NULL;

	return 1;
}

int list_length(isr1_word_entry* head) {
	/* Only use when necessary. This walks the entire list. */

	int output = 0;

	while (head) {
		output++;
		head = head->global_next;
	}

	return output;
}
