#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

#include <stdint.h>

#define BLOCK_SIZE 256
#define MAX_INODE_MAP 32  
#define MAX_BLOCK_MAP 128

#define MAX_INODE_COUNT 1024            //32*32 = 1024
#define MAX_BLOCK_COUNT 4096            //128*32 = 4096
#define RESERVE_BLOCK 131

typedef struct super_block{
    uint16_t s_magic;
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_free_blocks_count;
    uint32_t s_root;
    uint32_t block_map[MAX_BLOCK_MAP];  //按位记录block是否空闲，每个数含32位
    uint32_t inode_map[MAX_INODE_MAP];  //按位记录inode是否空闲，每个数含32位
} super_block; //662 bytes, 3 blocks

/**
 * @brief 初始化super block
 */
int init_spb();

/**
 * @brief 从磁盘中加载spb
 */
int load_spb();

/**
 * @brief 加载后核对spb中块上限数量和当前磁盘是否一致
 */
int check_spb();

/**
 * @brief 寻找并分配空闲块
 * @return 返回大于0代表成功（即块编号），-1代表失败
 */
int alloc_block();

/**
 * @brief 将已分配block还原
 * @param index 需要还原的block号
 * @return 返回0代表成功，-1代表失败，1代表本块本身已经空闲
 */
int free_block(uint16_t index);

super_block spb;

#endif