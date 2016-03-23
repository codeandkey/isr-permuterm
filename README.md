# Justin Stanley
# Information Storage and Retrieval
## Project 3

To compile this project, execute `make`. To clean object files and binaries, execute `make cleanbin`.
This project should compile without warnings on GCC 5.3.0+.

This program uses a handwritten implementation of a B-tree, which stores all of the nodes in RAM.
As a result, memory could become a big problem with a large document collection due to the growth rate of a permuterm index.

The program supports search queries with a maximum two wildcards per term.
Any search query without a wildcard is passed directly to the search procedure, so a permuterm query can be manually written.

The program uses a "counter" method to implement conjunctivity between the searches. Each document entry has a counter indicating the number of unique searches which matched a term in the document. After all searches have been processed, only the documents with the counter equal to the number of searches passed every search.
