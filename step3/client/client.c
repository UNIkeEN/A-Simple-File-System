#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define COMMAND_LENGTH 4096
#define RESPONSE_LENGTH 4096

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);

    struct sockaddr_in server_addr;
    int client_socket;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Failed to create socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  // 本机
    server_addr.sin_port = htons(port);

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to connect to server");
        return 1;
    }

    printf("Connected to server\n");

    char raw_command[COMMAND_LENGTH];
    char cur_path_string[128] = "$";
    char response[RESPONSE_LENGTH];
    ssize_t ret;

    //初始挂载 or 不挂载信息
    ret = recv(client_socket, response, sizeof(response), 0);
    if (ret <= 0) {
        perror("Failed to receive response from server");
    }
    else printf("%s\n", response);

    while (1) {
        memset(raw_command, 0, sizeof(raw_command));
        memset(cur_path_string, 0, sizeof(cur_path_string));
        // memset(response, 0, sizeof(response));
        ret = recv(client_socket, cur_path_string, sizeof(cur_path_string), 0);
        if (ret <= 0) {
            perror("Failed to receive cur_path_string from server");
            break;
        }
        printf("\033[1;35mUNIkeEN-fs\033[0m:\033[1;36m%s\033[0m >> ", cur_path_string);
        fgets(raw_command, sizeof(raw_command), stdin);

        if (strlen(raw_command) == 1 && raw_command[0] == '\n')
            continue; // 继续下一次循环

        if (send(client_socket, raw_command, sizeof(raw_command), 0) < 0) {
            perror("Failed to send command to server");
            break;
        }

        // Receive response from server
        memset(response, 0, sizeof(response));
        ret = recv(client_socket, response, sizeof(response), 0);
        if (ret <= 0) {
            perror("Failed to receive response from server");
            break;
        }

        printf("%s", response);
        if (raw_command[0]=='e') break;
    }

    close(client_socket);

    return 0;
}
