# Markov Chain Tweet Generator

This project implements a Markov chain-based text generator that creates random "tweets" based on input text. The generator analyzes input text to build a probabilistic model of word sequences and uses this model to generate new, random text that mimics the style of the input.

## Core Components

### MarkovChain Structure
The system uses several key data structures:
- `MarkovChain`: Contains the main database of words and their relationships
- `MarkovNode`: Represents a single word and stores its transition probabilities
- `MarkovNodeFrequency`: Tracks how often words appear after each other
- `LinkedList`: Provides the underlying data storage mechanism

### Main Files

#### markov_chain.h
Defines the core data structures and interfaces for the Markov chain implementation:
- Data structures for the chain, nodes, and frequency tracking
- Function declarations for chain manipulation and text generation
- Constants including maximum tweet length (20 words)

#### markov_chain.c
Implements the core Markov chain functionality:
- Random node selection based on frequency distributions
- Database management for word storage and retrieval
- Memory management for the chain structure
- Tweet generation logic ensuring proper sentence structure

#### tweets_generator.c
Provides the main program interface:
- Reads and processes input text files
- Builds the Markov chain from input
- Generates specified number of tweets
- Handles command-line arguments and file I/O

## Usage

```bash
./tweets_generator <seed> <number_of_tweets> <input_file> [words_to_read]
```

Arguments:
- `seed`: Random seed for reproducible results
- `number_of_tweets`: Number of tweets to generate
- `input_file`: Path to the source text file
- `words_to_read`: (Optional) Maximum number of words to read from input

## Features

- Probabilistic text generation based on input patterns
- Ensures generated tweets are between 2-20 words
- Proper sentence termination handling
- Memory-efficient linked list implementation
- Error handling for file operations and memory allocation
- Debug mode available (can be enabled by uncommenting `#define DEBUG`)

## Technical Details

### Memory Management
- Dynamic allocation for all data structures
- Proper cleanup of all allocated memory
- Frequency lists are reallocated as needed

### Word Processing
- Words are tokenized using space, newline, tab, and return as delimiters
- End-of-sentence detection based on period character
- Maintains word transition frequencies for natural-sounding output

### Random Generation
- Uses weighted random selection based on observed frequencies
- Ensures generated text maintains statistical properties of input
- Handles edge cases for first and last words

## Error Handling

The program includes comprehensive error checking for:
- File operations
- Memory allocation
- Invalid input parameters
- Chain generation failures
- Tweet generation failures

## Limitations

- Maximum tweet length is fixed at 20 words
- Only considers periods as sentence terminators
- Requires properly formatted input text
- Memory usage scales with input size

## Dependencies

- Standard C libraries: stdio.h, stdlib.h, string.h
- Custom linked list implementation (linked_list.h)
