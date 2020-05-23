#include "helper.hpp"

using namespace std;
#define NUM_BLOCKS 32768

// IMPORTANT NOTICE:
// Free blocks in the directory contents are represented as -1.
// Occupied blocks are represented with the IDs of files.

int BLOCK_SIZE = 1024;
int directory_contents[NUM_BLOCKS]; // file directory, -1 represents free blocks.
int available_blocks = NUM_BLOCKS;  // available block count holder
int f_id = 0;                       // global file id counter.
map<int, dtentry_t> DT;             // directory table map

string input_file;
int main(int argc, char** argv){

    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    int c_count = 0, e_count = 0, a_count = 0, sh_count = 0;
    int c_reject = 0, e_reject = 0, a_reject = 0, sh_reject = 0;

    // fill directory contents with -1.
    std::fill(directory_contents, directory_contents + NUM_BLOCKS, -1);

    // get input file from user and parse it to obtain block size;
    input_file = argv[1];
    stringstream parser(input_file);
    string block_sz;
    getline(parser, block_sz, '_');
    getline(parser, block_sz, '_');
    BLOCK_SIZE = stoi(block_sz);

    // open input file for reading
    ifstream commands(input_file);
    // strings for parsing by ':'
    string line, operation, id, offset;
    int ret_code;

    time_t start = time(NULL);
    while(getline(commands, line)){
        stringstream ss(line);
        getline(ss, operation, ':');
        if(operation == "c"){

            getline(ss, offset, ':');
            int bytes = stoi(offset);
            ret_code = create(f_id, bytes);
            c_count++;
            if(ret_code == -1) c_reject++;
            if(ret_code == 0) f_id++;

        } else {
            // these fields are common for three operations
            getline(ss, id, ':');
            getline(ss, offset, ':');
            int id_int = stoi(id);
            int offset_int = stoi(offset);

            if(operation == "a"){

                ret_code = access(id_int, offset_int);
                a_count++;
                if(ret_code == -1) a_reject++;

            } else if(operation == "sh"){

                ret_code = shrink(id_int, offset_int);
                sh_count++;
                if(ret_code == -1) sh_reject++;

            } else if(operation == "e"){

                ret_code = extend(id_int, offset_int);
                e_count++;
                if(ret_code == -1) e_reject++;
            }
        }
    }
    time_t end = time(NULL);

    double time_taken = double(end - start); 
    cout << "Time taken by program is : " << fixed 
         << time_taken << setprecision(5); 
    cout << " sec " << endl; 

    cout << "Total create: " << c_count << "\tReject create: " << c_reject << endl;
    cout << "Total extend: " << e_count << "\tReject extend: " << e_reject << endl;
    cout << "Total shrink: " << sh_count << "\tReject shrink: " << sh_reject << endl;
    cout << "Total access: " << a_count << "\tReject access: " << a_reject << endl;

    return 0;
}

// conversion from bytes to required amount of blocks.
int byte_to_block(int bytes){
    return ceil(static_cast<float>(bytes) / static_cast<float>(BLOCK_SIZE));
}

// returns the id of the next file in given range.
int find_next_file(int starting_position, int end_position){
    int id = -1;
    // iterative search in the given range.
    for(int i = starting_position; i < end_position; i++){
        if(directory_contents[i] != -1){
            id = directory_contents[i];
            break;
        }
    }
    return id;
}

// search a space to allocate a file
int find_first_fit(int required_block){
    int start = 0;
    bool check;
    while(start < NUM_BLOCKS - required_block + 1){
        // check if there is enough contiguous space
        check = seek(start, required_block);
        //if yes, then we found starting index
        if(check) break;
        // if not, update the start index of this file, to the end of the next file.
        int id = find_next_file(start, start + required_block);
        start = DT[id].starting_index + DT[id].size;
    }
    return check ? start : -1;
}

// search if there is enough contiguous space in given range.
bool seek(int starting_position, int required_block){
    for(int i = starting_position; i < starting_position + required_block; i++){
        if(directory_contents[i] != -1) return false;
    }
    return true;
}

// shifts a file as much as it can, and returns the new starting index of file.
// Here, extra buffer block (temp) is used.
int defragment_single(dtentry_t file){
    
    int first = file.starting_index, size = file.size, id = file.file_id;
    int start = first - 1;
    // shift as much as it is possible
    while(start != -1 && directory_contents[start] == -1){
        // we shift a file block by block, so swap every block in the file
        // one by one until it is shifted left by 1.
        for(int i = 0; i < size; i++){
            int temp = directory_contents[start + i];
            directory_contents[start + i] = directory_contents[first + i];
            directory_contents[first + i] = temp;
        }
        // shift heads left by 1
        first--;
        start--;
    }
    return first;
}

// for all the files in DT, shift them left. 
void defragment_all(){

    int start = 0, end = NUM_BLOCKS;
    dtentry_t file;
    // we are supposed to defragment DT.size() amount of files.
    for(int i = 0; i < DT.size(); i++){
        // find the next file to be shifted.
        int id = find_next_file(start, end);
        file = DT[id];
        // shift that file as much as possible.
        int new_starting = defragment_single(file);
        // update the starting index of the file.
        DT[id].starting_index = new_starting;
        // jump after this file to search next file.
        start += file.size;
    }
}

// move a file to another place block by block.
// Here, extra buffer block (temp) is used.
int move_a_file(dtentry_t file, int new_start){
    int start = file.starting_index, size = file.size;
    int id = file.file_id;
    // carry each block from starting index block by block.
    for(int i = 0; i < size; i++){
        // -1 means that block is freed.
        directory_contents[start + i] = -1;
        directory_contents[new_start + i] = id;
    }
    // update the new starting index of file.
    DT[id].starting_index = new_start;
    return 0;
}

// returns the block index that contains the specified byte of the file.
int access(int file_id, int offset){
    dtentry_t file = DT[file_id];
    // if no file or offset is too high, return -1.
    if(file.size == 0) return -1;
    if(offset > file.bytes) return -1;
    // return the block index from starting index of file.
    return file.starting_index + offset/BLOCK_SIZE;
}

// extends a file by given amount, on fail returns -1.
int extend(int file_id, int extension){

    // if there is no available blocks at all, reject.
    if(available_blocks < extension) return -1;

    
    dtentry_t file = DT[file_id];
    // if there is no such file, return -1.
    if(file.size == 0) return -1;

    int start = file.starting_index, size = file.size;
    // first check the contiguous blocks whether they are free or not.
    bool check = seek(start + size, extension);
    // if not, first defragment them all and try again.
    if(!check){
        defragment_all();
        // after defragment, check for a place to fit the file with extended size
        int new_start = find_first_fit(size + extension);
        // if no place, reject.
        if(new_start == -1) return -1; 
    
        // if there is a hole to fit the file, then:
        // First, we update the starting index of file to be moved,
        // because after defragment, starting points are changed.
        file.starting_index = DT[file_id].starting_index;
        // Then, move the file to the found hole.
        move_a_file(file, new_start);
        // And extend the file.
        fill(directory_contents + new_start + size, 
                directory_contents + new_start + size + extension, file_id);

    } else {
        // if the contigous space is free, then extend directly.
        fill(directory_contents + start + size, 
                directory_contents + start + size + extension, file_id);
    }
    // update the file size and decrease available blocks.
    DT[file_id].size = size + extension;
    DT[file_id].bytes += extension * BLOCK_SIZE;
    available_blocks -= extension;
    return 0;
}

// creates a file with given size, on fail returns -1.
int create(int file_id, int bytes){

    int required_blocks = byte_to_block(bytes);
    // if there is no enough space, then reject.
    if(available_blocks < required_blocks) return -1;
    

    int starting = find_first_fit(required_blocks);
    // if there is no contiguous place to fit, try again after defragmentation.
    if(starting == -1){
        defragment_all();
        // try again to find a place to fit in.
        starting = find_first_fit(required_blocks);
        // if no space to fit, then reject.
        if(starting == -1) return -1;
    }
    // if we have space to fit, create a file and add it to DT.
    dtentry_t entry;
    entry.file_id = file_id;
    entry.starting_index = starting;
    entry.size = required_blocks;
    entry.bytes = bytes;
    DT[file_id] = entry;
    // fill the directory contents and decrease available blocks.
    fill(directory_contents + starting, directory_contents + starting + required_blocks, file_id);
    available_blocks -= required_blocks;
    
    return 0;
}

// shrinks a file with given amount, on fail returns -1.
int shrink(int file_id, int shrinking){

    dtentry_t file = DT[file_id];
    // if there is no such file, return -1.
    if(file.size == 0) return -1;

    // if offset is more than file size, reject.
    if(shrinking >= file.size) return -1;

    int last = file.starting_index + file.size;
    int first = last - shrinking;
    // otherwise free the space in directory contents.
    fill(directory_contents + first, directory_contents + last, -1);
    
    // decrease file size in DT and decrease available blocks.
    DT[file_id].size -= shrinking;
    DT[file_id].bytes -= shrinking * BLOCK_SIZE;
    available_blocks += shrinking;

    return 0;
}