#include "helper.hpp"

using namespace std;
#define NUM_BLOCKS 32768
#define FREE -1

// IMPORTANT NOTICE:
// Free blocks in the directory contents are represented as -1.
// Occupied blocks are represented with the IDs of files.
// Operations do not rely on the values in directory contents.

int BLOCK_SIZE = 1024;
int directory_contents[NUM_BLOCKS]; // file directory, -1 represents free blocks.
int available_blocks = NUM_BLOCKS;  // available block count holder
int f_id = 0;                       // global file id counter.
map<int, dtentry_t> DT;             // directory table map

// total times for each operation.
int64_t create_t, extend_t, access_t, shrink_t;

string input_file;
int main(int argc, char** argv){

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

    auto start = chrono::steady_clock::now();
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
    auto end = chrono::steady_clock::now();

    int64_t elapsed = chrono::duration_cast<chrono::milliseconds>(end - start).count();
    cout << "Time taken by program is : " << fixed << elapsed << setprecision(5); 
    cout << " milisec " << endl; 
    
    cout << "Total create: " << c_count << "\tReject create: " << c_reject 
        << "\tAverage time: " << (double)create_t/c_count/1000 << endl;
    cout << "Total extend: " << e_count << "\tReject extend: " << e_reject 
        << "\tAverage time: " << (double)extend_t/e_count/1000 << endl;
    cout << "Total shrink: " << sh_count << "\tReject shrink: " << sh_reject 
        << "\tAverage time: " << (double)shrink_t/sh_count/1000 << endl;
    cout << "Total access: " << a_count << "\tReject access: " << a_reject 
        << "\tAverage time: " << (double)access_t/a_count/1000 << endl;

    return 0;
}

// conversion from bytes to required amount of blocks.
int byte_to_block(int bytes){
    return ceil(static_cast<float>(bytes) / static_cast<float>(BLOCK_SIZE));
}

// returns the block index that contains the specified byte of the file.
int access(int file_id, int offset){

    auto start_t = chrono::steady_clock::now();

    dtentry_t file = DT[file_id];
    // if no file or offset is too high, return -1.
    if(file.size == 0) return -1;
    if(offset > file.size) return -1;

    auto end = chrono::steady_clock::now();
    access_t += chrono::duration_cast<chrono::microseconds>(end - start_t).count();

    // return the block index from starting index of file.
    return file.starting_index + offset/BLOCK_SIZE;
}

// extends a file by given amount, on fail returns -1.
int extend(int file_id, int extension){

    auto start_t = chrono::steady_clock::now();

    // if there is no available blocks at all, reject.
    if(available_blocks < extension) return -1;

    dtentry_t file = DT[file_id];
    // if there is no such file, return -1.
    if(file.size == 0) return -1;

    int start = file.starting_index, size = byte_to_block(file.size);
    // first check the contiguous blocks whether they are free or not.
    bool check = seek(start + size, extension);
    // if not;
    if(!check){
        // first search for a space that size + extension fits
        int new_start = find_first_fit(size + extension);
        // if there is no space;
        if(new_start == -1){
            // defragment files and move the file.
            defragment_all();
            file.starting_index = DT[file_id].starting_index;
            new_start = NUM_BLOCKS - available_blocks;
            // in case where we cannot move the file to end, then reject.
            int moving = max(size, extension);
            if(available_blocks < moving) return -1;
            // if there is enough space to move the file, then move
            move_a_file(file, new_start);
            // if the contiguous space after file is smaller than extension
            // defragment again
            if(new_start + size >= NUM_BLOCKS - extension)
                defragment_all();
            // update the new starting index of file in case defragment occurs.
            new_start = DT[file_id].starting_index;

        } else {
            // if there is enough space to fit size + extension, directly move the file
            move_a_file(file, new_start);
        }
        // extend the file.
        fill(directory_contents + new_start + size, 
            directory_contents + new_start + size + extension, file_id);

    } else {
        // if the contigous space is free, then extend directly.
        fill(directory_contents + start + size, 
                directory_contents + start + size + extension, file_id);
    }
    // update the file size and decrease available blocks.
    DT[file_id].size += extension * BLOCK_SIZE;
    available_blocks -= extension;

    auto end = chrono::steady_clock::now();
    extend_t += chrono::duration_cast<chrono::microseconds>(end - start_t).count();

    return 0;
}

// creates a file with given size, on fail returns -1.
int create(int file_id, int bytes){

    auto start = chrono::steady_clock::now();

    int required_blocks = byte_to_block(bytes);
    // if there is no enough space, then reject.
    if(available_blocks < required_blocks) return -1;
    
    int starting = find_first_fit(required_blocks);
    // if there is no contiguous place to fit, try again after defragmentation.
    if(starting == -1){
        defragment_all();
        // try again to find a place to fit in.
        starting = NUM_BLOCKS - available_blocks;
    }
    // if we have space to fit, create a file and add it to DT.
    dtentry_t entry;
    entry.file_id = file_id;
    entry.starting_index = starting;
    entry.size = bytes;
    DT[file_id] = entry;
    // fill the directory contents and decrease available blocks.
    fill(directory_contents + starting, directory_contents + starting + required_blocks, file_id);
    available_blocks -= required_blocks;

    auto end = chrono::steady_clock::now();
    create_t += chrono::duration_cast<chrono::microseconds>(end - start).count();

    return 0;
}

// shrinks a file with given amount, on fail returns -1.
int shrink(int file_id, int shrinking){

    auto start = chrono::steady_clock::now();

    dtentry_t file = DT[file_id];
    // if there is no such file, return -1.
    if(file.size == 0) return -1;

    // if offset is more than file size, reject.
    if(shrinking >= byte_to_block(file.size)) return -1;

    int last = file.starting_index + byte_to_block(file.size);
    int first = last - shrinking;
    // otherwise free the space in directory contents.
    fill(directory_contents + first, directory_contents + last, -1);
    
    // decrease file size in DT and decrease available blocks.
    DT[file_id].size -= shrinking * BLOCK_SIZE;
    available_blocks += shrinking;

    auto end = chrono::steady_clock::now();
    shrink_t += chrono::duration_cast<chrono::microseconds>(end - start).count();

    return 0;
}

// search if there is enough contiguous space in given range.
bool seek(int starting_position, int required_block){
    for(int i = starting_position; i < starting_position + required_block; i++){
        if(directory_contents[i] != -1) return false;
    }
    return true;
}

// search a space to allocate a file, returns -1 when there is no fitting space.
int find_first_fit(int required_block){
    // head: starting index for fitting space
    // ind: points to the current index in the iteration
    // counter: count of the number of contiguous free blocks
    // val: val in the current index of directory contents.
    int head = 0, ind = 0, counter = 0, val;
    while(ind < NUM_BLOCKS){
        // if the block is free, increase the count.
        if(directory_contents[ind] == -1){
            counter++;
        } else {
            // if it is not free, then update the head and reset free block counter.
            head = ind + 1;
            counter = 0;
        }
        // if we found enough space, then return starting index of the space.
        if(counter == required_block) return head;
        ind++;
    }
    // if no space found, then return -1.
    return -1;
}

// move a file to another place block by block.
// Here, extra buffer block (temp) is used.
int move_a_file(dtentry_t file, int new_start){
    int start = file.starting_index, size = byte_to_block(file.size);
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

// for all the files in DT, shift them left. 
void defragment_all(){
    // pointer to file to be moved.
    dtentry_t entry;
    // count: number of files to be moved.
    // head: starting index of free space.
    // ind: current iteration index.
    int count = DT.size(), head = 0, ind = 0, val;
    while(ind < NUM_BLOCKS){
        // if the block is not free,
        if((val = directory_contents[ind]) != -1){
            // get the file in that block.
            entry = DT[val];
            // move it to the left as much as possible.
            move_a_file(entry, head);
            // update new starting index of free space
            int size = byte_to_block(entry.size);
            head += size;
            // jump over this file to found the next file
            ind += size;
            // decrease count, since we moved a file left.
            count--;
        } else {
            // otherwise, increase until find a non-free block.
            ind++;
        }
        // if count is 0, then we moved all files. Break.
        if(count == 0) break;
    }

}

