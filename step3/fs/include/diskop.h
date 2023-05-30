#ifndef DISKOP_H
#define DISKOP_H

#include <stdio.h>

#include "superblock.h"

char disk_file[MAX_BLOCK_COUNT][BLOCK_SIZE];    //磁盘数据文件
FILE *log_fp;                           //磁盘日志文件

/**
 * @brief 将数据写入块
 * @return 返回0代表成功，-1代表失败
 * @param block_no--写入起始块号
 * @param buf--要写入的数据
 * @param block_num--写入块的数量（默认连续写入且不做覆盖检查，检查应在上游函数完成） 
 */
int write_block(int block_no, char *buf, int block_num);

/**
 * @brief 将块中数据读出
 * @return 返回0代表成功，-1代表失败
 * @param block_no--读数据的块号
 * @param buf--读出数据写入buf
 */
int read_block(int block_no, char *buf);

/**
 * @brief 查询磁盘信息
 * @return 返回-1失败，正数则为磁盘总块数
*/
int get_disk_info();

#endif