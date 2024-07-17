#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "message_slot.h"


void handle_open(int fd){
    if (fd < 0){
        perror("Invalid fd provided");
        close(fd);
        exit(1);
    }
    return;
}

void handle_ioctl(int fd, int res_code){
    if (res_code != 0){
        perror("Failed to call ioctl api, recived res code");
        printf("Closing file descriptor");
        close(fd);
        exit(errno);
    }
    return;
}

void handle_write(int fd, int res_code){
    // Number of written bytes in negative indicating error occurred.
    if (res_code < 0){
        perror("Failed to write message to fd");
        close(fd);
        exit(errno);
    }
    return;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        perror("Arguments count including program executable should be 4");
        exit(1);
    }
    
    char *device_file_path = argv[1];
    int fd;
    char *endptr;
    unsigned long channel_id = atoi(argv[2]);

    char *message = argv[3];

    fd = open(device_file_path, O_RDWR);
    handle_open(fd);

    int res_code = ioctl(fd, MSG_SLOT_CHANNEL, channel_id);
    handle_ioctl(fd, res_code);

    res_code = write(fd, message, strlen(message));
    handle_write(fd, res_code);

    printf("Closing fd: %d", fd);
    close(fd);
    printf("Closed fd: %d", fd);
}
