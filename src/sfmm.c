/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"
#include <errno.h>

size_t block_size(sf_block *blockPointer){
    return (blockPointer->header >> 3) << 3; //shift the header by 3 bits to get block size
}

void place_in_freelists(sf_block *block){
    //adds to beginning
    size_t prev_upper_bound = 0;
    size_t pow_2 = 1;
    for(int i = 0; i < NUM_FREE_LISTS; i++){
        size_t curr_upper_bound = pow_2*32;
        if(( (prev_upper_bound < block_size(block)) && (block_size(block) <= curr_upper_bound)) || i == (NUM_FREE_LISTS-1)){
            sf_block *tail = sf_free_list_heads[i].body.links.prev;
            sf_block *temp = tail -> body.links.next;
            block -> body.links.prev = tail;
            block -> body.links.next = temp;
            temp -> body.links.prev = block;
            tail -> body.links.next = block;
            return;
        }
        prev_upper_bound = curr_upper_bound;
        pow_2 = pow_2 << 1;
    }
}

size_t* get_footer(sf_block *blockPointer){
    size_t *footerPointer = (size_t *)blockPointer + block_size(blockPointer)/8 - 1;
    return footerPointer;
}
//returns 0 on success. returns -1 when fails
int init_heap(){
    int *new_page = sf_mem_grow();
    if(new_page  == NULL)
        return -1;

    //Create prologue
    sf_block *prologue = sf_mem_start();
    prologue -> header = 0x21;  //set size and set allocated

    //Create epilogue
    sf_header *epilogue= sf_mem_end();
    epilogue--;
    *epilogue = 1;

    //Initialize quick lists
    for (int i = 0; i< NUM_QUICK_LISTS; i++){
        sf_quick_lists[i].length = 0;
    }
    //Initialize free lists to make them circular
    for (int i = 0; i < NUM_FREE_LISTS; ++i)
    {
        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
    }

    //insert the block implictly created into the last free list class
    int size_of_block = sf_mem_end() - sf_mem_start() - block_size(prologue) - sizeof(sf_header);

    //set header and footer of free block
    size_t *pBlock = (size_t *)prologue;
    pBlock += 4;
    *pBlock = size_of_block + 2;
    sf_block *block = (sf_block *)pBlock;
    pBlock += size_of_block/8 - 1;
    *pBlock = size_of_block + 2;

    place_in_freelists(block);


    return 0;
}

void remove_from_quicklists(sf_header *block){
    for (int i = 0; i < NUM_QUICK_LISTS; i++){
        int index = 0;
        sf_block *prev_block = NULL;
        sf_block *curr_block = sf_quick_lists[i].first;
        while(index < sf_quick_lists[i].length){
            if(curr_block == (sf_block *) block){
                if(prev_block == NULL){
                    sf_quick_lists[i].first = curr_block -> body.links.next;
                }else{
                    prev_block -> body.links.next = curr_block -> body.links.next;
                }
                sf_quick_lists[i].length--;
            }
            prev_block = curr_block;
            curr_block = curr_block -> body.links.next;
            index++;
        }
    }
}
void remove_from_freelists(sf_header *block){
    for (int i = 0; i < NUM_FREE_LISTS; i++){
        sf_block *curr_block = sf_free_list_heads[i].body.links.next;
        while(curr_block != &sf_free_list_heads[i]){
            if(curr_block == (sf_block *)block){
                //remove the block
                sf_block *previous = curr_block -> body.links.prev;
                previous -> body.links.next = curr_block -> body.links.next;
                curr_block -> body.links.next -> body.links.prev = previous;
            }
            curr_block = curr_block -> body.links.next;
        }
    }
}

int first_call(){
    return (sf_mem_start() == sf_mem_end());
}

//returns 0 if growing heap was successful. returns -1 on failure
int attempt_grow_heap(){
    //sf_show_heap();
    //old epilogue becomes header of new block
    sf_header *old_epilogue = sf_mem_end();
    old_epilogue--;
    int prev_alloc = *old_epilogue & 2;

    void *new_page_ptr = sf_mem_grow();
    if (new_page_ptr == NULL){
        return -1;    //failed to grow the heap
    }

    //set header and footer of new block
    sf_block *new_block = (sf_block*)old_epilogue;
    size_t size_of_block = (void *)sf_mem_end() - (void*)old_epilogue - 8; //End - start - size of epilogue
    new_block -> header = size_of_block;
    size_t *new_block_footer = (size_t *) new_block;
    new_block_footer += size_of_block/8 - 1;
    *new_block_footer = size_of_block;

    //coalesce this new block with the preceeding block if possible
    if(prev_alloc){ //previous block is allocated
        //sf_show_heap();
        place_in_freelists(new_block);

    }
    else{
        //get previous footer
        old_epilogue--;
        size_t prev_block_size = *old_epilogue;
        prev_block_size = (prev_block_size >> 3) << 3;
        size_t combined_size = size_of_block + prev_block_size;

        //get previous header and set its values
        sf_header *prev_header = old_epilogue + 1;
        prev_header -=  prev_block_size/8;
        //sf_header original_header = *prev_header;
        sf_header lower_bits = *prev_header - ((*prev_header >> 3) << 3);
        *prev_header = combined_size;
        *prev_header += lower_bits;
        old_epilogue = 0;
        new_block -> header = 0;

        //set the new block footer
        *new_block_footer = *prev_header;

        //if coalesed block was in quick list
        if(lower_bits & 0x4){
            remove_from_quicklists(prev_header);
        }
        else{
            remove_from_freelists(prev_header);
        }

        place_in_freelists((sf_block *)prev_header);
    }

    //create new epilogue
    sf_header *new_epilogue = sf_mem_end();
    new_epilogue--;
    *new_epilogue = 1;

    return 0;
}


//returns pointer to free block if found, else returns NULL
sf_block *find_free_block_quick(size_t size){
    for (int i = 0; i < NUM_QUICK_LISTS; i++){
        if((size <= (32 + i*8))){
            if(sf_quick_lists[i].length != 0){
                sf_block *found_free_block =  sf_quick_lists[i].first;
                sf_quick_lists[i].first = (*sf_quick_lists[i].first).body.links.next;
                sf_quick_lists[i].length--;
                return found_free_block;
            }
        }
    }
    return NULL;
}

//returns pointer to free block if found, else returns NULL
sf_block *find_free_block(size_t size){
    sf_block *found_free_block = NULL;
    while(found_free_block == NULL){
        size_t pow_2 = 1;
        for(int i = 0; i < NUM_FREE_LISTS-1;i++){
            int free_list_size = 32 * pow_2;    //upperbound
            sf_block *sentinal = &sf_free_list_heads[i];
            sf_block *curr_block = sf_free_list_heads[i].body.links.next;
            //iterate through the circular list
            while(curr_block != sentinal){
                if(free_list_size >= size){
                    //remove block from free list
                    sf_block *previous = curr_block -> body.links.prev;
                    previous -> body.links.next = curr_block -> body.links.next;
                    curr_block -> body.links.next -> body.links.prev = previous;
                    found_free_block = curr_block;
                    break;
                }
                if(found_free_block != NULL){break;}
                curr_block = (*curr_block).body.links.next;
            }
            pow_2 = pow_2 << 1;
        }
        if(found_free_block != NULL){
            return found_free_block;
        }
        //Check the last free list
        sf_block *sentinal = &sf_free_list_heads[NUM_FREE_LISTS-1];
        sf_block *curr_block = sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next;
        while(curr_block != sentinal){
            if(block_size(curr_block) >= size){
                //remove block from free list
                sf_block *previous = curr_block -> body.links.prev;
                previous -> body.links.next = curr_block -> body.links.next;
                curr_block -> body.links.next -> body.links.prev = previous;
                found_free_block = curr_block;
                break;
            }
            if(found_free_block != NULL){break;}
            curr_block = (*curr_block).body.links.next;
        }
        //if free block cannot be found in free list and quick list try growing the heap
        if(found_free_block == NULL){
            int r = attempt_grow_heap();
            if (r == -1){return NULL;}
        }
    }

    return found_free_block;
}

//returns pointer to coalesced block
sf_block* coalesce(sf_block *block){
    sf_block *rval = block;
    //set header
    size_t block_prev_alloc = 0;
    if(block -> header & 0x2){
        block_prev_alloc = 0x2;
    }
    rval -> header = block_size(rval) + block_prev_alloc;
    sf_header *rval_footer = get_footer(rval);
    *rval_footer = rval -> header;

    //if prev is free coalesce with left
    if(!(block -> header & 0x2)){
        //calculate location of previous block
        size_t *left_block_footer = ((size_t*) block) - 1;
        size_t left_block_size = (*left_block_footer >> 3) << 3;
        size_t lower_bits_left = *left_block_footer - left_block_size;
        sf_block *left_block = (sf_block*)(((size_t*) block) - left_block_size/8);
        size_t sum_left_mid_size = left_block_size + block_size(block);

        //remove left_block from free_lists
        remove_from_freelists((sf_header *)left_block);

        //left_block points to coalesed block header
        left_block -> header = sum_left_mid_size + lower_bits_left;
        sf_header *left_mid_footer = get_footer(left_block);
        *left_mid_footer = left_block -> header;

        //get rid of old header and footer
        block -> header = 0;
        *left_block_footer = 0;

        rval = left_block;
    }
    sf_block *right_block = (sf_block *)(get_footer(rval) + 1);

    //coalesce with right if free
    if(!(right_block -> header & 0x1)){
        //remove right_block from free_lists
        remove_from_freelists((sf_header *)right_block);
        size_t right_block_size = block_size(right_block);
        size_t total_size = block_size(rval) + right_block_size;
        sf_header *prev_footer = get_footer(rval);

        //set value of header
        size_t lower_bits = (rval -> header) - block_size(rval);
        rval -> header = total_size + lower_bits;
        sf_header *footer = get_footer(rval);
        *footer = rval -> header;

        right_block -> header = 0;
        *prev_footer = 0;
    }
    //update epilogue prev_alloc if needed
    size_t* next_header = (size_t*)((void*)rval + block_size(rval));
    *next_header &= ~0x2;
    return rval;
}

void split_and_handle_remainder(sf_block *raw_block, size_t size){
    //set header left side
    size_t raw_block_size = (((size_t) (raw_block -> header)) >> 3) << 3;
    if(raw_block_size == size || (raw_block_size - size < 4)){return;}
    size_t lower_bits = (raw_block -> header) - raw_block_size;
    raw_block -> header = size + lower_bits;
    //set new header and footer of right side
    size_t *right_block_pointer = (size_t *)raw_block + size/8;
    sf_block *right_block = (sf_block *)right_block_pointer;
    right_block -> header = raw_block_size - size;
    //set prev_alloc value
    right_block -> header += 2;
    //set right block footer
    size_t *right_footer = get_footer(right_block);
    *right_footer = right_block -> header;
    //add the right block into a free list

    sf_block *coalesced_block = coalesce(right_block);
    place_in_freelists(coalesced_block);

    //set prev alloc in epliogue if appropriate
    size_t* next_header = (size_t*)((void*)coalesced_block + block_size(coalesced_block));
    *next_header &= ~0x2;
}
void *sf_malloc(size_t size) {
    if (size == 0){return NULL;}
    size += 8;   //Add size of header

    //multiple of 8
    if (size % 8 != 0){
        size += 8 - (size%8);
    }
    if (size < 32){size = 32;}


    if(first_call()){
        int init_status = init_heap();
        if (init_status == -1){
            return NULL;
        }
    }
    sf_block *raw_block = find_free_block_quick(size);
    if (raw_block == NULL){
        raw_block = find_free_block(size);
    }
    if(raw_block == NULL){
        sf_errno = ENOMEM;
        return NULL;
    }
    //Set allocated
    raw_block -> header |= 1;

    //Split block and handle remainder
    split_and_handle_remainder(raw_block, size);

    //Set_epilogue prev alloc value to 1
    size_t* next_header = (size_t*)((void*)raw_block + block_size(raw_block));
    *next_header |= 0x2;
    return &(*raw_block).body.payload[0];   //raw_block pointer now points to the allocated split block
}

//returns 0 on success, -1 on failure
int place_in_quicklists(sf_block *block){
    for(int i = 0; i< NUM_QUICK_LISTS; i++){
        if(block_size(block) == 32+8*i){
            //format the block
            int prev_alloc = 0;
            if ((block -> header) & 0x2){
                prev_alloc = 0x2;
            }
            block -> header = block_size(block) + 0x4 + prev_alloc + 0x1;

            //flush quick list if needed
            if(sf_quick_lists[i].length == QUICK_LIST_MAX){
                while(sf_quick_lists[i].length != 0){
                    sf_block *block_to_insert = sf_quick_lists[i].first;
                    if(block_to_insert->body.links.next != NULL){
                        sf_quick_lists[i].first = block_to_insert->body.links.next;
                    }else{
                        sf_quick_lists[i].first = NULL;
                    }

                    //format new freelist block
                    int prev_alloc_block_to_insert = 0;
                    if ((block_to_insert-> header) & 0x2){
                        prev_alloc_block_to_insert = 0x2;
                    }
                    block_to_insert -> header = block_size(block_to_insert) + prev_alloc_block_to_insert;
                    sf_header *footer = get_footer(block_to_insert);
                    *footer = block_to_insert -> header;
                    block_to_insert -> body.links.next = 0;

                    sf_block *coalesced_block = coalesce(block_to_insert);
                    place_in_freelists(coalesced_block);
                    sf_quick_lists[i].length--;
                }
            }
            //insert
            block -> body.links.next = sf_quick_lists[i].first;
            sf_quick_lists[i].first = block;
            sf_quick_lists[i].length++;
            return 0;
        }
    }
    //should never reach here
    return -1;
}
//returns 0 if not valid. 1 if valid
int check_valid_pointer(void *pp){
    if (pp == NULL){
        return 0;
    }
    if (((unsigned long)pp %8) != 0){
        return 0;
    }
    sf_block *pp_block = (sf_block *)(pp - 8);
    if (block_size(pp_block) % 8 != 0){
        return 0;
    }
    if ((void*)pp_block < sf_mem_start() + 32){
        return 0;
    }
    if ((void*)get_footer(pp_block) > sf_mem_end() - 8){
        return 0;
    }
    if(!(pp_block -> header & 0x1)){
        return 0;
    }
    if (pp_block -> header & 0x4){
        return 0;
    }
    //if prev alloc is 0, indicating previous is free
    if (!((pp_block -> header) & 0x2)){
        //walk heap until you check if the previous block is actually free
        sf_block *prev = NULL;
        void *temp = sf_mem_start() + 32;

        while(temp != pp_block){
            prev = temp;
            temp += block_size(temp);
        }

        if(prev == NULL){
            return 0;
        }
        if(prev -> header & 0x1){
            return 0;
        }
    }
    return 1;
}
void sf_free(void *pp) {
    if(check_valid_pointer(pp) == 0){
        abort();
    }
    sf_block *pp_block = (sf_block *)(pp - 8);
    sf_block *block_ptr = (sf_block*) pp_block;
    size_t free_block_size = block_size(block_ptr);
    size_t *end_of_block = (size_t *)block_ptr + free_block_size/8;
    //remove old contents
    size_t *temp = (size_t*) &(block_ptr -> body.payload[0]);
    while(temp != end_of_block){
        *temp = 0;
        temp += 1;
    }
    sf_block *big_block= coalesce(block_ptr);

    //attempt to place in quicklist
    if(place_in_quicklists(big_block) == -1){
        place_in_freelists(big_block);
    }else{
        size_t* next_header = (size_t*)((void*)big_block + block_size(big_block));
        *next_header |= 0x2;
    }
}

void *sf_realloc(void *pp, size_t rsize) {
    if(check_valid_pointer(pp) == 0){
        sf_errno = EINVAL;
        return NULL;
    }
    sf_block *alloced_block = (sf_block *)(pp - 8);
    if(rsize == 0){
        sf_free(pp);
        return NULL;
    }
    size_t alloced_block_size = block_size(alloced_block);
    size_t padded_size = rsize + 8; //payload + header
    if (padded_size % 8 != 0){
        padded_size += 8 - (rsize %8);
    }
    if (padded_size < 32){
        padded_size = 32;
    }
    if(padded_size > alloced_block_size){
        sf_block *larger_block = sf_malloc(padded_size-8) - 8;
        size_t lower_bits = (alloced_block -> header) - alloced_block_size;
        memcpy(&(larger_block -> body.payload[0]), pp, block_size(alloced_block) - 8);
        sf_free(pp);

        //update header
        larger_block -> header = padded_size + lower_bits;

        size_t* next_header = (size_t*)((void*)larger_block + block_size(larger_block));
        *next_header |= 0x2;

        return &(larger_block->body.payload[0]);
    }
    else if(padded_size < alloced_block_size){
        size_t splinter_size = block_size(alloced_block) - padded_size;
        if((splinter_size < 32)){//dont split
            return pp;
        }else{//split
            split_and_handle_remainder(alloced_block, padded_size);
            //&(alloced_block->body.payload[0])
            return &(alloced_block->body.payload[0]);

        }
    }
    else{
        return pp;
    }
    abort();
}

//return 1 if num is a power of 2 else returns 0
int is_pow2(size_t num){
    return (num & (num-1)) == 0;
}

//returns 1 if is aligned with alignment, else 0
//checks by comparing number of zeros
int is_aligned(void* address, size_t alignment){
    int zeros = 0;
    int curr_bit = alignment & 0x1;
    while(curr_bit != 1){
        zeros++;
        alignment = alignment >> 1;
        curr_bit = alignment & 0x1;
    }
    size_t addres = (size_t)address;
    int i = 0;
    int curr_ptr_bit = addres & 0x1;
    while(i < zeros){
        if(curr_ptr_bit != 0){
            return 0;
        }
        addres = addres >> 1;
        curr_ptr_bit = addres & 0x1;
        i++;
    }
    return 1;
}

void *sf_memalign(size_t size, size_t align) {
    if(align < 8 || !is_pow2(align)){
        sf_errno = EINVAL;
        return NULL;
    }
    /*
    requested size, plus the alignment size, plus the minimum
    block size, plus the size required for a block header and footer.
    */
    size_t rblock_size = size + 8;

    if (rblock_size % 8 != 0){
        rblock_size += 8 - (rblock_size%8);
    }
    if(rblock_size < 32){rblock_size = 32;}

    size += 8;
    size +=  align + 32;
    if (size % align != 0){
        size += align - (size%align);
    }
    if(size < 32){size = 32;}

    if(first_call()){
        int init_status = init_heap();
        if (init_status == -1){
            return NULL;
        }
    }

    sf_block *raw_block = find_free_block_quick(size);
    if (raw_block == NULL){
        raw_block = find_free_block(size);
    }
    if(raw_block == NULL){
        sf_errno = ENOMEM;
        return NULL;
    }
    sf_block *rblock = NULL;

    //if normal payload address satisfies the alignment
    if(is_aligned(raw_block -> body.payload, align) == 1){
        char* payload_ptr = raw_block -> body.payload;
        //set header
        sf_header *hdr = (void*)payload_ptr - 8;
        *hdr = block_size(raw_block) + 1;

        rblock = (sf_block *)hdr;

        //clean old footer for payload
        sf_footer *raw_block_footer = get_footer(raw_block);
        *raw_block_footer = 0;
    }
    //larger address satisfies the alignment
    else{
        int payload_shift = 0;
        char* payload_ptr = &(raw_block -> body.payload[0]);
        while((is_aligned(payload_ptr, align) == 0) || (payload_shift < 32)){   //ensures alignment and sufficient space in initial
            payload_shift++;
            payload_ptr++;
        }

        //set header
        sf_header *hdr = (void*)payload_ptr - 8;
        *hdr = block_size(raw_block) - payload_shift + 1; //set size and set allocated and prev is free

        rblock = (sf_block *)hdr;

        //clean old footer for payload
        sf_footer *raw_block_footer = get_footer(raw_block);
        *raw_block_footer = 0;

        //create block for the intitial portion
        sf_block *initial = raw_block;
        size_t size_of_initial = block_size(raw_block) - (block_size(raw_block) - payload_shift);
        size_t lower_bits = (raw_block -> header) - block_size(raw_block);
        initial -> header = size_of_initial + lower_bits;
        sf_footer *initial_footer = (void*) initial + size_of_initial - 8;
        *initial_footer = initial -> header;

        sf_block *coalesced_block = coalesce(initial);

        place_in_freelists(coalesced_block);
    }
    //free the following portion
    split_and_handle_remainder(rblock, rblock_size);

    //Set_epilogue prev alloc value to 1 if appropriate
    size_t* next_header = (size_t*)((void*)rblock + block_size(rblock));
        *next_header |= 0x2;

    return rblock -> body.payload;

    abort();
}
