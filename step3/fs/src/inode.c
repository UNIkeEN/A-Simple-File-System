#include "inode.h"
#include "diskop.h"
#include "dir.h"

#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

int init_root_inode(){
    inode root;
    root.i_index = 0;
    root.i_mode = 1;
    root.i_link_count = 0;
    root.i_size = 0;
    root.i_timestamp = time(NULL);
    root.i_parent = 65535;
    memset(root.i_direct, 0, sizeof(root.i_direct));
    root.i_single_indirect = 0;
    inode_table[0] = root;
    spb.inode_map[0] |= (1 << 31);
    return 0;
}

int init_inode(inode* node, uint16_t index, uint8_t mode, uint8_t link, uint16_t size, uint16_t parent){
    node->i_index = index;
    node->i_mode = mode;
    node->i_link_count = link;
    node->i_size = size;
    node->i_parent = parent;
    node->i_timestamp = time(NULL);
    memset(node->i_direct, 0, sizeof(node->i_direct));
    node->i_single_indirect = 0;
    write_inode(node, index);
    return 0;
}

int write_inode(inode* node, uint16_t index){
    int block_id = INODE_TABLE_START_BLOCK + index / INODE_PER_BLOCK;
    int inode_start = sizeof(inode) * (index % INODE_PER_BLOCK);
    char buf[BLOCK_SIZE];
    if (read_block(block_id, buf) < 0) return -1;
    memcpy(buf + inode_start, node, sizeof(inode));
    write_block(block_id, buf, 1);
    return 0;
}

int read_inode(inode* node, uint16_t index){
    int block_id = INODE_TABLE_START_BLOCK + index / INODE_PER_BLOCK;
    int inode_start = sizeof(inode) * (index % INODE_PER_BLOCK);
    char buf[BLOCK_SIZE];
    if (read_block(block_id, buf) < 0)  return -1;
    memcpy(node, &buf[inode_start], sizeof(inode));
    return 1;
}

int alloc_inode(){
    if (!spb.s_free_inodes_count) return -1;
    for (int i = 0; i < MAX_INODE_MAP; i++) {
        uint32_t node = spb.inode_map[i];
        for (int j = 0; j < 32; j++){
            if ((node >> (31-j)) & 1) continue; //此块不空闲
            else {
                spb.s_free_inodes_count--;
                spb.s_inodes_count++;
                spb.inode_map[i] |= 1 << (31-j);

                char buf[3*BLOCK_SIZE]; //将修改后的spb经由spb写入内存块1-3
                memset(buf, 0, sizeof(buf));
                memcpy(buf, &spb, sizeof(spb));
                write_block(0, buf, 3);
                return i*32+j;
            }
        }
    }
    return -1;
}

//dir型inode本函数将直接删除并释放块（不检查块中是否有文件，这应由上游完成）；file型会释放块
int free_inode(inode* node){
    if (node->i_index <= 0) return -1; //不允许free root
    int i = node->i_index / 32;
    int j = node->i_index % 32;
    if (((spb.inode_map[i] >> (31 - j)) & 1) == 0) return 1; //此块本身已经空闲
    else{
        for (int k = 0; k < 8; k++)                         //清空direct
            if (node->i_direct[k]!=0) free_block(node->i_direct[k]);
        if (node->i_single_indirect != 0){                  //清空一级indirect
            char buf[BLOCK_SIZE];
            uint16_t indirect_blocks[128];
            if (read_block(node->i_single_indirect, buf) < 0) return -1;
            memcpy(&indirect_blocks, buf, BLOCK_SIZE);
            for (int k = 0; k < 127; k++)
                if (indirect_blocks[k]!=0) free_block(indirect_blocks[k]);
            free_block(node->i_single_indirect);
        }
        if (init_inode(node, node->i_index, 0, 0, 0, 65535)<0) return -1;   //  还原此inode
        
        spb.inode_map[i] ^= 1 << (31-j);    //修改inode table
        spb.s_free_inodes_count++;
        spb.s_inodes_count--;
        if (spb.s_free_blocks_count > MAX_INODE_COUNT) return -1;

        char buf[3*BLOCK_SIZE]; //将修改后的spb经由spb写入内存块1-3
        memset(buf, 0, sizeof(buf));
        memcpy(buf, &spb, sizeof(spb));
        if(write_block(0, buf, 3) < 0) return -1;
        return 0;
    }
}

//目录型inode仅direct有效，每个指向的块中含有8个dir_item，一共有64个dir_item可以存在一个目录下
int add_to_dir_inode(inode* dir_node, char* name, uint8_t type){
    if (!dir_node->i_mode) return -1;   //不是目录结点
    char buf[BLOCK_SIZE];
    dir_item dir_items[8];
    for (int i = 0; i < 8; i++){
        if (dir_node->i_direct[i]==0) continue;
        if (read_block(dir_node->i_direct[i], buf)<0) continue;
        memcpy(&dir_items, buf, BLOCK_SIZE);
        for (int j = 0; j < 8; j++){    //遍历8个direct各自对应block中的8个dir_item，找空
            if (dir_items[j].valid == 0){
                int new_inode_id = alloc_inode();
                if (new_inode_id >= 0){
                    init_inode(&inode_table[new_inode_id], new_inode_id, type, 0, 0, dir_node->i_index);

                    dir_items[j].valid = 1;
                    strcpy(dir_items[j].name, name);
                    dir_items[j].inode_id = new_inode_id;
                    dir_items[j].type = type;

                    memcpy(buf, &dir_items, BLOCK_SIZE);
                    if(write_block(dir_node->i_direct[i], buf, 1)<0) return -1;

                    dir_node->i_timestamp = time(NULL);
                    if(write_inode(dir_node, dir_node->i_index)<0) return -1;
                    return 0;
                }
                else return -1;
            }  
        }
    }
    //如果上述过程无结果且link<8，则新分配block
    if (dir_node->i_link_count < 8){
        int k; //寻找尚未link的direct编号
        for(k = 0; k < 8; k++) if(dir_node->i_direct[k] == 0) break;
        int new_block_id = alloc_block();
        if (new_block_id >= 0){
            int new_inode_id = alloc_inode();
            if (new_inode_id >= 0){
                dir_node->i_direct[k] = new_block_id;   //新分配block
                dir_node->i_link_count += 1;

                for(int i = 0; i< 8; i++){   
                    dir_items[i].valid = 0;
                    dir_items[i].inode_id = 0;
                }
                init_inode(&inode_table[new_inode_id], new_inode_id, type, 0, 0, dir_node->i_index);    //新direct-block第一位创建新inode

                dir_items[0].valid = 1;
                strcpy(dir_items[0].name, name);
                dir_items[0].inode_id = new_inode_id;
                dir_items[0].type = type;

                memcpy(buf, &dir_items, BLOCK_SIZE);
                if(write_block(dir_node->i_direct[k], buf, 1)<0) return -1;

                dir_node->i_timestamp = time(NULL);
                if(write_inode(dir_node, dir_node->i_index)<0) return -1;
                return 0;
            }
            else return -1;
        }
        else return -1;
    }
    return -1;
}

int rm_from_dir_inode(inode* dir_node, char* name){
    if (!dir_node->i_mode) return -1;
    char buf[BLOCK_SIZE];
    dir_item dir_items[8];
    for (int i = 0; i < 8; i++){
        if (dir_node->i_direct[i]==0) continue;
        if (read_block(dir_node->i_direct[i], buf)<0) continue;
        memcpy(&dir_items, buf, BLOCK_SIZE);
        for(int j = 0; j < 8; j++){
            if (dir_items[j].valid == 0) continue;
            if (strcmp(dir_items[j].name, name) == 0 && dir_items[j].type == 0){    //名称相符且是文件
                free_inode(&inode_table[dir_items[j].inode_id]);
                dir_items[j].valid = 0;
                memcpy(buf, &dir_items, BLOCK_SIZE);
                if(write_block(dir_node->i_direct[i], buf, 1)<0) return -1;

                dir_node->i_timestamp = time(NULL);
                if(write_inode(dir_node, dir_node->i_index)<0) return -1;
                return 0;
            }
        }
    }
    return -1;
}

int rmdir_from_dir_inode(inode* dir_node, char* name){
    if (!dir_node->i_mode) return -1;
    char buf[BLOCK_SIZE];
    dir_item dir_items[8];
    for (int i = 0; i < 8; i++){
        if (dir_node->i_direct[i]==0) continue;
        if (read_block(dir_node->i_direct[i], buf)<0) continue;
        memcpy(&dir_items, buf, BLOCK_SIZE);
        for(int j = 0; j < 8; j++){
            if (dir_items[j].valid == 0) continue;
            if (strcmp(dir_items[j].name, name) == 0 && dir_items[j].type == 1){    //名称相符且是文件夹
                if (check_dir_isempty_delete(&inode_table[dir_items[j].inode_id]) < 0) return -1;   //非空或清空失败
                free_inode(&inode_table[dir_items[j].inode_id]);
                dir_items[j].valid = 0;
                memcpy(buf, &dir_items, BLOCK_SIZE);
                if(write_block(dir_node->i_direct[i], buf, 1)<0) return -1;

                dir_node->i_timestamp = time(NULL);
                if(write_inode(dir_node, dir_node->i_index)<0) return -1;
                return 0;
            }
        }
    }
    return -1;
}

int check_dir_isempty_delete(inode* dir_node){
    if (!dir_node->i_mode) return -1;
    char buf[BLOCK_SIZE];
    dir_item dir_items[8];
    for (int i = 0; i < 8; i++){
        if (dir_node->i_direct[i]==0) continue;
        if (read_block(dir_node->i_direct[i], buf)<0) continue;
        memcpy(&dir_items, buf, BLOCK_SIZE);
        for(int j = 0; j < 8; j++){
            if (dir_items[j].valid) {
                printf("Error: dir is not empty!\n");
                return -1;
            }
        }
        free_block(dir_node->i_direct[i]);
        dir_node->i_direct[i] = 0;
        dir_node->i_link_count--;
    }
    return 0;
}

int lexic_cmp(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

int ls_dir_inode(inode* dir_node, char *ret, char *ret2){
    if (!dir_node->i_mode) return -1;
    char buf[BLOCK_SIZE];
    bool flag = false;  //文件夹是否为空，false代表空

    char dir_name[64][MAX_NAME_SIZE];
    char file_name[64][MAX_NAME_SIZE];
    char dir[64 * MAX_NAME_SIZE];
    char file[64 * MAX_NAME_SIZE];
    int num_dir = 0, num_file = 0;

    ret[0]='\0';
    dir_item dir_items[8];
    for (int i = 0; i < 8; i++){
        if (dir_node->i_direct[i]==0) continue;
        if (read_block(dir_node->i_direct[i], buf)<0) continue;
        memcpy(&dir_items, buf, BLOCK_SIZE);
        for(int j = 0; j < 8; j++){
            if (dir_items[j].valid) {
                if (dir_items[j].type == 1)
                    strcpy(dir_name[num_dir++], dir_items[j].name);
                else 
                    strcpy(file_name[num_file++], dir_items[j].name);
                flag = true;
            }
        }
    }
    // 字典序排序文件与文件夹名
    qsort(dir_name, num_dir, sizeof(dir_name[0]), lexic_cmp);
    qsort(file_name, num_file, sizeof(file_name[0]), lexic_cmp);
    dir[0] = '\0';
    for (int i = 0; i < num_dir; i++) {
        if (i) strcat(dir, "  ");
        strcat(dir, dir_name[i]);
    }
    file[0] = '\0';
    for (int i = 0; i < num_file; i++) {
        if (i) strcat(file, "  ");
        strcat(file, file_name[i]);
    }
    if (flag){
        sprintf(ret, "%s  &  %s\n", file, dir);
        sprintf(ret2, "%s  \033[1;34m%s\033[0m\n", file, dir);
    }else{
        sprintf(ret, " \n");
        sprintf(ret2, " \n");
    }
    return 0;
}

int search_in_dir_inode(inode* dir_node, char* name, int type){
    if (!dir_node->i_mode) return -1;
    char buf[BLOCK_SIZE];
    dir_item dir_items[8];
    for (int i = 0; i < 8; i++){
        if (dir_node->i_direct[i]==0) continue;
        if (read_block(dir_node->i_direct[i], buf)<0) continue;
        memcpy(&dir_items, buf, BLOCK_SIZE);
        for(int j = 0; j < 8; j++){
            if (dir_items[j].valid == 0) continue;
            if (strcmp(dir_items[j].name, name) == 0 && dir_items[j].type == type){ 
                return dir_items[j].inode_id;
            }
        }
    }
    return -1;
}

int read_file_inode(inode* file_node, char* ret){
    if (file_node->i_mode) return -1;    //不是文件则退出
    uint16_t indirect_blocks[128];
    char buf[BLOCK_SIZE];
    memset(ret, 0, sizeof(&ret));

    // if (file_node->i_link_count <= 8)
    //     for(int i = 0; i < file_node->i_link_count; i++){
    //         if (file_node->i_direct[i]==0) return -1;
    //         memset(buf, 0, BLOCK_SIZE);
    //         if (read_block(file_node->i_direct[i], buf)<0) return -1;
    //         memcpy(ret + strlen(ret), buf, BLOCK_SIZE);  // 使用 memcpy 将块数据复制到目标字符串中，因为strcpy需要null结尾但分block可能没有！
    //     }
    // else{
    //     if (file_node->i_single_indirect == 0) return -1;
    //     if (read_block(file_node->i_single_indirect, buf) < 0) return -1;
    //     memcpy(&indirect_blocks, buf, BLOCK_SIZE);
    //     for (int i=0; i < file_node->i_link_count - 8; i++){
    //         if (indirect_blocks[i] == 0) return -1;
    //         if (read_block(indirect_blocks[i], buf)<0) return -1;
    //         memcpy(ret + strlen(ret), buf, BLOCK_SIZE); 
    //     }
    // }
    // return 0;
    int total_blocks = file_node->i_link_count;
    int last_block_size = file_node->i_size % BLOCK_SIZE;
    if (last_block_size == 0 && total_blocks > 0) last_block_size = BLOCK_SIZE;

    if (total_blocks <= 8)
        for(int i = 0; i < total_blocks; i++){
            if (file_node->i_direct[i]==0) return -1;
            memset(buf, 0, BLOCK_SIZE);
            if (read_block(file_node->i_direct[i], buf)<0) return -1;
            int size_to_copy = (i == total_blocks - 1) ? last_block_size + 1 : BLOCK_SIZE;
            memcpy(ret + i * BLOCK_SIZE, buf, size_to_copy);
            // printf("%d %ld\n", i, strlen(ret));
        }
    else{
        for(int i = 0; i < 8; i++){
            if (file_node->i_direct[i]==0) return -1;
            memset(buf, 0, BLOCK_SIZE);
            if (read_block(file_node->i_direct[i], buf)<0) return -1;
            memcpy(ret + i * BLOCK_SIZE, buf, BLOCK_SIZE);
        }
        if (file_node->i_single_indirect == 0) return -1;
        if (read_block(file_node->i_single_indirect, buf) < 0) return -1;
        memcpy(&indirect_blocks, buf, BLOCK_SIZE);
        for (int i=0; i < total_blocks - 8; i++){
            if (indirect_blocks[i] == 0) return -1;
            if (read_block(indirect_blocks[i], buf)<0) return -1;
            int size_to_copy = (i == total_blocks - 8 - 1) ? last_block_size : BLOCK_SIZE;
            memcpy(ret + (i + 8) * BLOCK_SIZE, buf, size_to_copy);
        }
    }
    return 0;
}

int write_file_inode(inode* file_node, char* src){
    if (file_node->i_mode) return -1;    //不是文件则退出
    uint16_t indirect_blocks[128];
    memset(indirect_blocks, 0 ,sizeof(indirect_blocks));
    int need_link_count = (strlen(src) + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int ori_link_count = file_node->i_link_count;

    // printf("need: %d, ori: %d\n", need_link_count, ori_link_count);
    char tmp[need_link_count * BLOCK_SIZE];
    memset(tmp, 0, sizeof(tmp));
    memcpy(tmp, src, strlen(src) + 1);
    char buf[BLOCK_SIZE];

    if (need_link_count <= ori_link_count){
        //写入
        for(int i = 0; i < (need_link_count>=8 ? 8:need_link_count); i++){
            memcpy(buf, tmp + i*BLOCK_SIZE, BLOCK_SIZE);
            write_block(file_node->i_direct[i], buf, 1);
        }
        if (read_block(file_node->i_single_indirect, buf) < 0) return -1;
        memcpy(&indirect_blocks, buf, BLOCK_SIZE);
        for(int i = 0; i < (need_link_count - 8); i++){
            memcpy(buf, tmp + (i+8)*BLOCK_SIZE, BLOCK_SIZE);
            write_block(indirect_blocks[i], buf, 1);
        }

        //归还多余block
        for(int i = need_link_count; i < (ori_link_count>=8 ? 8:ori_link_count); i++){
            free_block(file_node->i_direct[i]);
            file_node->i_direct[i] = 0;
        }
        if (ori_link_count > 8 && file_node->i_single_indirect!=0){
            for(int i = (need_link_count<8?0:need_link_count-8); i < ori_link_count - 8; i++){
                free_block(indirect_blocks[i]);
                indirect_blocks[i] = 0;
            }
            if (need_link_count <= 8){
                free_block(file_node->i_single_indirect);
                file_node->i_single_indirect = 0;
            }
            else{
                memcpy(buf, &indirect_blocks, BLOCK_SIZE);
                write_block(file_node->i_single_indirect, buf, 1);
            }
        }
    }
    else{
        //申请不足的新结点
        if (need_link_count - ori_link_count > spb.s_free_blocks_count) return -1;
        for(int i=ori_link_count; i<(need_link_count > 8 ? 8 : need_link_count); i++){
            uint16_t new_block_id = alloc_block();
            if (new_block_id < 0) return -1;
            file_node->i_direct[i] = new_block_id;
        }   

        if (need_link_count > 8){
            if (ori_link_count <=8 && file_node->i_single_indirect==0){
                uint16_t new_block_id = alloc_block();
                if (new_block_id < 0) return -1;
                file_node->i_single_indirect = new_block_id;
                memset(indirect_blocks, 0 ,sizeof(indirect_blocks));
            }
            else{
                if (read_block(file_node->i_single_indirect, buf) < 0) return -1;
                memcpy(&indirect_blocks, buf, BLOCK_SIZE);
            }
            for(int i=(ori_link_count<8?0:ori_link_count-8); i < need_link_count - 8; i++){
                uint16_t new_block_id = alloc_block();
                if (new_block_id < 0) return -1;
                indirect_blocks[i] = new_block_id;
            }
            memcpy(buf, &indirect_blocks, BLOCK_SIZE);
            write_block(file_node->i_single_indirect, buf, 1);
        }
        //写入
        for(int i = 0; i < (need_link_count>=8 ? 8:need_link_count); i++){
            memcpy(buf, tmp + i*BLOCK_SIZE, BLOCK_SIZE);
            write_block(file_node->i_direct[i], buf, 1);
        }
        if (read_block(file_node->i_single_indirect, buf) < 0) return -1;
        memcpy(&indirect_blocks, buf, BLOCK_SIZE);
        for(int i = 0; i < (need_link_count - 8); i++){
            memcpy(buf, tmp + (i+8)*BLOCK_SIZE, BLOCK_SIZE);
            write_block(indirect_blocks[i], buf, 1);
        }
    }
    file_node->i_link_count = need_link_count; //更新连接数
    file_node->i_size = strlen(src);    //size储存文件实际长度
    file_node->i_timestamp = time(NULL);
    write_inode(file_node, file_node->i_index);
    return 0;
}