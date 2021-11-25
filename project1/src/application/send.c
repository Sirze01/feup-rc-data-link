#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// #include "../protocol/data_link.h"
#include "packet.h"
#include "send.h"

static int fd = -1;
static char packet[4096];

int send_file(char *file_path, char *file_name, int port,
              int bytes_per_packet) {
    /* Retrieve file info */
    struct stat st;
    if (stat(file_path, &st) == -1) {
        perror("Retrieve file info");
        return -1;
    }

    /* Open stream */
    // llopen();

    /* Send start packet */
    if (strcmp(file_name, "") == 0) {
        assemble_control_packet(packet, 0, 1, sizeof(int), (char *)&st.st_size);
    } else {
        int file_name_len = strlen(file_name);
        assemble_control_packet(packet, 0, 2, sizeof(int), (char *)&st.st_size,
                                file_name_len, (char *)file_name);
    }
    // llwrite();

    /* Open file for reading */
    fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Open file");
        return -1;
    }

    /* Send file to stream at given rate */
    char data[4096];
    unsigned seq_no = 0;
    for (int i = 0; i < st.st_size; i += bytes_per_packet) {
        if (read(fd, data, bytes_per_packet) != bytes_per_packet) {
            break;
        }
        assemble_data_packet(packet, seq_no++, data, bytes_per_packet);
        // llwrite();
    }

    /* Send end packet */
    if (strcmp(file_name, "") == 0) {
        assemble_control_packet(packet, 1, 1, sizeof(int), (char *)&st.st_size);
    } else {
        int file_name_len = strlen(file_name);
        assemble_control_packet(packet, 1, 2, sizeof(int), (char *)&st.st_size,
                                file_name_len, (char *)file_name);
    }
    // llwrite();

    /* Close file */
    if (close(fd) == -1) {
        perror("Close file");
        return -1;
    }

    /* Close stream */
    // llclose();

    return -1;
}
