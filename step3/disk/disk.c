#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>

#define SECTOR_SIZE 256
#define BUFFER_SIZE 256

char response[BUFFER_SIZE] = "";
char timestamp[20];
double track_to_track_delay;

/*增加到response*/
void _printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int current_length = strlen(response);

    // 格式化并追加
    vsnprintf(response + current_length, BUFFER_SIZE - current_length, format, args);

    va_end(args);
}

/*获取格式化的时间*/
void get_formatted_timestamp(char *timestamp, size_t timestamp_size) {
    time_t current_time;
    struct tm *time_info;
    current_time = time(NULL);
    time_info = localtime(&current_time);
    // Format
    strftime(timestamp, timestamp_size, "%Y-%m-%d %H:%M:%S", time_info);
}

typedef struct{
    char *filename;
    int fd;
    int num_cylinders;
    int sectors_per_cylinder;
    double track_to_track_delay;
    char *diskfile;
    FILE *log_fp;
} SimDisk;

//Initialize disk struction
void Init(SimDisk *disk, int num_cylinders, int sectors_per_cylinder, double track_to_track_delay, const char *filename)
{
    disk->num_cylinders = num_cylinders;
    disk->sectors_per_cylinder = sectors_per_cylinder;
    disk->track_to_track_delay = track_to_track_delay;
    disk->filename = strdup(filename);

    disk->fd = open(filename, O_RDWR | O_CREAT, 0666);
    if (disk->fd < 0)
    {
        printf("Error: Could not open file '%s'.\n", filename);
        exit(-1);
    }

    // Calculate the disk storage size
    long filesize = num_cylinders * sectors_per_cylinder * SECTOR_SIZE;

    int result = lseek(disk->fd, filesize - 1, SEEK_SET);
    if (result == -1)
    {
        perror("Error calling lseek() to 'stretch' the file");
        close(disk->fd);
        exit(-1);
    }

    result = write(disk->fd, "", 1);
    if (result != 1)
    {
        perror("Error writing last byte of the file");
        close(disk->fd);
        exit(-1);
    }

    // Map the disk storage file to memory
    disk->diskfile = mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, disk->fd, 0);
    if (disk->diskfile == MAP_FAILED)
    {
        close(disk->fd);
        printf("Error: Could not map file.\n");
        exit(-1);
    }

    disk->log_fp = fopen("disk.log", "w");
}

//Destruct before exit
void Destructor(SimDisk *disk)
{
    free(disk->filename);
    fclose(disk->log_fp);

    // Unmap the disk storage file from memory
    if (munmap(disk->diskfile, disk->num_cylinders * disk->sectors_per_cylinder * SECTOR_SIZE) == -1)
    {
        perror("Error unmapping disk storage file");
        get_formatted_timestamp(timestamp, sizeof(timestamp));
        fprintf(disk->log_fp, "[%s] [ERROR] Failed to unmap disk storage file\n", timestamp);
    }

    // Close the disk storage file
    if (close(disk->fd) == -1)
    {
        perror("Error closing disk storage file");
    }
}

typedef struct Command {
    bool (*judge_trigger)(const char* cmd_head);
    int (*handle)(SimDisk *disk, int block_id, char* data);
} Command;

bool I_judgeTrigger(const char* cmd_head){ return cmd_head[0]=='I'; }
bool R_judgeTrigger(const char* cmd_head){ return cmd_head[0]=='R'; }
bool W_judgeTrigger(const char* cmd_head){ return cmd_head[0]=='W'; }
bool E_judgeTrigger(const char* cmd_head){ return cmd_head[0]=='E'; }

int I_handle(SimDisk *disk, int block_id, char* data){
    get_formatted_timestamp(timestamp, sizeof(timestamp));
    fprintf(disk->log_fp, "[%s] [INFO] I commands executed successfully.\n", timestamp);
    get_formatted_timestamp(timestamp, sizeof(timestamp));
    fprintf(disk->log_fp, "[%s] [DEBUG] Disk info: %d cylinders, %d sectors per cylinder.\n", timestamp, disk->num_cylinders, disk->sectors_per_cylinder);
    _printf("%d %d\n", disk->num_cylinders, disk->sectors_per_cylinder);
    return 0;
}

int R_handle(SimDisk *disk, int block_id, char* data){
    int cylinder = block_id / disk->sectors_per_cylinder;
    int sector = block_id % disk->sectors_per_cylinder;
    if (cylinder < 0 || cylinder >= disk->num_cylinders || sector < 0 || sector >= disk->sectors_per_cylinder)
    {
        _printf("Disk Instruction error!\n");
        get_formatted_timestamp(timestamp, sizeof(timestamp));
        fprintf(disk->log_fp, "[%s] [ERROR] R commands error: invaild sector or cyclinder number.\n", timestamp);
        return 1;
    }
    char *sector_data = disk->diskfile + block_id * SECTOR_SIZE;
    get_formatted_timestamp(timestamp, sizeof(timestamp));
    fprintf(disk->log_fp, "[%s] [INFO] R commands executed successfully.\n", timestamp);
    memcpy(response, sector_data, SECTOR_SIZE);
    return 0;
}

int W_handle(SimDisk *disk, int block_id, char* data){
    int cylinder = block_id / disk->sectors_per_cylinder;
    int sector = block_id % disk->sectors_per_cylinder;
    if (cylinder < 0 || cylinder >= disk->num_cylinders || sector < 0 || sector >= disk->sectors_per_cylinder)
    {
        _printf("Disk Instruction error!\n");
        get_formatted_timestamp(timestamp, sizeof(timestamp));
        fprintf(disk->log_fp, "[%s] [ERROR] W command error: invaild sector or cyclinder number.\n", timestamp);
        return 1;
    }
    char *sector_data = disk->diskfile + block_id * SECTOR_SIZE;
    memcpy(sector_data, data, SECTOR_SIZE);
    get_formatted_timestamp(timestamp, sizeof(timestamp));
    fprintf(disk->log_fp, "[%s] [INFO] W commands executed successfully.\n", timestamp);
    return 0;
}

int E_handle(SimDisk *disk, int block_id, char* data){
    get_formatted_timestamp(timestamp, sizeof(timestamp));
    fprintf(disk->log_fp, "[%s] [INFO] Exited.\n", timestamp);
    Destructor(disk);
    return -1;
}

Command commands[] = {
    {I_judgeTrigger, I_handle},
    {R_judgeTrigger, R_handle},
    {W_judgeTrigger, W_handle},
    {E_judgeTrigger, E_handle},
};
const int num_commands = sizeof(commands) / sizeof(Command);

typedef struct input_cmd{
    char type;
    int block_no;
    char info[SECTOR_SIZE];
} input_cmd;

//处理来自fs的请求
void handle_fs_client(SimDisk *disk , int client_socket) {
    input_cmd raw_command;
    int block_id;
    char data[SECTOR_SIZE];
    char cmdtype[BUFFER_SIZE];
    int ret;

    //use for simulating track-to-track delay
    int pre = 0;
    int delta;

    while (1) {
        memset(&raw_command, 0, sizeof(raw_command));
        memset(response, 0, sizeof(response));
        ssize_t ret = recv(client_socket, &raw_command, sizeof(raw_command), 0);

        if (ret <= 0) {
            printf("Failed to read from fs client, disconnecting.\n");
            break;
        }
        if (strlen(raw_command.info) == 1 && raw_command.info[0] == '\n')
            continue; // 继续下一次循环

        printf("Received disk command: %c %d\n", raw_command.type, raw_command.block_no);
        get_formatted_timestamp(timestamp, sizeof(timestamp));
        fprintf(disk->log_fp, "[%s] [INFO] Received disk command:: %c %d\n",timestamp, raw_command.type, raw_command.block_no);

        bool handled = false;
        sprintf(cmdtype, "%c", raw_command.type);
        block_id = raw_command.block_no;
        memcpy(data, raw_command.info, SECTOR_SIZE);
        for (int i = 0; i < num_commands; i++) {
            if (commands[i].judge_trigger(cmdtype)) {
                ret = commands[i].handle(disk, block_id, data);
                handled = true;
                break;
            }
        }
        if (handled && block_id >= 0 && ret!=1){
            delta = abs((block_id / disk->sectors_per_cylinder) - pre);
            pre = (block_id / disk->sectors_per_cylinder);
            // usleep(delta * track_to_track_delay);
            get_formatted_timestamp(timestamp, sizeof(timestamp));
            fprintf(disk->log_fp, "[%s] [INFO] Track to Track Delay: %.2lf\n", timestamp, delta * track_to_track_delay);
        }
        if (!handled) {
            _printf("Undefined command!\n");
        } else if (ret == -1) {
            _printf("Exit\n");
            break;
        }

        printf("Response:");
        for(int i = 0; i< SECTOR_SIZE; i++) printf("%c", response[i]);
        printf("\n");

        if (send(client_socket, response, sizeof(response), 0) < 0) {
            perror("Failed to send response to client");
            }
        
    }
}

int main(int argc, char *argv[])
{
    if (argc != 6)
    {
        printf("Usage: %s <cylinders> <sector per cylinder> <track-to-track delay> <disk-storage-filename> <DiskPort>\n", argv[0]);
        return 1;
    }

    int num_cylinders = atoi(argv[1]);
    int sectors_per_cylinder = atoi(argv[2]);
    char *endptr;
    track_to_track_delay = strtod(argv[3], &endptr);
    const char *filename = argv[4];
    int port = atoi(argv[5]);

    SimDisk disk;
    Init(&disk, num_cylinders, sectors_per_cylinder, track_to_track_delay, filename);

    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    int client_socket;

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Failed to create socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to bind socket");
        return 1;
    }
    if (listen(server_socket, 1) < 0) {
        perror("Failed to listen on socket");
        return 1;
    }

    printf("Disk server started. Listening on port %d\n", port);

    while (1) {
        client_len = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Failed to accept connection");
            continue;
        }

        printf("New FS client connected. Client socket: %d\n", client_socket);
        handle_fs_client(&disk, client_socket);
        printf("FS client disconnected. Client socket: %d\n", client_socket);
        close(client_socket);
    }

    close(server_socket);

    return 0;
}