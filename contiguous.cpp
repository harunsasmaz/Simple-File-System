#include "helper.hpp"

using namespace std;
#define NUM_BLOCKS 32768
#define FREE -1

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

     
    cout << "Time taken by program is : " << fixed 
         << chrono::duration_cast<chrono::milliseconds>(end - start).count() << setprecision(5); 
    cout << " milissec " << endl; 

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

// search a space to allocate a file
int find_first_fit(int required_block){
   
    int head = 0, ind = 0, counter = 0, val;
    while(ind < NUM_BLOCKS){

        if(directory_contents[ind] == -1){
            counter++;
        } else {
            val = directory_contents[ind];
            head = ind += DT[val].size;
            counter = 0;
        }

        if(counter == required_block) return head;
        ind++;
    }
    return -1;
}

// search if there is enough contiguous space in given range.
bool seek(int starting_position, int required_block){
    for(int i = starting_position; i < starting_position + required_block; i++){
        if(directory_contents[i] != -1) return false;
    }
    return true;
}

// for all the files in DT, shift them left. 
void defragment_all(){

    dtentry_t entry;
    int count = DT.size(), head = 0, ind = 0, id;
    while(ind < NUM_BLOCKS){

        if((id = directory_contents[ind]) != -1){
            entry = DT[id];
            move_a_file(entry, head);
            head += entry.size;
            ind += entry.size;
            count--;
        } else {
            ind++;
        }

        if(count == 0) break;
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
        starting = NUM_BLOCKS - available_blocks;
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