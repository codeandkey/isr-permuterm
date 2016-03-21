/*
 * Justin Stanley
 * Dr. Gray
 * Information Storage and Retrieval - Project 2
 * ---------------------------------------------
 *
 * This project is written in C (ISO C99), and should compile with the cflags `-std=gnu99`.
 * To enable verbose output, compile with `-DISR3_VERBOSE`
 *
 * -- Method --
 *
 * (Largely the same as project 1, except with stemming)
 *
 * This program uses a binary search tree to store individual words, which heavily reduces lookup costs for words to O(log n) average.
 * Each word is stored based on an SBDM hash of itself.
 * Each file is read incrementally to reduce memory usage.
 *
 * <Comment from project 1 detailing sorting of output>
 * * One of the larger problems I had to tackle while writing this was the issue of sorting all of the words for the output.
 * * It is a very difficult problem to sort a BST, especially when it is indexed by hashes rather than the target value to be sorted.
 * * I got around this problem by making each word entry exist in two different linked lists simultaneously. Each word entry has two `next` pointers.
 * * This allows me to efficiently keep a contiguous linked list of all the independent word entries with little overhead and no memory penalties.
 * * Now that I had a contiguous list of words, I used my word comparison function to implement a mergesort on the linked list.
 *
 */

/*
 * Constant definitions.
 */

#define ISR3_HASH_LENGTH 4
#define ISR3_BTREE "./search.db"

/*
 * Debug output control.
 * If we are not configured to be verbose, we trash the definition of isr3_debugf.
 */

#ifdef ISR3_VERBOSE
#define isr3_debugf(x, ...) fprintf(stderr, "[%s] " x, __func__, ##__VA_ARGS__)
#define isr3_debug(x) fprintf(stderr, "[%s] " x, __func__)
#else
#define isr3_debugf(x, ...)
#define isr3_debug(x)
#endif

#define isr3_errf(x, ...) fprintf(stderr, "[%s] " x, __func__, ##__VA_ARGS__)
#define isr3_err(x) fprintf(stderr, "[%s] " x, __func__)

/*
 * Header includes.
 * We only really need some parts of the stdlib.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>
#include <limits.h>
#include <db.h>
#include <fcntl.h>

/*
 * Since we are hashing the word values to store them in the tree, we will need to utilize open hashing to keep track of words with the same hash.
 * This leads us to create a 2D linked list: One axis is the target word, the other is which files reference it.
 */

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

/*
 * The tree structure is pretty straightforward.
 * Each node has a hash value and a list of words which hashed to that value.
 * Each word has a list of file references.
 */

typedef struct isr3_tree_node isr3_tree_node;

struct isr3_tree_node {
	isr3_tree_node* left, *right;
	isr3_word_entry* word_list; // List of words corresponding to node_hash[].
	char node_hash[ISR3_HASH_LENGTH];
};

/* Program function declarations. */

int parse_file(const char* filename, unsigned int ref_id, isr3_tree_node** root, isr3_word_entry** global_list, int* largest_word_length); /* Parse a file into the tree. */
int insert_word(char* word_buf, int word_len, unsigned int ref_id, isr3_tree_node** root, isr3_word_entry** global_list); /* Insert a word into the tree. */
isr3_word_entry* sort_list(isr3_word_entry* word_list);
void free_tree(isr3_tree_node* root);

void gen_permuterm(isr3_word_entry* entry, DB* tree); /* For each permutation of the word, insert a permuterm key pointing to "entry" into a btree. */
void search_permuterm(char* query, int query_len, DB* tree, void (*callback)(isr3_word_entry* list));

/* Utility functions : hashing and comparing words. */

int hash_word(char* word_buf, int word_len, char* out_buf, int out_len); /* Hash a word into a buffer. */
int word_cmp(char* word_buf1, int word_len1, char* word_buf2, int word_len2); /* Returns 1 if word1 > word2, -1 if word1 < word2, and 0 if word1 = word2. */

/* Internal sorting functions. */

isr3_word_entry* merge_nodes(isr3_word_entry* first, isr3_word_entry* second);
int divide_list(isr3_word_entry* head, isr3_word_entry** first, isr3_word_entry** second);
int list_length(isr3_word_entry* head);

/* Stemmer function declarations. */

extern int stem(char* c, int i, int j);

/* Function definitions. */

int main(int argc, char** argv) {
	isr3_debug("Starting ISR3.\n");
	isr3_tree_node* root = NULL;
	isr3_word_entry* word_list_g = NULL;

	DB* perm_btree = dbopen(ISR3_BTREE, 0, O_RDWR, DB_BTREE, NULL);

	if (!perm_btree) {
		isr3_errf("Failed to create B-tree at disk location [%s]\n", ISR3_BTREE);
		return 1;
	}

	int largest_word = 0;

	if (argc > 1) {
		for (int i = 1; i < argc; ++i) {
			isr3_debugf("Parsing input file %s..\n", argv[i]);

			if (!parse_file(argv[i], i - 1, &root, &word_list_g, &largest_word)) { /* We just pass `i - 1` as the reference ID. Makes it very easy to ID files in order. */
				isr3_errf("Parsing failed for file [%s].\n", argv[i]);
				return 1;
			}
		}
	} else {
		isr3_err("No files passed to program.\n");
		isr3_errf("Usage: %s <file1> <file2> <fileN>\n", argv[0]);
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

	/* We insert the sorted wordlist into the on-disk B-tree -- I'm not yet sure if sorting the list is even necessary now. */

	while (word_list_g) {
		fprintf(stdout, "%.*s", word_list_g->word_len, word_list_g->word);

		for (int i = 0; i < (largest_word - word_list_g->word_len) + 1; ++i) { /* Extra space to accomadate the column seperator. */
			fputc(' ', stdout);
		}

		isr3_ref_entry* cur_ref = word_list_g->ref_list_head;

		while (cur_ref) {
			fprintf(stdout, "%02d ", cur_ref->ref_id + 1);
			cur_ref = cur_ref->next;
		}

		fputc('\n', stdout);
		gen_permuterm(word_list_g, perm_btree);

		word_list_g = word_list_g->global_next;
	}

	free_tree(root); /* We cleanly exit, returning all memory to the OS. */
	perm_btree->close(perm_btree);

	/* To prevent bad things from happening at next runtime, we destroy the btree. */
	if (remove(ISR3_BTREE)) {
		isr3_err("Failed to remove B-tree file from disk. This could interfere with the next execution.\n");
	}

	return 0;
}

int parse_file(const char* filename, unsigned int ref_id, isr3_tree_node** root, isr3_word_entry** global_list, int* largest_word_length) {
	FILE* fd = fopen(filename, "r");

	if (!fd) {
		isr3_errf("Failed to open [%s] for reading.\n", filename);
		return 0;
	}

	char* cur_word = NULL;
	int cur_word_len = 0; /* Instead of using strlen, we just keep a counter. Much faster. */

	char cur_char = 0; /* This stores the most recent character read to make things more readable. */

	/*
	 * This may get a bit complicated - reading strings from a file into a dynamically allocated string (and then stemming it) requires much more code.
	 * Also, we'll have to free all of the word data when we're done.
	 */

	while (!feof(fd)) {
		/* First: get rid of any preceding whitespace (but store the last character in the first index of cur_word) */
		while (isspace(cur_char = fgetc(fd)));

		if (cur_char == EOF) {
			break; /* Depending on the file, an EOF may occur here if there is no whitespace between the last word and the EOF. */
		}

		cur_word = malloc(sizeof *cur_word * 2);

		if (!cur_word) {
			isr3_err("Failed to allocate memory. Is there any RAM left?\n");
			return 0;
		}

		cur_word[0] = cur_char;
		cur_word[1] = 0;
		cur_word_len = 1; /* The character which terminated the last loop is the first character of the target word. */

		/* Next: read until the next whitespace character. */
		while (!isspace(cur_char = fgetc(fd)) && !feof(fd)) {
			if (cur_char == '\'') {
				continue; /* This seems to be the most effective method of handling punctuation (simply removing it from the word prior to stemming) [subject to change] */
			}

			if (!isalnum(cur_char)) {
				break; /* Break on non-alphanumeric characters. */
			}

			cur_word = realloc(cur_word, cur_word_len + 2);

			if (!cur_word) {
				isr3_err("Failed to allocate memory. Is there any RAM left?\n");
				return 0;
			}

			cur_word[cur_word_len++] = cur_char; /* If this were in the while condition, it would increment even when fgetc() returned whitespace. */
			cur_word[cur_word_len] = 0;
		}

		int stem_length = stem(cur_word, 0, cur_word_len - 1) + 1;

		cur_word[stem_length] = 0; /* Null-terminate the stemmed word. */
		isr3_debugf("Read word with length %d [stem %d], data [%.*s]\n", cur_word_len, stem_length, cur_word_len, cur_word);

		cur_word_len = stem_length;

		if (largest_word_length && cur_word_len >= *largest_word_length) {
			 *largest_word_length = cur_word_len;
		}

		if (!insert_word(cur_word, cur_word_len, ref_id, root, global_list)) {
			isr3_err("Failed to insert word into tree.\n");
			return 0;
		}

		cur_word = NULL;
		cur_word_len = 0;
	}

	fclose(fd);
	return 1;
}

int insert_word(char* word_buf, int word_len, unsigned int ref_id, isr3_tree_node** root, isr3_word_entry** global_list) {
	if (!word_buf || !root) {
		isr3_err("Invalid input!\n");
		return 0;
	}

	isr3_ref_entry* new_ref_entry = malloc(sizeof *new_ref_entry);

	if (!new_ref_entry) {
		isr3_err("malloc() failed. System may be out of RAM!\n");
		exit(1);
	}

	new_ref_entry->ref_id = ref_id;
	new_ref_entry->next = NULL;

	/* We will have to hash the word, but we won't necessarily have to create a word entry. */
	char word_hash[ISR3_HASH_LENGTH] = {0};
	hash_word(word_buf, word_len, word_hash, sizeof word_hash / sizeof *word_hash);

	isr3_tree_node** cur_node = root;

	while (*cur_node) {
		int result = memcmp(word_hash, (*cur_node)->node_hash, ISR3_HASH_LENGTH);

		if (result > 0) {
			/* Our hash is larger -> We want to continue search from the right child. */
			cur_node = &(*cur_node)->right;
		} else if (result < 0) {
			cur_node = &(*cur_node)->left;
			/* Our hash is smaller -> We want to continue searching from the left child. */
		} else {
			/* The hashes are equal -> We want to locate the target word in this node. */
			/* This will be a generally quick procedure. We scan through the wordlist and insert our ref_id. */
			isr3_word_entry* cur_word_entry = (*cur_node)->word_list;
			int located = 0; /* We keep a small flag to indicate whether we need to insert a new word entry. */

			while (cur_word_entry) {
				if (!word_cmp(cur_word_entry->word, cur_word_entry->word_len, word_buf, word_len)) {
					/* We found our word. Add our refID and set the located flag. */
					isr3_ref_entry* cur_ref = cur_word_entry->ref_list_head;
					int located_ref = 0;

					while (cur_ref) {
						if (cur_ref->ref_id == ref_id) {
							located_ref = 1;
							break;
						}

						cur_ref = cur_ref->next;
					}

					if (!located_ref) {
						/* Instead of doing a quick two-line linked list insertion, we push it to the end to reverse the output order. */

						new_ref_entry->next = NULL;

						if (cur_word_entry->ref_list_tail) {
							cur_word_entry->ref_list_tail->next = new_ref_entry;
						}

						if (!cur_word_entry->ref_list_head) {
							cur_word_entry->ref_list_head = new_ref_entry;
						}

						cur_word_entry->ref_list_tail = new_ref_entry;
					}

					located = 1;

					free(word_buf); /* Our word already exists. We free the memory which was automatically allocated by the file parser. */
				}

				cur_word_entry = cur_word_entry->next;
			}

			if (!located) {
				/* We didn't find our word in the entry list. Add a new one! */
				isr3_word_entry* new_entry = malloc(sizeof *new_entry);

				if (!new_entry) {
					isr3_err("malloc() failed. System may be out of RAM!\n");
					exit(1);
				}

				new_entry->word = word_buf;
				new_entry->word_len = word_len;

				new_entry->ref_list_head = new_entry->ref_list_tail = new_ref_entry;
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

	isr3_debug("No node found. Inserting new node..\n");

	isr3_tree_node* new_node = malloc(sizeof *new_node);

	if (!new_node) {
		isr3_err("malloc() failed. System may be out of RAM!\n");
		exit(1);
	}

	memcpy(new_node->node_hash, word_hash, ISR3_HASH_LENGTH);
	new_node->left = new_node->right = NULL;

	isr3_word_entry* new_entry = malloc(sizeof *new_entry);

	if (!new_entry) {
		isr3_err("malloc() failed. System may be out of RAM!\n");
		exit(1);
	}

	new_entry->word = word_buf;
	new_entry->word_len = word_len;

	isr3_debugf("Inserted new node [%.*s]\n", new_entry->word_len, new_entry->word);

	new_entry->ref_list_head = new_entry->ref_list_tail = new_ref_entry;
	new_entry->next = new_node->word_list;

	new_node->word_list = new_entry;
	new_entry->global_next = *global_list;

	*global_list = new_entry;
	*cur_node = new_node;
	return 1;
}

int hash_word(char* word_buf, int word_len, char* hash_buf, int hash_len) {
	/* hash_word implements the SDBM hash algorithm, a small and fast hashing algorithm with an emphasis on performance and minimizing collisions. */

	if (hash_len != ISR3_HASH_LENGTH) {
		isr3_errf("Hash output buffer is incorrect size. ISR3 hash length is %d, but buffer is %d.\n", ISR3_HASH_LENGTH, hash_len);
		return 0;
	}

	if (!hash_buf || !word_buf || !word_len) {
		isr3_err("Invalid input to hash procedure.\n");
		return 0;
	}

	if (ISR3_HASH_LENGTH != 4) {
		isr3_err("SDBM must operate with a hash length of 4 bytes.\n");
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

void free_tree(isr3_tree_node* root) {
	/* Quick recursive cleanup of the tree and everything allocated inside. */

	if (!root) {
		return;
	}

	free_tree(root->left);
	free_tree(root->right);

	isr3_word_entry* cur_word = root->word_list, *tmp_word = NULL;

	while (cur_word) {
		isr3_ref_entry* cur_ref = cur_word->ref_list_head, *tmp_ref = NULL;

		while (cur_ref) {
			tmp_ref = cur_ref->next;
			free(cur_ref);
			cur_ref = tmp_ref;
		}

		tmp_word = cur_word->next;
		free(cur_word->word);
		free(cur_word);
		cur_word = tmp_word;
	}

	free(root);
}

isr3_word_entry* sort_list(isr3_word_entry* head) {
	isr3_word_entry* first = NULL, *second = NULL;

	if (!head || !head->global_next) {
		return head;
	}

	divide_list(head, &first, &second);

	first = sort_list(first);
	second = sort_list(second);

	/* Merge operation.. with linked lists! */
	return merge_nodes(first, second);
}

isr3_word_entry* merge_nodes(isr3_word_entry* first, isr3_word_entry* second) {
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

int divide_list(isr3_word_entry* head, isr3_word_entry** first, isr3_word_entry** second) {
	if (!head || !head->global_next) {
		*first = head;
		(*first)->global_next = NULL;
		*second = NULL;

		isr3_debug("List length is <2, returning single element\n");

		return 1;
	}

	isr3_word_entry* slow = head, *fast = head->global_next;

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

void gen_permuterm(isr3_word_entry* entry, DB* tree) {
	/* To permute the word, we have to use the string kind of like a circular buffer with only two memcpy calls. */
	/* This is a pretty quick and easy way to do it. */

	int inp_wordlen = entry->word_len + 2; /* Make room for the '$' and a null terminator (req. for the btree key) */
	char* permbuf = malloc(inp_wordlen), *inp_word = malloc(inp_wordlen);

	if (!permbuf || !inp_word) {
		isr3_err("malloc failure\n");
		return;
	}

	memcpy(inp_word, entry->word, entry->word_len);

	inp_word[inp_wordlen - 2] = '$';
	inp_word[inp_wordlen - 1] = permbuf[inp_wordlen - 1] = 0;

	for (int i = 0; i < inp_wordlen; ++i) {
		memcpy(permbuf, inp_word + i, inp_wordlen - i);
		memcpy(permbuf + inp_wordlen - i, inp_word, i);

		isr3_debugf("Permuterm %d of [%.*s] : [%.*s]\n", i, inp_wordlen, inp_word, inp_wordlen, permbuf);
		btree_add(tree, (unsigned char*) permbuf, (unsigned char*) entry, sizeof entry, 1);

		DBT key = {0}, data = {0};

		key.data = permbuf;
		key.size = inp_wordlen;

		data.data = entry;
		data.size = sizeof entry;

		tree->put(tree, &key, &data, 0);
	}

	free(permbuf);
	free(inp_word);
}

void search_permuterm(char* query, int len, DB* tree, void (*callback)(struct isr3_word_entry* entry)) {
	/* Most of the work here will be spent transforming the query into a permuterm query (which isn't too hard anyway) */
}
