#include "helper.hpp"

using namespace std;
#define NUM_BLOCKS 32768
#define BLOCK_SIZE 1024


int directory_contents[NUM_BLOCKS];
int available_blocks = NUM_BLOCKS;
int f_id = 0;
map<int, dtentry_t> DT;

int main(int argc, char** argv){

    fill(directory_contents, directory_contents + NUM_BLOCKS, 0);



    return 0;
}

// byte to required amount of block conversion.
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
    for(int i = starting_position; i < end_position; i++){
        if(directory_contents[i] != 0){
            id = directory_contents[i];
            break;
        }
    }
    return id;
}

// search a space to allocate a file
int find_first_fit(int required_block){
    int start = 0;
    dtentry_t entry;
    bool check;
    while(start < NUM_BLOCKS - required_block + 1){
        // check if there is enough contiguous space
        check = seek(start, required_block);
        //if yes, then we found starting index
        if(check) break;
        // if not, update the start index to the end of the next file.
        int id = find_next_file(start, start + required_block);
        entry = DT[id];
        start = entry.starting_index + entry.size;
    }
    return check ? start : -1;
}

// search if there is enough contiguous space in given range.
bool seek(int starting_position, int required_block){
    for(int i = starting_position; i < starting_position + required_block; i++){
        if(directory_contents[i] != 0) return false;
    }
    return true;
}

// shifts a file as much as it can, and returns the new starting index of file.
int defragment_single(dtentry_t file){

    int first = file.starting_index, size = file.size, id = file.file_id;
    int start = first - 1;

    while(start != -1 && directory_contents[start] == 0){
        for(int i = 0; i < size; i++){
            int temp = directory_contents[start + i];
            directory_contents[start + i] = directory_contents[first + i];
            directory_contents[first + i] = temp;
        }
        first--;
        start--;
    }

    return first;
}

// for all the files in DT, shift them left. 
int defragment_all(){

    for(map<int, dtentry_t>::iterator it = DT.begin(); it != DT.end(); it++){
        int new_starting = defragment_single(it->second);
        DT[it->first].starting_index = new_starting;
    }
    return 0;
}

int access(int file_id, int offset){
    dtentry_t file = DT[file_id];
    if(offset > file.size) return -1;
    return file.starting_index + offset/BLOCK_SIZE;
}

int extend(int file_id, int extension){

    if(available_blocks < extension){
        cout << "Extension request rejected for file id: " << file_id << endl;
        return -1;
    }
    
    dtentry_t file = DT[file_id];
    int start = file.starting_index, size = file.size;

    bool check = seek(start + size, extension);
    if(!check){
        start = defragment_single(file);
        check = seek(start + size, extension);
        if(!check){
            cout << "Extension request rejected for file id: " << file_id << endl;
            return -1;
        }
    }

    int first = start + size, last = first + extension;
    for(int i = first; i < last; i++){
        directory_contents[i] = file_id;
    }
    DT[file_id].starting_index = start;
    DT[file_id].size = size + extension;
    available_blocks -= extension;

    return 0;
}

int create(int file_id, int size){

    int required_blocks = byte_to_block(size);
    if(available_blocks < required_blocks){
        cout << "Create rejected for file id: " << file_id << endl;
        return -1;
    } 

    dtentry_t entry;
    int starting = find_first_fit(required_blocks);
    if(starting == -1){
        defragment_all();
        starting = find_first_fit(required_blocks);
        if(starting == -1){
            cout << "Create rejected for file id: " << file_id << endl;
            return -1;
        }
    }

    entry.file_id = file_id;
    entry.starting_index = starting;
    entry.size = required_blocks;
    DT[file_id] = entry;
    fill_directory_content(entry);
    available_blocks -= required_blocks;
    
    return 0;
}