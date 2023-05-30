#include "superblock.h"
#include "diskop.h"

#include<string.h>

void init_spb(){
    spb.s_free_inodes_count = MAX_INODE_COUNT;
    spb.s_free_blocks_count = MAX_BLOCK_COUNT - RESERVE_BLOCK;     //3 for spb, 128 for inode_table
    spb.s_inodes_count = 0;
    spb.s_blocks_count = 131;
    memset(spb.inode_map, 0, sizeof(spb.inode_map));
    memset(spb.block_map, 0, sizeof(spb.block_map));
    for(int i = 0; i <= 3; i++)
        spb.block_map[i] = ~0;          //第1-131位占用，block_map写1
    spb.block_map[4] = (0xe0000000);
    
    char buf[3*BLOCK_SIZE];
    memset(buf, 0, sizeof(buf));        //将spb经由spb写入内存块1-3
    memcpy(buf, &spb, sizeof(spb));
    write_block(0, buf, 3);
}

int alloc_block(){
    if (!spb.s_free_blocks_count) return -1;
    for (int i = 4; i < MAX_BLOCK_MAP; i++) {
        uint32_t block = spb.block_map[i];
        for (int j = 0; j < 32; j++){
            if ((block >> (31-j)) & 1) continue; //此块不空闲
            else {
                spb.s_free_blocks_count--;
                spb.s_blocks_count++;
                spb.block_map[i] |= 1 << (31-j);

                char buf[3*BLOCK_SIZE]; //将修改后的spb经由spb写入内存块1-3
                memset(buf, 0, sizeof(buf));
                memcpy(buf, &spb, sizeof(spb));
                if(write_block(0, buf, 3) < 0) return -1;
                return i*32+j;
            }
        }
    }
    return -1;
}

int free_block(uint16_t index){
    if (index < RESERVE_BLOCK) return -1; //本函数不允许free spb和inodetable所在块
    int i = index / 32;
    int j = index % 32;
    if (((spb.block_map[i] >> (31 - j)) & 1 ) == 0) return 1; //此块本身已经空闲
    else{
        spb.block_map[i] ^= 1 << (31-j);
        spb.s_free_blocks_count++;
        spb.s_blocks_count--;

        char buf[BLOCK_SIZE];   //清空对应块
        memset(buf, 0, sizeof(buf));
        if(write_block(index, buf, 1) < 0) return -1;

        char buf_2[3*BLOCK_SIZE]; //将修改后的spb经由spb写入内存块1-3
        memset(buf_2, 0, sizeof(buf_2));
        memcpy(buf_2, &spb, sizeof(spb));
        if(write_block(0, buf_2, 3) < 0) return -1;
        return 0;
    }
    return -1;
}