#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdbool.h>

#define SECTOR_SIZE 256
#define BUFFER_SIZE 128

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
    }

    // Close the disk storage file
    if (close(disk->fd) == -1)
    {
        perror("Error closing disk storage file");
    }
}

typedef struct Command {
    bool (*judge_trigger)(const char* cmd_head);
    int (*handle)(SimDisk *disk, int cylinder, int sector, char* data);
} Command;

bool I_judgeTrigger(const char* cmd_head){ return cmd_head[0]=='I'; }
bool R_judgeTrigger(const char* cmd_head){ return cmd_head[0]=='R'; }
bool W_judgeTrigger(const char* cmd_head){ return cmd_head[0]=='W'; }
bool E_judgeTrigger(const char* cmd_head){ return cmd_head[0]=='E'; }

int I_handle(SimDisk *disk, int cylinder, int sector, char* data){
    fprintf(disk->log_fp, "%d %d\n", disk->num_cylinders, disk->sectors_per_cylinder);
    return 0;
}

int R_handle(SimDisk *disk, int cylinder, int sector, char* data){
    if (cylinder < 0 || cylinder >= disk->num_cylinders || sector < 0 || sector >= disk->sectors_per_cylinder)
    {
        printf("Instruction error!\n");
        fprintf(disk->log_fp, "No\n");
        return 1;
    }
    char *sector_data = disk->diskfile + (cylinder * disk->sectors_per_cylinder + sector) * SECTOR_SIZE;
    fprintf(disk->log_fp, "Yes ");
    int i = 0;
    while (sector_data[i])
    {
        fprintf(disk->log_fp, "%c", sector_data[i]);
        i++;
        if (i == SECTOR_SIZE) break;
    }
    fprintf(disk->log_fp, "\n");
    return 0;
}

int W_handle(SimDisk *disk, int cylinder, int sector, char* data){
    if (cylinder < 0 || cylinder >= disk->num_cylinders || sector < 0 || sector >= disk->sectors_per_cylinder)
    {
        printf("Instruction error!\n");
        fprintf(disk->log_fp, "No\n");
        return 1;
    }
    char *sector_data = disk->diskfile + (cylinder * disk->sectors_per_cylinder + sector) * SECTOR_SIZE;
    memcpy(sector_data, data, SECTOR_SIZE);
    fprintf(disk->log_fp, "Yes\n");
    return 0;
}

int E_handle(SimDisk *disk, int cylinder, int sector, char* data){
    fprintf(disk->log_fp, "Goodbye!\n");
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

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        printf("Usage: %s <cylinders> <sector per cylinder> <track-to-track delay> <disk-storage-filename>\n", argv[0]);
        return 1;
    }

    int num_cylinders = atoi(argv[1]);
    int sectors_per_cylinder = atoi(argv[2]);
    char *endptr;
    double track_to_track_delay = strtod(argv[3], &endptr);
    const char *filename = argv[4];

    SimDisk disk;
    Init(&disk, num_cylinders, sectors_per_cylinder, track_to_track_delay, filename);

    char raw_command[BUFFER_SIZE];
    int cylinder, sector;
    char data[SECTOR_SIZE];
    char cmdtype[BUFFER_SIZE];
    int ret;

    //use for simulating track-to-track delay
    int pre = 0;
    int delta;

    while (1)
    {
        fgets(raw_command, sizeof(raw_command), stdin);

        bool handled = false;
        for (int i = 0; i < num_commands; i++) {
            cylinder = -1; sector = -1;
            sscanf(raw_command, "%s %d %d %[^\n]", cmdtype, &cylinder, &sector, data);
            if (commands[i].judge_trigger(cmdtype)) {
                ret = commands[i].handle(&disk, cylinder, sector, data);
                handled = true;
                break;
            }
        }
        if (handled && cylinder >= 0 && ret!=1){
            delta = abs(cylinder - pre);
            pre = cylinder;
            usleep(delta * track_to_track_delay);
            printf("Track to Track Delay: %.2lf\n", delta * track_to_track_delay);
        }
        if (!handled) {
            printf("Undefined command!\n");
        }
        if (ret == -1) break; //E command means exit
    }

}