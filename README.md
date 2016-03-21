# Justin Stanley
# Information Storage and Retrieval
## Project 3

To compile this project, execute `make`. To clean object files and binaries, execute `make cleanbin`.
This project should compile without warnings on GCC 5.3.0+.

This program uses the "otree" implementation of an on-disk B-tree which can be found here: https://github.com/antirez/otree

The B-tree implementation this program uses stores the tree data on the disk (as expected for a structure usually intended for I/O manipulation and search indices)
This is also optimal for the program since permuterm trees are huge for documents with diverse terms.

As a result, the program is a bit slower as B-tree searches utilize the disk instead of RAM.

I added some arguments which the program can accept to optimize the behavior:
	[no flags] => Program will construct a B-tree with the filenames provided, prompt for a search, output results, and then delete the B-tree from the disk and exit.
	(-c | --construct) => Program will construct a B-tree with the filenames provided and then exit.
	(-s | --search) => Program will prompt for a search, and then output results from the B-tree on disk.
	(-d | --delete) => Program will delete the B-tree on disk.

These flags allow for more efficient operation so the B-tree doesn't necessarily have to be regenerated every time the program is run.
