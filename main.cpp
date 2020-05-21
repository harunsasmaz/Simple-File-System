#include "helper.hpp"

using namespace std;
#define NUM_BLOCKS 32768


int BLOCK_SIZE = 1024;
int directory_contents[NUM_BLOCKS];
int available_blocks = NUM_BLOCKS;
int f_id = 0;
map<int, dtentry_t> DT;

string input_file;
int main(int argc, char** argv){

    // fill directory contents with zero
    fill(directory_contents, directory_contents + NUM_BLOCKS, -1);

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
    while(getline(commands, line)){
        stringstream ss(line);
        getline(ss, operation, ':');
        if(operation == "c"){
            getline(ss, offset, ':');
            int bytes = stoi(offset);
            create(f_id, bytes);
            f_id++;
        } else {
            // these fields are common for three operations
            getline(ss, id, ':');
            getline(ss, offset, ':');
            int id_int = stoi(id);
            int offset_int = stoi(offset);

            if(operation == "a"){
                access(id_int, offset_int);
            } else if(operation == "sh"){
                shrink(id_int, offset_int);
            } else if(operation == "e"){
                extend(id_int, offset_int);
            }
        }
    }

    for(map<int, dtentry_t>::iterator it = DT.begin(); it != DT.end(); it++){
        cout << it->second.file_id << " " << it->second.starting_index
            << " " << it->second.size << endl;
    }

    for(int i = 0; i < NUM_BLOCKS; i++){
        cout << directory_contents[i] << " ";
    }

    cout << endl << available_blocks << endl;

    return 0;
}

// conversion from bytes to required amount of blocks.
int byte_to_block(int bytes){
    return ceil(static_cast<float>(bytes) / static_cast<float>(BLOCK_SIZE));
}

// fill the directory content array according to the entry data.
int fill_directory_content(dtentry_t entry){
    int start = entry.starting_index;
    int occupied = entry.size;
    int id = entry.file_id;
    fill(directory_contents+start, directory_contents+start+occupied, id);
    return 0;
}

// find the id of the next file in given range.
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
// Here, extra buffer (temp) is used.
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
int defragment_all(){
    // for each file in DT, shift them left as much as we can.
    for(map<int, dtentry_t>::iterator it = DT.begin(); it != DT.end(); it++){
        int new_starting = defragment_single(it->second);
        DT[it->first].starting_index = new_starting;
    }
    return 0;
}

// move a file to another place block by block.
// Here, extra buffer (temp) is used.
int move_a_file(dtentry_t file, int new_start){
    int start = file.starting_index, size = file.size;
    // carry each block from starting index block by block.
    for(int i = 0; i < size; i++){
        // -1 means that block is freed.
        int temp = directory_contents[start + i];
        directory_contents[start + i] = -1;
        directory_contents[new_start + i] = temp;
    }
    // update the new starting index of file.
    DT[file.file_id].starting_index = new_start;
    return 0;
}

// returns the block index that contains the specified byte of the file.
int access(int file_id, int offset){
    dtentry_t file = DT[file_id];
    // if no file or offset is too high, return -1.
    if(file.size == 0) return -1;
    if(offset > file.size * BLOCK_SIZE) return -1;
    // return the block index from starting index of file.
    return file.starting_index + offset/BLOCK_SIZE;
}

// extends a file by given amount, on fail returns -1.
int extend(int file_id, int extension){

    // if there is no available blocks at all, reject.
    if(available_blocks < extension){
        cout << "Extension request rejected for file id: " << file_id << endl;
        return -1;
    }
    
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
        if(new_start == -1){
            cout << "Extension request rejected for file id: " << file_id << endl;
            return -1; 
        }
        // if there is, first move the file to there, and then extend.
        move_a_file(file, new_start);
        fill(directory_contents + new_start + size, 
                directory_contents + new_start + size + extension, file_id);

    } else {
        // if the contigous space is free, then extend directly.
        fill(directory_contents + start + size, 
                directory_contents + start + size + extension, file_id);
    }
    // update the file size and decrease available blocks.
    DT[file_id].size = size + extension;
    available_blocks -= extension;
    return 0;
}

// creates a file with given size, on fail returns -1.
int create(int file_id, int size){

    int required_blocks = byte_to_block(size);
    // if there is no enough space, then reject.
    if(available_blocks < required_blocks){
        cout << "Create rejected for file id: " << file_id << endl;
        return -1;
    } 

    int starting = find_first_fit(required_blocks);
    // if there is no contiguous place to fit, try again after defragmentation.
    if(starting == -1){
        defragment_all();
        // try again to find a place to fit in.
        starting = find_first_fit(required_blocks);
        // if no space to fit, then reject.
        if(starting == -1){
            cout << "Create rejected for file id: " << file_id << endl;
            return -1;
        }
    }
    // if we have space to fit, create a file and add it to DT.
    dtentry_t entry;
    entry.file_id = file_id;
    entry.starting_index = starting;
    entry.size = required_blocks;
    DT[file_id] = entry;
    // fill the directory contents and decrease available blocks.
    fill_directory_content(entry);
    available_blocks -= required_blocks;
    
    return 0;
}

// shrinks a file with given amount, on fail returns -1.
int shrink(int file_id, int shrinking){

    dtentry_t file = DT[file_id];
    // if there is no such file, return -1.
    if(file.size == 0) return -1;

    // if offset is more than file size, reject.
    if(shrinking >= file.size){
        cout << "Shrink rejected for file id: " << file_id << endl;
        return -1;
    }

    int last = file.starting_index + file.size;
    int first = last - shrinking;
    // otherwise free the space in directory contents.
    fill(directory_contents + first, directory_contents + last, -1);
    // decrease file size in DT and decrease available blocks.
    DT[file_id].size -= shrinking;
    available_blocks += shrinking;

    return 0;
}