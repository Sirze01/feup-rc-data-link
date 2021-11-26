#include "receive.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../protocol/data_link.h"
#include "packet.h"

#define DEFAULT_OUT_FILE_NAME "out_file"

static char packet[4096];
static int packet_length = -1;
static char file_name[PATH_MAX];
static char file_path[PATH_MAX];

int receive_file(char *out_file_path, char *out_file_name, int port) {
    /* Open stream */
    int port_fd;
    if ((port_fd = llopen(port, RECEIVER)) == -1) {
        fprintf(stderr, "Can't open port\n");
    }

    /* Read start control packet */
    int file_size = 0;
    int file_name_length = 0;
    int bytes_per_packet = 0;

    if ((packet_length = llread(port_fd, packet)) == -1) {
        fprintf(stderr, "Can't read start packet\n");
        if (llclose(port_fd) == -1) {
            fprintf(stderr, "Can't close port\n");
        }
        return -1;
    }

    if (packet[0] == CP_CONTROL_START) {
        for (int i = 1, arg = 0; arg < 3;) {
            switch (packet[i]) {
                case CP_TYPE_SIZE:
                    memcpy(&file_size, packet + i + 2, packet[i + 1]);
                    i += (2 + packet[i + 1]);
                    arg++;
                    break;
                case CP_TYPE_FILENAME:
                    memcpy(&file_name_length, packet + i + 1, sizeof(int));
                    i += (1 + sizeof(int));
                    memcpy(file_name, packet + i, file_name_length);
                    i += file_name_length;
                    arg++;
                    break;
                case CP_TYPE_BYTES_PER_PACKET:
                    memcpy(&bytes_per_packet, packet + i + 2, packet[i + 1]);
                    i += (2 + packet[i + 1]);
                    arg++;
                    break;
                default:
                    fprintf(stderr, "Unsuported parameter type\n");
                    if (llclose(port_fd) == -1) {
                        fprintf(stderr, "Can't close port\n");
                    }
                    return -1;
            }
        }
    } else {
        fprintf(stderr, "Unexpected packet type\n");
        if (llclose(port_fd) == -1) {
            fprintf(stderr, "Can't close port\n");
        }
        return -1;
    }

    if (strlen(out_file_name) < 2) {
        if (strlen(file_name) < 2) {
            strncpy(file_name, DEFAULT_OUT_FILE_NAME,
                    strlen(DEFAULT_OUT_FILE_NAME) + 1);
        }
    } else {
        strncpy(file_name, out_file_name, strlen(out_file_name) + 1);
    }

    snprintf(file_path, PATH_MAX, "%s/%s", out_file_path, file_name);
    int fd = -1;
    if ((fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC,
                   S_IRWXU | S_IRWXG | S_IRWXO)) == -1) {
        fprintf(stderr, "Can't create file %s in path %s\n", file_name,
                out_file_path);
        if (llclose(port_fd) == -1) {
            fprintf(stderr, "Can't close port\n");
        }
        return -1;
    }

    /* Read from stream and write to file */
    int seq_no = 0;
    int bytes_read = 0;
    for (;;) {
        if ((bytes_read = llread(port_fd, packet)) == -1) {
            fprintf(stderr, "The packet received is corrupted\n");
            continue;
        }
        if (packet[0] == CP_CONTROL_END) {
            break;
        }

        write(fd, &packet[4], packet[2] * 256 + packet[3]);
        seq_no++;
    }
    //  write...

    /* Read close control packet */
    // read...

    if (llclose(port_fd) == -1) {
        fprintf(stderr, "Can't close port\n");
        if (close(fd) == -1) {
            perror("Closing file");
        }
        return -1;
    }

    if (close(fd) == -1) {
        perror("Closing file");
        return -1;
    }

    return 0;
}