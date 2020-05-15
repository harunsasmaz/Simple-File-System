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
    while(start < NUM_BLOCKS - required_block + 1){
        // check if there is enough contiguous space
        bool check = seek(start, required_block);
        //if yes, then we found starting index
        if(check) break;
        // if not, update the start index to the end of the next file.
        int id = find_next_file(start, start + required_block);
        entry = DT[id];
        start = entry.starting_index + entry.size;
    }
    return start;
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

    dtentry_t file = DT[file_id];
    int id = file.file_id, start = file.starting_index, size = file.size;
    int first = start + size, last = first + extension;

    bool check = seek(first, extension);
    if(check){
        for(int i = first; i < last; i++){
            directory_contents[i] = id;
        }
    } else {
        int new_start = defragment_single(file);
        check = seek(new_start + size, extension);
        if(check){
            int new_first = new_start + size, new_last = new_first + extension;
            for(int i = new_first; i < new_last; i++){
                directory_contents[i] = id;
            }
        } else {
            cout << "Extension request rejected for file id: " << id << endl;
            return -1;
        }
    }

    return 0;
}