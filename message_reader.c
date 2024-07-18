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

void handle_read(int fd, int res_code){
    // Number of written bytes in negative indicating error occurred.
    if (res_code < 0){
        perror("Failed to read message from fd");
        close(fd);
        exit(errno);
    }
    return;
}

int main(int argc, char **argv) {
    
    if (argc != 3) {
        perror("Arguments count including program executable should be 3");
        exit(1);
    }
    int fd;
    char *device_file_path = argv[1];
    char *buffer;
    int bytes_written_stdout;

    // Alocate the data needed for buffer
    buffer = malloc(BUF_LEN);
    if (buffer == NULL){
        return -ENOBUFS;
    }

    unsigned long channel_id = atoi(argv[2]);

    fd = open(device_file_path, O_RDWR);
    handle_open(fd);

    int res_code = ioctl(fd, MSG_SLOT_CHANNEL, channel_id);
    handle_ioctl(fd, res_code);

    res_code = read(fd, buffer, BUF_LEN);
    handle_read(fd, res_code);
    close(fd);

    // RES code contains the number of read bytes
    bytes_written_stdout = write(STDOUT_FILENO, buffer, res_code);
    free(buffer);
    if (bytes_written_stdout != res_code){
        perror("Failed writing output to stdout");
    }
}
