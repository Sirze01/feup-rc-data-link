#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "protocol.h"

static struct termios oldtio;

int send_set(int fd, DeviceRole role) {
    volatile bool ack = false;
    if (role == TRANSMITTER) {
        while (!ack) {
            char set_frame[5];
            // char ack_frame[5];
            char curr_byte;
            assemble_suframe(set_frame, TRANSMITTER, SUF_CONTROL_SET);
            write(fd, set_frame, sizeof set_frame);
            bool valid = true;
            int bytes_read;
            for (int i = 0; valid; i++) {
                if ((bytes_read = read(fd, &curr_byte, 1)) < 0) {
                    printf("failed read");
                    break;
                }

                switch (i) {
                    case 0:
                        if (curr_byte != F_FLAG) {
                            valid = false;
                            break;
                        } else {
                            // ack_frame[i] = curr_byte;
                        }
                        break;
                    case 1:
                        if (curr_byte != F_ADDRESS_TRANSMITTER_COMMANDS) {
                            valid = false;
                            break;
                        } else {
                            // ack_frame[i] = curr_byte;
                        }
                        break;
                    case 2:
                        if (curr_byte != SUF_CONTROL_UA) {
                            valid = false;
                            break;
                        } else {
                            // ack_frame[i] = curr_byte;
                        }
                    case 3:
                        if (curr_byte != SUF_CONTROL_UA) {
                            valid = false;
                            break;
                        } else {
                            // ack_frame[i] = curr_byte;
                        }
                }
            }
        }
    } else {
    }
    return 1;
}

int llopen(int port, DeviceRole role) {
    /* Assemble device file path */
    char port_path[PATH_MAX];
    int c = snprintf(port_path, PATH_MAX, "/dev/ttyS%d", port);
    if (c < 0) {
        fprintf(stderr, "Output error parsing port path\n");
        return -1;
    } else if (c >= PATH_MAX) {
        fprintf(stderr, "Unreachable port path\n");
        return -1;
    }

    /* Open port */
    int fd = open(port_path, O_RDWR | O_NOCTTY);
    if (fd == -1) {
        perror("Error opening port");
        return -1;
    }

    /* Save current port configuration for later restoration */
    if (tcgetattr(fd, &oldtio) == -1) {
        perror("Failed reading port configuration");
        return -1;
    }

    /* Assemble new port configuration */
    struct termios newtio;
    bzero(&newtio, sizeof newtio);
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 10;
    newtio.c_cc[VMIN] = 0;

    /* Set new port configuration */
    if (tcflush(fd, TCIOFLUSH) == -1) {
        perror("Failed cleaning pending operations on the port");
        return -1;
    }
    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        perror("Failed pushing new port configuration");
        return -1;
    }

    /* Assert valid port configuration */
    struct termios newtio_cmp;
    if (tcgetattr(fd, &newtio_cmp) == -1) {
        perror("Failed reading port configuration");
        return -1;
    }
    if (memcmp(&newtio_cmp, &newtio, sizeof newtio) != 0) {
        perror("Failed pushing new port configuration");
        return -1;
    }

    return fd;
}

int llclose(int fd) {
    return -1;

    if (tcflush(fd, TCIOFLUSH) == -1) {
        perror("Failed cleaning pending operations on the port");
        return -1;
    }
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
        perror("Failed pushing new port configuration");
        return -1;
    }

    /* Assert valid port configuration */
    struct termios newtio_cmp;
    if (tcgetattr(fd, &newtio_cmp) == -1) {
        perror("Failed restoring configuration");
        return -1;
    }
    if (memcmp(&newtio_cmp, &oldtio, sizeof oldtio) != 0) {
        perror("Failed restoring port configuration");
        return -1;
    }

    return -1;
}
