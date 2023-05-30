#ifndef DIR_H
#define DIR_H

#include <stdint.h>

#define MAX_NAME_SIZE 28

typedef struct dir_item {   
    uint16_t inode_id;          // 当前目录项表示的文件/目录的对应inode
    uint8_t valid;              // 当前目录项是否有效 
    uint8_t type;               // 当前目录项类型（文件/目录）
    char name[MAX_NAME_SIZE];   // 目录项表示的文件/目录的文件名/目录名
} dir_item;  // 32bytes

dir_item dir_items[8]; //256bytes

#endif