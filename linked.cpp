#include "helper.hpp"

using namespace std;
#define NUM_BLOCKS 32768
#define FILE_END INT_MAX
#define FAT_VALUE INT_MIN
#define FREE -1

// IMPORTANT NOTICE:
// Free blocks in the directory contents are represented as -1.
// Occupied blocks are represented with the IDs of files.
// FAT blocks are occupied at the beginning and represented with FAT_VALUE
// Operations do not rely on the values in directory contents.

int BLOCK_SIZE = 1024;              // default block size is 1024
int directory_contents[NUM_BLOCKS]; // file directory, -1 represents free blocks.
int available_blocks = NUM_BLOCKS;  // available block count holder
int f_id = 0; 

int FAT[NUM_BLOCKS];
int FAT_sz;
map<int, dtentry_t> DT;

// total times for each operation.
int create_t, extend_t, shrink_t, access_t;

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

    // store the FAT in directory contents.
    FAT_sz = ceil((double)NUM_BLOCKS / (BLOCK_SIZE / 4 + 1));
    std::fill(directory_contents, directory_contents + FAT_sz, FAT_VALUE);
    available_blocks -= FAT_sz;

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

int create(int file_id, int bytes){

    auto start = chrono::steady_clock::now();

    // convert bytes to blocks, if not enough available
    // blocks exists, then reject.
    int required_blocks = byte_to_block(bytes);
    if(required_blocks > available_blocks) return -1;
    
    // Linked list manner, we start with a free cell, which is also starting point.
    // Then, find a next cell, link them and iterate until file size is reached.

    int starting, counter = 0, ind = FAT_sz, current;
    while(ind < NUM_BLOCKS)
    {   
        // if a free block is found
        if(directory_contents[ind] == -1){
            // if counter is 0, then it is the starting index of file.
            if(counter == 0){
                starting = ind;
            // if not 0, then we link the previous block to this index.
            } else {
                FAT[current] = ind;
            }
            // update the current pointer to new end of file.
            current = ind;
            // increase counter
            counter++;
            // update directory contents with file data.
            directory_contents[current] = file_id;
        }
        // if we fill enough blocks, then point the last block to EOF
        if(counter == required_blocks){
            FAT[current] = FILE_END;
            break;
        }
        // continue with next block.
        ind++;
    }

    // allocate new entry in Directory Table
    dtentry_t entry;
    entry.file_id = file_id;
    entry.starting_index = starting;
    entry.size = bytes;
    DT[file_id] = entry;

    // decrease available blocks.
    available_blocks -= required_blocks;

    auto end = chrono::steady_clock::now();
    create_t += chrono::duration_cast<chrono::microseconds>(end - start).count();

    return 0;
}

int extend(int file_id, int extension){

    auto start = chrono::steady_clock::now();

    // if there is no enough space, then reject.
    if(extension > available_blocks) return -1;
    
    // starting point and size to be able to find tail of file.
    int starting = DT[file_id].starting_index;
    int bytes = DT[file_id].size;
    int size = byte_to_block(bytes);

    // if no such file, then reject.
    if(size == 0) return -1;
    
    // iterate until reaching last block.
    int current = starting;
    for(int i = 1; i < size; i++){
        current = FAT[current];
    }

    // find new free cells to extend the file.
    int counter = 0, ind = FAT_sz;
    while(ind < NUM_BLOCKS)
    {   
        // if the block is free
        if(directory_contents[ind] == -1){
            // link the last block of file to this
            FAT[current] = ind;
            // ind becomes the new last block
            current = ind;
            // count the number of extended blocks
            counter++;
            // update directory contents with extended file.
            directory_contents[current] = file_id;
        }
        // if enough blocks are extended, then link last block
        // to EOF and return.
        if(counter == extension){
            FAT[current] = FILE_END;
            break;
        }
        // increase index.
        ind++;
    }

    // update related information of the file.
    DT[file_id].size += extension * BLOCK_SIZE;

    // decrease available blocks.
    available_blocks -= extension;

    auto end = chrono::steady_clock::now();
    extend_t += chrono::duration_cast<chrono::microseconds>(end - start).count();

    return 0;
}

int access(int file_id, int offset){

    auto start = chrono::steady_clock::now();

    // calculate how many blocks to iterate.
    int block_ind = byte_to_block(offset);

    // needed file information.
    int starting = DT[file_id].starting_index;
    int bytes = DT[file_id].size;

    // if no such file, reject.
    if(bytes == 0) return -1;

    // if offset is larger than file size, reject.
    if(offset > bytes) return -1;
    
    // if everything is ok, then iterate until reaching the desired block.
    int current = starting;
    for(int i = 1; i < block_ind; i++){
        current = FAT[current];
    }

    auto end = chrono::steady_clock::now();
    access_t += chrono::duration_cast<chrono::microseconds>(end - start).count();

    return current;
}

int shrink(int file_id, int shrinking){

    auto start = chrono::steady_clock::now();

    // necessary file information
    int bytes = DT[file_id].size;
    int size = byte_to_block(bytes);
    int starting = DT[file_id].starting_index;

    // check if there is no such file or shrinking amount is too high.
    if(size == 0 || shrinking > size - 1) return -1;
    
    // determine the new end of file.
    int new_last = starting;
    for(int i = 1; i < size - shrinking; i++){
        new_last = FAT[new_last];
    }

    // free the remanining blocks in directory contents.
    int curr = FAT[new_last];
    while(curr != FILE_END){
        directory_contents[curr] = -1;
        curr = FAT[curr];
    }

    // update the new end of file in FAT.
    FAT[new_last] = FILE_END;

    // update file data in Directory Table.
    DT[file_id].size -= shrinking * BLOCK_SIZE;

    // increase free blocks.
    available_blocks += shrinking;

    auto end = chrono::steady_clock::now();
    shrink_t += chrono::duration_cast<chrono::microseconds>(end - start).count();

    return 0;
}