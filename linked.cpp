#include "helper.hpp"

using namespace std;
#define NUM_BLOCKS 32768
#define FILE_END INT_MAX
#define FAT_VALUE INT_MIN
#define FREE -1

int BLOCK_SIZE = 1024;              // default block size is 1024
int directory_contents[NUM_BLOCKS]; // file directory, -1 represents free blocks.
int available_blocks = NUM_BLOCKS;  // available block count holder
int f_id = 0; 

int FAT[NUM_BLOCKS];
int FAT_sz;

map<int, dtentry_t> DT;
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

    cout << "Time taken by program is : " << fixed 
         << chrono::duration_cast<chrono::milliseconds>(end - start).count() << setprecision(2); 
    cout << " milisec " << endl; 
    
    cout << "Total create: " << c_count << "\tReject create: " << c_reject << endl;
    cout << "Total extend: " << e_count << "\tReject extend: " << e_reject << endl;
    cout << "Total shrink: " << sh_count << "\tReject shrink: " << sh_reject << endl;
    cout << "Total access: " << a_count << "\tReject access: " << a_reject << endl;

    cout << "Available blocks: " << available_blocks << endl;

    return 0;
}

// conversion from bytes to required amount of blocks.
int byte_to_block(int bytes){
    return ceil(static_cast<float>(bytes) / static_cast<float>(BLOCK_SIZE));
}

int find_free_block(){

    int ind = FAT_sz;
    while(ind < NUM_BLOCKS){
        if(directory_contents[ind] == -1) break;
        ind++;
    }
    return ind;
}

int create(int file_id, int bytes){

    // convert bytes to blocks, if not enough available
    // blocks exists, then reject.
    int required_blocks = byte_to_block(bytes);
    if(required_blocks > available_blocks) return -1;
    
    // Linked list manner, we start with a free cell, which is also starting point.
    // Then, find a next cell, link them and iterate until file size is reached.

    int next, current = find_free_block(), starting = current;
    directory_contents[current] = file_id;
    for(int i = 1; i < required_blocks; i++){
        // find next cell
        next = find_free_block();
        // link current to next
        FAT[current] = next;
        // next points to special end of file
        FAT[next] = FILE_END;
        // update pointers.
        current = next;
        // update directory contents.
        directory_contents[current] = file_id;
    }

    // allocate new entry in Directory Table
    dtentry_t entry;
    entry.file_id = file_id;
    entry.starting_index = starting;
    entry.size = required_blocks;
    entry.bytes = bytes;
    DT[file_id] = entry;

    // decrease available blocks.
    available_blocks -= required_blocks;
    return 0;
}

int extend(int file_id, int extension){

    // if there is no enough space, then reject.
    if(extension > available_blocks) return -1;
    
    // starting point and size to be able to find tail of file.
    int starting = DT[file_id].starting_index;
    int size = DT[file_id].size;

    // if no such file, then reject.
    if(size == 0) return -1;
    
    // iterate until reaching last block.
    int current = starting;
    for(int i = 1; i < size; i++){
        current = FAT[current];
    }

    // find new free cells to extend the file.
    int next;
    for(int i = 0; i < extension; i++){
        // find a new cell
        next = find_free_block();
        // link the current pointer to next.
        FAT[current] = next;
        // point next pointer to end of file.
        FAT[next] = FILE_END;
        // update pointers.
        current = next;
        // update directory contents.
        directory_contents[current] = file_id;
    }

    // update related information of the file.
    DT[file_id].size += extension;
    DT[file_id].bytes += extension * BLOCK_SIZE;

    // decrease available blocks.
    available_blocks -= extension;
    return 0;
}

int access(int file_id, int offset){

    // calculate how many blocks to iterate.
    int block_ind = byte_to_block(offset);

    // needed file information.
    int starting = DT[file_id].starting_index;
    int bytes = DT[file_id].bytes;

    // if no such file, reject.
    if(bytes == 0) return -1;

    // if offset is larger than file size, reject.
    if(offset > bytes) return -1;
    
    // if everything is ok, then iterate until reaching the desired block.
    int current = starting;
    for(int i = 1; i < block_ind; i++){
        current = FAT[current];
    }

    return current;
}

int shrink(int file_id, int shrinking){

    // necessary file information
    int size = DT[file_id].size;
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
    DT[file_id].size -= shrinking;
    DT[file_id].bytes -= shrinking * BLOCK_SIZE;

    // increase free blocks.
    available_blocks += shrinking;

    return 0;
}