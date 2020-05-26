#include "stdc++.h"

// struct for storing data in DT.
typedef struct DTEntry {
    int file_id;
    int starting_index;
    int size; // in terms of number of occupied blocks.
    int bytes;
}dtentry_t;


// ============== CONTIGUOUS ALLOCATION HELPER FUNCTIONS ================== //

// returns the first block index of the first fit in directory content.
int find_first_fit(int required_block);
// checks if the contiguous space is enough to place a file
bool seek(int starting_position, int required_block);
// returns the required amount of blocks for a given size.
int byte_to_block(int bytes);
// shifts left the all files.
void defragment_all();
// moves a file block by block to the given new starting index.
int move_a_file(dtentry_t file, int new_start);



// ===================== DESIRED FUNCTIONALITIES ==================== //
int create(int file_id, int bytes);
int access(int file_id, int offset);
int extend(int file_id, int extension);
int shrink(int file_id, int shrinking);
