#include "superblock.h"
#include "diskop.h"

#include <string.h>

int write_block(int block_no, char *buf, int block_num){
    if (block_no + block_num - 1 < 0 || block_no + block_num > MAX_BLOCK_COUNT) return -1;
    for (int i = 0; i < block_num; i++) {
        memcpy(disk_file[block_no + i], buf + BLOCK_SIZE * i, BLOCK_SIZE);
    }
    return 0;
}

int read_block(int block_no, char *buf){
    if (block_no < 0 || block_no > MAX_BLOCK_COUNT) return -1;
    memcpy(buf, disk_file[block_no], BLOCK_SIZE);
    return 0;
}