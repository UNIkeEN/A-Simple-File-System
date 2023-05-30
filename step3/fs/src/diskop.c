#include "superblock.h"
#include "diskop.h"
#include "dir.h"

#include <sys/socket.h>
#include <string.h>

char disk_command[BLOCK_SIZE+10];
char tmp[BLOCK_SIZE];
extern int disk_server_socket;

typedef struct input_cmd{
    char type;
    int block_no;
    char info[BLOCK_SIZE];
} input_cmd;

int write_block(int block_no, char *buf, int block_num){
    if (block_no + block_num - 1 < 0 || block_no + block_num > MAX_BLOCK_COUNT) return -1;
    for (int i = 0; i < block_num; i++) {
        // memset(disk_command, 0, sizeof(disk_command));
        // int offset = snprintf(disk_command, sizeof(disk_command), "W %d ", block_no + i);
        // memcpy(disk_command + offset, tmp, sizeof(tmp));
        input_cmd disk_command;
        disk_command.type = 'W';
        disk_command.block_no = block_no + i;
        memcpy(disk_command.info, buf + BLOCK_SIZE * i, BLOCK_SIZE);

        if (send(disk_server_socket, &disk_command, sizeof(disk_command), 0) < 0) {
            perror("Failed to send command to disk server");
            return -1;
        }
        memset(tmp, 0, sizeof(tmp));
        int ret = recv(disk_server_socket, tmp, BLOCK_SIZE, 0);
        if (ret <= 0) {
            perror("Failed to receive response from disk server");
            return -1;
        }
    }
    return 0;
}

int read_block(int block_no, char *buf){
    if (block_no < 0 || block_no > MAX_BLOCK_COUNT) return -1;
    input_cmd disk_command;
    disk_command.type = 'R';
    disk_command.block_no = block_no;
    if (send(disk_server_socket, &disk_command, sizeof(disk_command), 0) < 0) {
        perror("Failed to send command to disk server");
        return -1;
        }
    memset(buf, 0, BLOCK_SIZE);
    int ret = recv(disk_server_socket, buf, BLOCK_SIZE, 0);
    if (ret <= 0) {
        perror("Failed to receive response from disk server");
        return -1;
    }
    // memcpy(buf, disk_file[block_no], BLOCK_SIZE);
    return 0;
}

int get_disk_info(){
    input_cmd disk_command;
    disk_command.type = 'I';
    disk_command.block_no = -1;
    if (send(disk_server_socket, &disk_command, sizeof(disk_command), 0) < 0) {
            perror("Failed to send command to disk server");
            return -1;
    }
    memset(tmp, 0, sizeof(tmp));
    int ret = recv(disk_server_socket, tmp, BLOCK_SIZE, 0);
    if (ret <= 0) {
        perror("Failed to receive response from disk server");
        return -1;
    }
    int num1, num2;
    sscanf(tmp, "%d %d", &num1, &num2);
    return num1*num2;
}