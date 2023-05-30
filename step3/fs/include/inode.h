#ifndef INODE_H
#define INODE_H

#include "superblock.h"
#include <stdint.h>

#define INODE_PER_BLOCK 8
#define INODE_TABLE_START_BLOCK 3 
#define MAX_LINKS_PER_NODE 136          //8 for direct, 128 for single-indirect

typedef struct inode{
    uint8_t i_mode;                     //0代表文件，1代表目录
    uint8_t i_link_count;               //已有的连接数量
    uint16_t i_size;                    //记录文件大小
    uint32_t i_timestamp;               //记录文件修改时间
    uint16_t i_parent;                  //父inode编号（65535表示无）
    uint16_t i_direct[8];               //直接块（记录block编号，block编号统一使用uine16_t）
    uint16_t i_single_indirect;         //一层间接块
    uint16_t reserve;
    uint16_t i_index;
} inode; //32 bytes

inode inode_table[MAX_INODE_COUNT];

/**
 * @brief 初始化root inode节点
 * @return 返回0代表成功，-1代表失败
 */
int init_root_inode();

/**
 * @brief 初始化inode节点
 * @return 返回0代表成功，-1代表失败
 */
int init_inode(inode* node, uint16_t index, uint8_t mode, uint8_t link, uint16_t size, uint16_t parent);

/**
 * @brief 写inode到inode_table的block
 * @param node 要写的inode数据
 * @param index 要写的inode在inode_table中的编号
 * @return 返回0代表成功，-1代表失败
 */
int write_inode(inode* node, uint16_t index);

/**
 * @brief 读disk的inode_table
 * @param node 要读取的inode数据
 * @param index 要读取的inode在inode_table中的编号
 * @return 返回0代表成功，-1代表失败
 */
int read_inode(inode* node, uint16_t index);

/**
 * @brief 寻找并分配空闲inode节点
 * @return 返回大于0代表成功（即新inode编号），-1代表失败
 */
int alloc_inode();

/**
 * @brief 将已分配inode节点还原
 * @param node 节点指针
 * @return 返回0代表成功，-1代表失败，1代表本块本身已经空闲
 */
int free_inode(inode* node);

/**
 * @brief 在目录结点下新增文件或文件夹
 * @param dir_node 当前所在位置dir_inode指针
 * @param name 要新建dir_item的名称
 * @param type 要新建dir_item的种类（0代表文件，1代表目录）
 * @return 返回0代表成功，-1代表失败
*/
int add_to_dir_inode(inode* dir_node, char* name, uint8_t type);

/**
 * @brief 从文件夹中删除文件dir_item
 * @param dir_node 目标路径dir_inode指针
 * @param name 要删除dir_item的名称
 * @return 返回0代表成功，-1代表失败
*/
int rm_from_dir_inode(inode* dir_node, char* name);

/**
 * @brief 从文件夹中删除文件夹dir_item
 * @param dir_node 目标路径dir_inode指针
 * @param name 要删除dir_item的名称
 * @return 返回0代表成功，-1代表失败
*/
int rmdir_from_dir_inode(inode* dir_node, char* name);

/**
 * @brief 检查dir_node是否为空，若空（无block或block中全valid = 0）则删除并归还block
 * @param dir_node 目标路径dir_inode指针
 * @return 返回0代表成功，-1代表失败
*/
int check_dir_isempty_delete(inode* dir_node);

/**
 * @brief 用于按字典序std-qsort时作为cmp函数
*/
int lexic_cmp(const void *a, const void *b);

/**
 * @brief 列出dir下所有文件夹和文件
 * @param dir_node 目标路径dir_inode指针
 * @param ret 结果字符串(写入log 不带颜色)
 * @param ret2 结果字符串(显示 带颜色)
 * @return 返回0代表成功，-1代表失败或空
*/
int ls_dir_inode(inode* dir_node, char *ret, char *ret2);

/**
 * @brief 按名称从文件夹寻找子文件夹或子文件
 * @param dir_node 目标路径dir_inode指针
 * @param name 目标dir_item名称
 * @return 返回大于等于0代表成功（即子项的inode_id），失败返回-1
*/
int search_in_dir_inode(inode* dir_node, char* name, int type);

/**
 * @brief 读文件inode的内容
 * @param file_node 目标文件file_inode指针
 * @param ret 将读取结果返回的字符串
 * @return 返回0代表成功，失败返回-1
*/
int read_file_inode(inode* file_node, char* ret);

/**
 * @brief 写文件inode的内容
 * @param file_node 目标文件file_inode指针
 * @param src 要写入的字符串
 * @return 返回0代表成功，失败返回-1
*/
int write_file_inode(inode* file_node, char* src);

#endif