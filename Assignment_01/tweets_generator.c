//#define DEBUG

#include "markov_chain.h"
//include stream for file
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINE_LENGTH 1000
#define DELIMITERS " \n\t\r"
#define FILE_PATH_ERROR "Error: incorrect file path\n"
#define NUM_ARGS_ERROR "Usage: invalid number of arguments\n"

/**
 * Builds a Markov chain from input file
 */
MarkovChain* build_markov_chain(FILE* fp, int words_to_read) {
    // Create chain
    MarkovChain* chain = malloc(sizeof(MarkovChain));
    if (!chain) {
        printf(ALLOCATION_ERROR_MASSAGE);
        return NULL;
    }

    chain->database = malloc(sizeof(LinkedList));
    if (!chain->database) {
        printf(ALLOCATION_ERROR_MASSAGE);
        free(chain);
        return NULL;
    }
    chain->database->first = NULL;
    chain->database->last = NULL;
    chain->database->size = 0;

    char line[MAX_LINE_LENGTH];
    int words_read = 0;
    Node* prev_node = NULL;

    // Read lines until EOF or word limit reached
    while (fgets(line, MAX_LINE_LENGTH, fp) != NULL &&
           (words_to_read == -1 || words_read < words_to_read)) {

        char* word = strtok(line, DELIMITERS);
        while (word != NULL && (words_to_read == -1 || words_read < words_to_read)) {
            if (strlen(word) == 0) {
                word = strtok(NULL, DELIMITERS);
                continue;
            }

            // Add word to database
            Node* current = add_to_database(chain, word);
            if (!current) {
#ifdef DEBUG
                printf("Failed to add word: %s\n", word);
#endif
                free_database(&chain);
                return NULL;
            }
#ifdef DEBUG
            printf("Added word: %s\n", word);
#endif
            words_read++;

            // Add to frequency list of previous word
            if (prev_node) {
                MarkovNode* prev_markov = (MarkovNode*)prev_node->data;
                MarkovNode* curr_markov = (MarkovNode*)current->data;
                if (add_node_to_frequencies_list(prev_markov, curr_markov) != 0) {
#ifdef DEBUG
                    printf("Failed to add to frequency list: %s -> %s\n",
                           prev_markov->data, curr_markov->data);
#endif
                    free_database(&chain);
                    return NULL;
                }
            }

            prev_node = current;
            word = strtok(NULL, DELIMITERS);
        }
    }

#ifdef DEBUG
    printf("Total words read: %d\n", words_read);
#endif
    return chain;
}

/**
 * Main function
 */
int main(int argc, char *argv[]) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
#ifdef DEBUG
        printf("Current working directory: %s\n", cwd);
#endif
    } else {
        perror("getcwd() error");
        return EXIT_FAILURE;
    }

    // Parse arguments
    if (argc <= 3 || argc > 5) {
        printf(NUM_ARGS_ERROR);
        return EXIT_FAILURE;
    }

    unsigned int seed = (int)strtol(argv[1], NULL, 10);
    int tweets_count = (int)strtol(argv[2], NULL, 10);
    char* path = argv[3];
    int words_to_read = (argv[4] != NULL) ? (int)strtol(argv[4], NULL, 10) : -1;

#ifdef DEBUG
    printf("Arguments parsed: tweets_count=%d, path=%s, words_to_read=%d\n",
           tweets_count, path, words_to_read);
#endif

    // Seed random
    srand(seed);

    // Open file
    FILE *fp = fopen(path, "r");
    if (!fp) {
        printf(FILE_PATH_ERROR);
        exit(EXIT_FAILURE);
    }
#ifdef DEBUG
    printf("File opened successfully: %s\n", path);
#endif

    // Build chain
    MarkovChain* chain = build_markov_chain(fp, words_to_read);
    fclose(fp);
    if (!chain) {
        printf("Failed to build Markov chain.\n");
        exit(EXIT_FAILURE);
    }
#ifdef DEBUG
    printf("Markov chain built successfully.\n");
#endif

    // Generate tweets
    for (int i = 1; i <= tweets_count; i++) {
        MarkovNode* first = get_first_random_node(chain);
        if (!first || !first->data) {
#ifdef DEBUG
            printf("Failed to get valid first node for tweet %d.\n", i);
#endif
            continue;
        }

        // Create a temporary file to capture the output
        FILE* temp = tmpfile();
        if (!temp) {
            continue;
        }

        // Try to generate the tweet
        int result = generate_tweet(first, chain, temp);

        // If tweet generation was successful
        if (result > 0) {
            // Print tweet number and the generated tweet
            printf("Tweet %d: ", i);

            // Rewind temp file and copy its contents to stdout
            rewind(temp);
            char buffer[MAX_LINE_LENGTH];
            if (fgets(buffer, MAX_LINE_LENGTH, temp) != NULL) {
                printf("%s", buffer);
            }
        }
#ifdef DEBUG
        else {
            printf("Failed to generate valid tweet %d.\n", i);
        }
#endif

        fclose(temp);
    }

    // Cleanup
    free_database(&chain);
#ifdef DEBUG
    printf("Cleanup done.\n");
#endif
    return EXIT_SUCCESS;
}