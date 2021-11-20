#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "packet.h"
#include "send.h"

static int fd = -1;
static unsigned char packet[4096];

int send_file(char *file_path, char *file_name, int port,
              int bytes_per_packet) {
    /* Retrieve file info */
    struct stat st;
    if (stat(file_path, &st) == -1) {
        perror("Retrieve file info");
        return -1;
    }

    /* Send start packet */
    int file_size = st.st_size;

    if (strcmp(file_name, "") == 0) {
        assemble_control_packet(packet, 0, 1, sizeof(int),
                                (unsigned char *)&file_size);
    } else {
        int file_name_len = strlen(file_name);
        assemble_control_packet(packet, 0, 2, sizeof(int),
                                (unsigned char *)&file_size, file_name_len,
                                (unsigned char *)file_name);
    }

    /* Open file for reading */
    fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Open file");
        return -1;
    }

    return -1;
}
