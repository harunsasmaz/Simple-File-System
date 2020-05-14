// struct for storing data in DT.
typedef struct DTEntry {
    int file_id;
    int starting_index;
    int size; // in terms of number of occupied blocks.
}dtentry_t;

// returns the first block index of the first fit in directory content.
int find_first_fit(int required_block);
// checks if the contiguous space is enough to place a file
bool seek(int starting_position, int required_block);
// returns the id of the next file in a given range.
int find_next_file(int starting_position, int end_position);
// returns the required amount of blocks for a given size.
int byte_to_block(int bytes);
// fills the directory content array with the unique id of the file.
int fill_directory_content(dtentry_t entry);
// shifts left the all files.
int defragment();
// shifts a file to left as much as it can, and returns the new starting index of file.
int move_file_to_left(dtentry_t file);


// required functions
int create_file(int file_id, int size);
int access(int file_id, int offset);
int extend(int file_id, int extension);
int shrink(int file_id, int shrinking);
