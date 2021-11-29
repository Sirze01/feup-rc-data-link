#include "receive.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../protocol/data_link.h"
#include "packet.h"

#define DEFAULT_OUT_FILE_NAME "out_file"

static char packet[MAX_PACKET_SIZE];
static int packet_length = -1;
static char file_pathname[PATH_MAX];

// can we use atexit?

int validate_start_packet(int *file_size, int *bytes_per_packet,
                          char *packet_file_name,
                          int *packet_file_name_length) {
    if (packet[0] != CP_CONTROL_START) {
        return -1;
    }

    for (int i = 1, arg = 0; arg < 3; arg++) {
        switch (packet[i]) {
            case CP_TYPE_SIZE:
                memcpy(file_size, packet + i + 2, packet[i + 1]);
                i += (2 + packet[i + 1]);
                break;
            case CP_TYPE_FILENAME:
                memcpy(packet_file_name_length, packet + i + 1, sizeof(int));
                i += (1 + sizeof(int));
                memcpy(packet_file_name, packet + i, *packet_file_name_length);
                i += *packet_file_name_length;
                break;
            case CP_TYPE_BYTES_PER_PACKET:
                memcpy(bytes_per_packet, packet + i + 2, packet[i + 1]);
                i += (2 + packet[i + 1]);
                break;
            default:
                fprintf(stderr, "Unsuported parameter type\n");
                return -1;
        }
    }
    return 0;
}

void parse_new_pathname(char *out_file_name, char *out_file_path,
                        char *packet_file_name, int packet_file_name_lenght) {
    /* Parse new file name -
     * If option specified, then new name = name specified, else name =
     * file_name in start packet, the default file name otherwise.
     */
    if (strlen(out_file_name) == 0) {
        if (packet_file_name_lenght == 0) {
            strncpy(packet_file_name, DEFAULT_OUT_FILE_NAME,
                    strlen(DEFAULT_OUT_FILE_NAME) + 1);
        }
    } else {
        strncpy(packet_file_name, out_file_name, strlen(out_file_name) + 1);
    }
    char out_file_path_safe_length[MAX_FILE_PATH_LENGTH];
    strncpy(out_file_path_safe_length, out_file_path,
            sizeof out_file_path_safe_length - 1);
    out_file_path_safe_length[sizeof out_file_path_safe_length - 1] = '\0';

    snprintf(file_pathname, PATH_MAX, "%s/%s", out_file_path_safe_length,
             packet_file_name);
}

int validate_end_packet(int file_size, int bytes_per_packet,
                        char *packet_file_name, int packet_file_name_length) {
    if (packet[0] == CP_CONTROL_END) {
        for (int i = 1, arg = 0; arg < 3; arg++) {
            switch (packet[i]) {
                case CP_TYPE_SIZE:
                    if (memcmp(&file_size, packet + i + 2, packet[i + 1]) !=
                        0) {
                        return -1;
                    }
                    i += (2 + packet[i + 1]);
                    break;
                case CP_TYPE_FILENAME:
                    if (memcmp(&packet_file_name_length, packet + i + 1,
                               sizeof(int)) != 0) {
                        return -1;
                    }
                    i += (1 + sizeof(int));
                    if (memcmp(packet_file_name, packet + i,
                               packet_file_name_length) != 0) {
                        return -1;
                    }
                    i += packet_file_name_length;
                    break;
                case CP_TYPE_BYTES_PER_PACKET:
                    if (memcmp(&bytes_per_packet, packet + i + 2,
                               packet[i + 1]) != 0) {
                        return -1;
                    }

                    i += (2 + packet[i + 1]);
                    break;
                default:
                    return -1;
            }
        }
        return 0;
    }
    return -1;
}

int receive_file(char *out_file_dir, char *out_file_name, int port) {
    /* Open stream */
    int port_fd;
    if ((port_fd = llopen(port, RECEIVER)) == -1) {
        fprintf(stderr, "Can't open port\n");
    }

    /* Read start control packet */
    char packet_file_name[MAX_FILENAME_LENGTH];
    int packet_file_name_length = -1;
    int file_size = 0;
    int bytes_per_packet = 0;

    if ((packet_length = llread(port_fd, packet)) == -1) {
        fprintf(stderr, "Can't read start packet\n");
        if (llclose(port_fd) == -1) {
            fprintf(stderr, "Can't close port\n");
        }
        return -1;
    }

    if (validate_start_packet(&file_size, &bytes_per_packet, packet_file_name,
                              &packet_file_name_length) == -1) {
        fprintf(stderr, "Unsuported packet or parameter type\n");
        if (llclose(port_fd) == -1) {
            fprintf(stderr, "Can't close port\n");
        }
        return -1;
    }

    parse_new_pathname(out_file_name, out_file_dir, packet_file_name,
                       packet_file_name_length);

    /* (Creating&)Opening file (truncated) to write received data */
    int file_fd = -1;
    if ((file_fd = open(file_pathname, O_WRONLY | O_CREAT | O_TRUNC,
                        S_IRWXU | S_IRWXG | S_IRWXO)) == -1) {
        fprintf(stderr, "Can't create/open file in path %s\n", file_pathname);
        if (llclose(port_fd) == -1) {
            fprintf(stderr, "Can't close port\n");
        }
        return -1;
    }

    /* Read from stream and write to file */
    int file_bytes = 0;
    int bytes_read = -1;
    for (int seq_no = 0;; seq_no = (seq_no + 1) % 200) {
        if ((bytes_read = llread(port_fd, packet)) == -1) {
            fprintf(stderr, "The packet received is corrupted\n");
            continue;
        }
        if (packet[0] != DP_CONTROL) {
            break;
        }
        if (packet[0] != DP_CONTROL) {
            fprintf(stderr, "The packet received is corrupted\n");
            continue;
        }
        if (packet[1] != DP_SEQ_NO(seq_no)) {
            fprintf(
                stderr,
                "The packet doesn't match the expected packet's seq_no:%d\n",
                seq_no);
            continue;
        }

        write(file_fd, &packet[4], packet[2] * 256 + packet[3]);
        file_bytes += (packet[2] * 256 + packet[3]);
    }

    /* Validate end packet */
    if (validate_end_packet(file_size, bytes_per_packet, packet_file_name,
                            packet_file_name_length) == -1) {
        fprintf(stderr, "Unsuported packet or parameter type\n");
    }

    if (llclose(port_fd) == -1) {
        fprintf(stderr, "Can't close port\n");
        if (close(file_fd) == -1) {
            perror("Closing file");
        }
        return -1;
    }

    if (close(file_fd) == -1) {
        perror("Closing file");
        return -1;
    }

    return 0;
}