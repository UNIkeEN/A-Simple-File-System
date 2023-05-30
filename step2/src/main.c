#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "superblock.h"
#include "inode.h"
#include "diskop.h"
#include "dir.h"

#define COMMAND_LENGTH 4096
#define MAX_PATH_STR_LENGTH 29696 //inode_num(1024)*(max_name_size(28)+'/'(1)）

uint16_t current_dir = 65535;               //当前目录结点编号
char cur_path_string[MAX_PATH_STR_LENGTH];  //当前目录字符串
bool formatted = false;                             //是否格式化

/** @brief 检查名称合法性（只允许数字、字母、_和.）
 * @return 返回1表示不合法，0表示合法 */
int check_name(char *text){
    int i = 0;
    char a = text[0];
    while(a!='\0'){
        if((a>='a'&&a<='z') || (a>='A'&&a<='Z') || (a>='0' && a<='9') || a=='_' || a=='.'){
            a = text[++i];
        }
        else {
            printf("Illegal name! (Only accept numbers ,letters ,_ and . )\n");
            return 1;
        }
    }
    if (i > MAX_NAME_SIZE) {
        printf("Illegal name! (Must be shorter than 28)\n");
        return 1;
    }
    return 0;
}

/** @brief 初始化文件系统 */
int format_handle(){
    init_spb();                             //初始化super block
    if(init_root_inode() < 0){              //初始化root inode
        printf("Init root inode failed.");
        return 0;
    };
    current_dir = 0;                        //初始化当前目录为root
    strcpy(cur_path_string, "/");
    for (int i = 1; i < MAX_INODE_COUNT; i++) { //初始化inode table
        if (init_inode(&inode_table[i], i, 0, 0, 0, 65535)<0) {
            printf("Init inode failed.\n");
            return 0;
        }
    }
    for (int i = 0; i < MAX_INODE_COUNT; i++){  //写回inode table到diskfile
        if (write_inode(&inode_table[i], i)<0) {
            printf("Write Inode failed.\n");
            return 0;
        }
    }
    fprintf(log_fp, "Done\n");
    formatted = true;
    return 0;
}

/** @brief 创建文件
 * @param params 格式<f>，f为文件名*/
int mk_handle(char* params){
    if (!formatted) {fprintf(log_fp, "No\n"); printf("Please first format.\n"); return 0;}
    if (check_name(params)) {fprintf(log_fp, "No\n"); return 0; }     //文件名非法
    if (search_in_dir_inode(&inode_table[current_dir], params, 0) >=0 ) {   //重名
        fprintf(log_fp, "No\n"); 
        printf("A file with the same name already exists!\n");
        return 0; 
    }
    if (add_to_dir_inode(&inode_table[current_dir], params, 0) < 0)
        fprintf(log_fp, "No\n");
    else fprintf(log_fp, "Yes\n");
    return 0;
}

/** @brief 创建文件夹
 * @param params 格式<f>，f为文件夹名*/
int mkdir_handle(char* params){
    if (!formatted) {fprintf(log_fp, "No\n"); printf("Please first format.\n"); return 0;}
    if (check_name(params)) {fprintf(log_fp, "No\n"); return 0; }     //文件夹名非法
    if (search_in_dir_inode(&inode_table[current_dir], params, 1) >=0 ) {   //重名
        fprintf(log_fp, "No\n"); 
        printf("A sub-dir with the same name already exists!\n");
        return 0; 
    }
    if (add_to_dir_inode(&inode_table[current_dir], params, 1) < 0)
        fprintf(log_fp, "No\n");
    else fprintf(log_fp, "Yes\n");
    return 0;
}

/** @brief 删除文件
 * @param params 格式<f>，f为文件名*/
int rm_handle(char* params){
    if (!formatted) {fprintf(log_fp, "No\n"); printf("Please first format.\n"); return 0;}
    if (check_name(params)) {fprintf(log_fp, "No\n"); return 0; }     //文件名非法
    if (rm_from_dir_inode(&inode_table[current_dir], params) < 0)
        fprintf(log_fp, "No\n");
    else fprintf(log_fp, "Yes\n");
    return 0;
}

/** @brief 更改路径
 * @param params 格式<f>，f为路径字符串*/
int cd_handle(char* params){
    if (!formatted) {fprintf(log_fp, "No\n"); printf("Please first format.\n"); return 0;}

    uint16_t ori_dir = current_dir;     //先保存当前dir_id和路径字符串
    char ori_path_string[MAX_PATH_STR_LENGTH]; 
    strcpy(ori_path_string, cur_path_string);

    char* path = strtok(params, "/"); 
    while (path != NULL) {
        if (strcmp(path, "..") == 0) {  //回到上一级
            uint16_t parent = inode_table[current_dir].i_parent;
            if (parent != 65535) {      //65535为无父母或为root
                current_dir = parent; 

                if (strcmp(cur_path_string, "/") != 0) {
                    char* last_slash = strrchr(cur_path_string, '/');
                    *last_slash = '\0';
                }
                if (parent == 0) strcpy(cur_path_string, "/");
            }
        } else if (strcmp(path, ".") != 0) {    //.代表当前目录，无视之
            int result = search_in_dir_inode(&inode_table[current_dir], path, 1);
            if (result == -1) {
                printf("Directory not found.\n");
                current_dir = ori_dir;  // 找不到对应文件夹，回退
                strcpy(cur_path_string, ori_path_string);
                return 0; 
            }
            if (current_dir!=0) strcat(cur_path_string, "/");
            current_dir = result;
            strcat(cur_path_string, path);
        }
        path = strtok(NULL, "/"); 
    }
    return 0;
}

/** @brief 删除文件夹
 * @param params 格式<f>，f为文件夹名*/
int rmdir_handle(char* params){
    if (!formatted) {fprintf(log_fp, "No\n"); printf("Please first format.\n"); return 0;}
    if (check_name(params)) {fprintf(log_fp, "No\n"); return 0; }     //文件名非法
    if (rmdir_from_dir_inode(&inode_table[current_dir], params) < 0)
        fprintf(log_fp, "No\n");
    else fprintf(log_fp, "Yes\n");
    return 0;
}

/** @brief 列出文件夹和文件名称*/
int ls_handle(){
    if (!formatted) {fprintf(log_fp, "No\n"); printf("Please first format.\n"); return 0;}
    char buf[64 * MAX_NAME_SIZE];
    if (ls_dir_inode(&inode_table[current_dir], buf)==0) 
        fprintf(log_fp, "%s", buf);
    return 0;
}

/** @brief 读取文件内容
 * @param params 格式<f>，f为文件名*/
int cat_handle(char* params){
    if (!formatted) {fprintf(log_fp, "No\n"); printf("Please first format.\n"); return 0;}
    // char buf[MAX_LINKS_PER_NODE * BLOCK_SIZE];
    char buf[65535];
    int ret_id;
    if (check_name(params)) {fprintf(log_fp, "No\n"); return 0; }     //文件名非法
    if ((ret_id = search_in_dir_inode(&inode_table[current_dir], params, 0)) < 0){   //文件不存在
        fprintf(log_fp, "No\n");
        printf("File does not exist!\n");
        return 0;
    }
    if (read_file_inode(&inode_table[ret_id], buf)==0){ 
        fprintf(log_fp, "%s\n", buf);
        printf("%s\n", buf);
    }
    else fprintf(log_fp, "No\n");
    return 0;
}

/** @brief 读取文件内容
 * @param params 格式<f> <l> <data>，f为文件名，l为长度，data为写入数据*/
int w_handle(char* params){
    if (!formatted) {fprintf(log_fp, "No\n"); printf("Please first format.\n"); return 0;}
    char buf[MAX_LINKS_PER_NODE * BLOCK_SIZE];
    char file_name[MAX_NAME_SIZE];
    int ret_id = 0;
    int len = 0;
    memset(buf, 0, sizeof(buf));
    sscanf(params, "%s %d %[^\n]", file_name, &len, buf);
    if (check_name(file_name) || (len==0)) {fprintf(log_fp, "No\n"); return 0; }     //文件名非法
    if ((ret_id = search_in_dir_inode(&inode_table[current_dir], file_name, 0)) < 0){   //文件不存在
        fprintf(log_fp, "No\n");
        printf("File does not exist!\n");
        return 0;
    }
    if (len!=strlen(buf)) {printf("\033[1;33mWarning: The string length parameter does not match the actual length!\033[0m\n"); len=strlen(buf);}
    if (write_file_inode(&inode_table[ret_id], buf)==0) 
        fprintf(log_fp, "Yes\n");
    else fprintf(log_fp, "No\n");
    return 0;
}

/** @brief 插入
 * @param params 格式<f> <pos> <l> <data>，f为文件名，pos为位置，l为长度，data为写入数据*/
int i_handle(char* params){
    if (!formatted) {fprintf(log_fp, "No\n"); printf("Please first format.\n"); return 0;}
    char buf[MAX_LINKS_PER_NODE * BLOCK_SIZE];
    char src[MAX_LINKS_PER_NODE * BLOCK_SIZE];
    char file_name[MAX_NAME_SIZE];
    int ret_id = 0, len = 0, pos = 0;
    memset(src, 0, MAX_LINKS_PER_NODE * BLOCK_SIZE);
    sscanf(params, "%s %d %d %[^\n]", file_name, &pos, &len, src);
    if (check_name(file_name) || (len==0)) {fprintf(log_fp, "No\n"); return 0; }     //文件名非法
    if ((ret_id = search_in_dir_inode(&inode_table[current_dir], file_name, 0)) < 0){   //文件不存在
        fprintf(log_fp, "No\n");
        printf("File does not exist!\n");
        return 0;
    }
    if (len!=strlen(src)) {printf("\033[1;33mWarning: The string length parameter does not match the actual length!\033[0m\n"); len=strlen(src);}
        
    int file_size = inode_table[ret_id].i_size; // 获取文件当前的大小
    if (pos > file_size) {
        printf("\033[1;33mWarning: The <pos> parameter is greater than the file size and is appended to the end of the file!\033[0m\n");
        pos = file_size; // 如果 pos 大于文件大小，则将数据插入到文件末尾
    }
    int new_size = file_size + len;
    if (new_size > MAX_LINKS_PER_NODE * BLOCK_SIZE - 1){fprintf(log_fp, "No\n"); return 0;}  //过长
    memset(buf, 0, MAX_LINKS_PER_NODE * BLOCK_SIZE);
    int ret = read_file_inode(&inode_table[ret_id], buf);
    if (ret < 0) { fprintf(log_fp, "No\n"); return 0;} // 读取文件内容失败，处理错误情况

    // 在指定位置插入数据
    memmove(buf + pos + len, buf + pos, file_size - pos); // 向后移动数据
    memcpy(buf + pos, src, len); // 插入数据
    buf[new_size] = '\0';

    if (write_file_inode(&inode_table[ret_id], buf)==0) 
        fprintf(log_fp, "Yes\n");
    else fprintf(log_fp, "No\n");
    return 0;
}

/** @brief 删除
 * @param params 格式<f> <pos> <l>，f为文件名，pos为位置，l为长度*/
int d_handle(char* params){
    if (!formatted) {fprintf(log_fp, "No\n"); printf("Please first format.\n"); return 0;}
    char buf[MAX_LINKS_PER_NODE * BLOCK_SIZE];
    char src[MAX_LINKS_PER_NODE * BLOCK_SIZE];
    char file_name[MAX_NAME_SIZE];
    int ret_id = 0, len = 0, pos = 0;
    sscanf(params, "%s %d %d", file_name, &pos, &len);
    if (check_name(file_name) || (len==0)) {fprintf(log_fp, "No\n"); return 0; }     //文件名非法
    if ((ret_id = search_in_dir_inode(&inode_table[current_dir], file_name, 0)) < 0){   //文件不存在
        fprintf(log_fp, "No\n");
        printf("File does not exist!\n");
        return 0;
    }
        
    int file_size = inode_table[ret_id].i_size; // 获取文件当前的大小
    if (pos > file_size) { fprintf(log_fp, "No\n"); return 0;}
    if (pos + len > file_size) len = file_size - pos;
    memset(buf, 0, MAX_LINKS_PER_NODE * BLOCK_SIZE);
    int ret = read_file_inode(&inode_table[ret_id], buf);
    if (ret < 0) { fprintf(log_fp, "No\n"); return 0;} // 读取文件内容失败，处理错误情况

    // 在指定位置删除数据

    // memmove(buf + pos, buf + pos + len, file_size - pos - len);
    memcpy(src, buf, pos);
    memcpy(src + pos, buf + pos + len, file_size - pos - len);
    src[file_size-len] = '\0';
    // printf("buf: %s\n", buf);

    if (write_file_inode(&inode_table[ret_id], src)==0) 
        fprintf(log_fp, "Yes\n");
    else fprintf(log_fp, "No\n");
    return 0;
}

/** @brief 退出系统 */
int exit_handle(){
    printf("Goodbye!\n");
    fprintf(log_fp, "Goodbye!\n");
    fclose(log_fp);
    return -1;  //  返回-1使main中循环退出
}

/** @brief 显示块内容
 * @param params 格式<id>，块id*/
int debug_getb_handle(char *params){
    int id;
    sscanf(params, "%d", &id);
    if (id<0 || id>=MAX_BLOCK_COUNT) return 0;
    char buf[BLOCK_SIZE];
    read_block(id, buf);
    for(int i=0; i<BLOCK_SIZE;i++){
        if (buf[i]=='\0') printf("\033[1;34m%c\033[0m", '$');
        else printf("%c", buf[i]);
    }
    printf("\n\033[1;34m$\033[0m represents'\\0'\n");
    return 0;
}

/** @brief 显示inode内容
 * @param params 格式<id>，inode id*/
int debug_geti_handle(char *params){
    int id;
    sscanf(params, "%d", &id);
    if (id<0 || id>=MAX_INODE_COUNT) return 0;
    inode* node = &inode_table[id];
    if (node->i_mode == 0){
        printf("\033[1;33mThis is a file inode\033[0m\n");
        printf(" - index: %d\n", node->i_index);
        printf(" - mode: %d\n", node->i_mode);
        printf(" - link_count: %d\n", node->i_link_count);
        printf(" - size: %d\n", node->i_size);
        printf(" - timestamp: %d\n", node->i_timestamp);
        printf(" - parent: %d\n", node->i_parent);
        printf(" - direct: {");
        for (int i = 0; i < 8; i++) printf("%d,",node->i_direct[i]);
        printf("}\n - single_indirect: %d\n", node->i_single_indirect);
        if (node->i_single_indirect != 0){
            char buf[BLOCK_SIZE];
            uint16_t indirect_blocks[128];
            read_block(node->i_single_indirect, buf);
            memcpy(&indirect_blocks, buf, BLOCK_SIZE);
            printf("{");
            for(int i = 0; i < 128; i++) printf("%d,",indirect_blocks[i]);
            printf("}\n");
        }
    }
    else {
        printf("\033[1;33mThis is a dir inode\033[0m\n");
        printf(" - index: %d\n", node->i_index);
        printf(" - mode: %d\n", node->i_mode);
        printf(" - link_count: %d\n", node->i_link_count);
        printf(" - size: invalid\n" );
        printf(" - timestamp: %d\n", node->i_timestamp);
        if (node->i_parent == 65535) printf(" - parent: invalid\n");
        else printf(" - parent: %d\n", node->i_parent);
        printf(" - direct: {");
        for (int i = 0; i < 8; i++) printf("%d,",node->i_direct[i]);
        printf("}\n");
    }
    return 0;
}

typedef struct Command {
    char cmd_head[6];                   //命令名称，对每行输入的首段进行匹配
    int (*handle)(char* params);        //处理该命令的函数
} Command;

Command commands_list[] = {
    {"f", format_handle},
    {"mk", mk_handle},
    {"mkdir", mkdir_handle},
    {"rm", rm_handle},
    {"cd", cd_handle},
    {"rmdir", rmdir_handle},
    {"ls", ls_handle},
    {"cat", cat_handle},
    {"w", w_handle},
    {"i", i_handle},
    {"d", d_handle},
    {"gb", debug_getb_handle},          //DEBUG
    {"gi", debug_geti_handle},          //DEBUG
    {"e", exit_handle},
};

const int num_commands = sizeof(commands_list) / sizeof(Command);

int main(){
    char raw_command[COMMAND_LENGTH];
    char cmd_head[6];
    char params[COMMAND_LENGTH];
    int ret;
    log_fp = fopen("fs.log", "w");
    // printf("%ld bytes\n", sizeof(inode));

    while (1)
    {
        printf("\033[1;35mUNIkeEN-fs\033[0m:\033[1;36m%s\033[0m >> ", cur_path_string);
        fgets(raw_command, sizeof(raw_command), stdin);

        if (strlen(raw_command) == 1 && raw_command[0] == '\n') continue; // 继续下一次循环

        bool handled = false;
        for (int i = 0; i < num_commands; i++) {
            sscanf(raw_command, "%s %[^\n]%*c", cmd_head, params);
            if (strcmp(cmd_head, commands_list[i].cmd_head) == 0) {
                ret = commands_list[i].handle(params);
                handled = true;
                break;
            }
        }
        if (!handled) {
            printf("Undefined command!\n");
        }
        if (ret == -1) break; //E command means exit
    }
}
