//#define DEBUG
#include "markov_chain.h"

/**
 * Get random number between 0 and max_number [0, max_number)
 * @param max_number maximal number to return
 * @return Random number
 */
int get_random_number(int max_number) {
    return rand() % max_number;;
}

/**
 * Check if data_ptr is in database
 */
Node* get_node_from_database(MarkovChain *markov_chain, char *data_ptr) {
    if (!markov_chain || !data_ptr) {
        return NULL;
    }

    Node *current = markov_chain->database->first;
    while (current != NULL) {
        if (strcmp(((MarkovNode*)current->data)->data, data_ptr) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

/**
 * Add node to database if not exists
 */
Node* add_to_database(MarkovChain *markov_chain, char *data_ptr) {
    if (!markov_chain || !data_ptr) {
        return NULL;
    }

    // Check if already exists
    Node *existing = get_node_from_database(markov_chain, data_ptr);
    if (existing != NULL) {
        return existing;
    }

    // Create new MarkovNode
    MarkovNode *new_markov_node = malloc(sizeof(MarkovNode));
    if (!new_markov_node) {
        printf(ALLOCATION_ERROR_MASSAGE);
        return NULL;
    }

    // Copy the word
    new_markov_node->data = malloc(strlen(data_ptr) + 1);
    if (!new_markov_node->data) {
        printf(ALLOCATION_ERROR_MASSAGE);
        free(new_markov_node);
        return NULL;
    }
    strcpy(new_markov_node->data, data_ptr);
#ifdef DEBUG
    printf("Added to database: '%s', length: %lu\n", new_markov_node->data,
           strlen(new_markov_node->data));
#endif
    // Initialize other fields
    new_markov_node->frequency_list = NULL;
    new_markov_node->frequency_list_size = 0;
    new_markov_node->total_frequency = 0;
    new_markov_node->is_last = (data_ptr[strlen(data_ptr)-1] == '.');

    // Add to database
    if (add(markov_chain->database, new_markov_node) != 0) {
        free(new_markov_node->data);
        free(new_markov_node);
        return NULL;
    }

    return markov_chain->database->last;
}

/**
 * Add second node to first node's frequency list
 */
int add_node_to_frequencies_list(MarkovNode *first_node, MarkovNode *second_node) {
    if (!first_node || !second_node) {
        return 1;
    }

    // Check if second_node already in frequency list
    for (int i = 0; i < first_node->frequency_list_size; i++) {
        if (first_node->frequency_list[i].markov_node == second_node) {
            first_node->frequency_list[i].frequency++;
            first_node->total_frequency++;
            return 0;
        }
    }

    // Need to add new frequency node
    MarkovNodeFrequency *new_list = realloc(first_node->frequency_list,
                                            (first_node->frequency_list_size + 1) *
                                            sizeof(MarkovNodeFrequency));
    if (!new_list) {
        printf(ALLOCATION_ERROR_MASSAGE);
        return 1;
    }

    // Add the new frequency node
    first_node->frequency_list = new_list;
    first_node->frequency_list[first_node->frequency_list_size].markov_node = second_node;
    first_node->frequency_list[first_node->frequency_list_size].frequency = 1;
    first_node->frequency_list_size++;
    first_node->total_frequency++;

    return 0;
}

/**
 * Free all allocated memory
 */
void free_database(MarkovChain **ptr_chain) {
    if (!ptr_chain || !(*ptr_chain)) {
        return;
    }

    MarkovChain *chain = *ptr_chain;
    Node *current = chain->database->first;

    while (current != NULL) {
        Node *next = current->next;  // Save next pointer before freeing current
        MarkovNode *node = (MarkovNode*)current->data;
        free(node->data);
        free(node->frequency_list);
        free(node);
        free(current);  // Add this line to free the Node structure
        current = next;
    }

    free(chain->database);
    free(chain);
    *ptr_chain = NULL;
}

/**
 * Get random first node that isn't a sentence ending
 */
MarkovNode* get_first_random_node(MarkovChain *markov_chain) {
    if (!markov_chain || !markov_chain->database ||
        !markov_chain->database->first || markov_chain->database->size == 0) {
#ifdef DEBUG
        printf("Invalid chain or database\n");
#endif
        return NULL;
    }

#ifdef DEBUG
    printf("Database size: %d\n", markov_chain->database->size);
#endif

    int max_attempts = 1000;
    while (max_attempts-- > 0) {
        int index = get_random_number(markov_chain->database->size);
        Node *current = markov_chain->database->first;

#ifdef DEBUG
        printf("Trying index: %d\n", index);
#endif

        for (int i = 0; i < index && current != NULL; i++) {
            current = current->next;
        }

        if (!current) {
#ifdef DEBUG
            printf("Null node at index %d\n", index);
#endif
            continue;
        }

        if (!current->data) {
#ifdef DEBUG
            printf("Null data at index %d\n", index);
#endif
            continue;
        }

        MarkovNode *node = (MarkovNode*)current->data;
        if (!node->data) {
#ifdef DEBUG
            printf("Null string at index %d\n", index);
#endif
            continue;
        }

        if (!node->is_last) {
#ifdef DEBUG
            printf("Found valid node at index %d with data: %s\n", index, node->data);
#endif
            return node;
        }
    }
#ifdef DEBUG
    printf("Failed to find valid node after %d attempts\n", 1000);
#endif
    return NULL;
}

/**
 * Get random next node based on frequencies
 */
MarkovNode* get_next_random_node(MarkovNode *state_struct_ptr) {
    if (!state_struct_ptr) {
#ifdef DEBUG
        printf("get_next_random_node: null state_struct_ptr\n");
#endif
        return NULL;
    }

    if (!state_struct_ptr->frequency_list) {
#ifdef DEBUG
        printf("get_next_random_node: node '%s' has null frequency_list\n",
               state_struct_ptr->data);
#endif
        return NULL;
    }

#ifdef DEBUG
    printf("get_next_random_node: current word '%s' has %d frequencies, total %d\n",
           state_struct_ptr->data,
           state_struct_ptr->frequency_list_size,
           state_struct_ptr->total_frequency);
#endif

    int r = get_random_number(state_struct_ptr->total_frequency);
#ifdef DEBUG
    printf("Random number: %d\n", r);
#endif

    int count = 0;
    for (int i = 0; i < state_struct_ptr->frequency_list_size; i++) {
        if (!state_struct_ptr->frequency_list[i].markov_node) {
#ifdef DEBUG
            printf("Null markov_node at frequency index %d\n", i);
#endif
            continue;
        }
        count += state_struct_ptr->frequency_list[i].frequency;
        if (r < count) {
#ifdef DEBUG
            printf("Selected next word: %s\n",
                   state_struct_ptr->frequency_list[i].markov_node->data);
#endif
            return state_struct_ptr->frequency_list[i].markov_node;
        }
    }

#ifdef DEBUG
    printf("Warning: fell through to default case\n");
#endif
    if (state_struct_ptr->frequency_list_size > 0) {
        return state_struct_ptr->frequency_list[0].markov_node;
    }
    return NULL;
}

/**
 * Generate a random tweet
 */
int generate_tweet(MarkovNode *first_node, MarkovChain *markov_chain, FILE* out) {
    if (!first_node || !markov_chain || !out) {
#ifdef DEBUG
        printf("generate_tweet: null parameters\n");
#endif
        return -1;
    }

    if (!first_node->data) {
#ifdef DEBUG
        printf("generate_tweet: first node has null data\n");
#endif
        return -1;
    }

    int words = 0;
    MarkovNode *current = first_node;

    // Store words temporarily in case we need to discard them
    char tweet[MAX_TWEET_LENGTH * 100] = ""; // Buffer for storing the tweet
    char temp[100]; // Temporary buffer for each word

    // Add first word
    strcpy(tweet, current->data);
    words++;

    // Generate rest of tweet
    while (words < MAX_TWEET_LENGTH) {
        // Get next word
        MarkovNode *next = get_next_random_node(current);
        if (!next) {
#ifdef DEBUG
            printf("No next word found after '%s'\n", current->data);
#endif
            break;
        }

        // Add space and word to temporary tweet
        sprintf(temp, " %s", next->data);
        strcat(tweet, temp);
        words++;

        current = next;

        // If we hit end of sentence (period)
        if (current->is_last) {
            // Only print if we have at least 2 words
            if (words >= 2) {
                fprintf(out, "%s\n", tweet);
                return words;
            }
            // If less than 2 words, continue generating
            continue;
        }

        // If we hit max length without finding a period, print anyway
        if (words >= MAX_TWEET_LENGTH) {
            fprintf(out, "%s\n", tweet);
            return words;
        }
    }

    // If we get here without printing, it means we didn't find a proper ending
    // Try to generate a new tweet instead
//    MarkovNode* new_first = get_first_random_node(markov_chain);
//    if (new_first) {
//        return generate_tweet(new_first, markov_chain, out);
//    }

    return -1;
}