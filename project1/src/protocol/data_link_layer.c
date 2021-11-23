#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "data_link_layer.h"

#define DEFAULT_DELAY_US 20000
#define BAUDRATE B38400
#define CONNECTION_TIMEOUT_DS 10
#define CONNECTION_MAX_RETRIES 3
#define NEXT_FRAME_NUMBER(curr) (curr + 1) % 2

static struct termios oldtio;
device_role connection_role;
char in_frame[IF_FRAME_SIZE];
char out_frame[IF_FRAME_SIZE];
int curr_fr_no = 0;

static int restore_port_settings(int fd) {
    if (tcdrain(fd) == -1) {
        return -1;
    }
    if (tcflush(fd, TCIOFLUSH) == -1) {
        return -1;
    }
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
        return -1;
    }
    if (close(fd) == -1)
        return -1;

    return 0;
}

static int read_validate_suf(int fd, char addr, char cmd) {
    char expected_suf[5] = {F_FLAG, addr, cmd, addr ^ cmd, F_FLAG};
    bzero(in_frame, SUF_FRAME_SIZE);
    for (int i = 0; i < 5; i++) {
        if (read(fd, in_frame + i, 1) < 0) {
            return -1;
        }
        if (in_frame[i] != expected_suf[i]) {
            printf("in_frame[%d]: %x, expected: %x\n", i, in_frame[i],
                   expected_suf[i]);
            return -1;
        }
    }
    return 0;
}

static int read_validate_if(int fd, char addr, char cmd, char *data_buffer) {
    char expected_if_header[4] = {F_FLAG, addr, cmd, addr ^ cmd};
    bzero(in_frame, SUF_FRAME_SIZE);
    int frame_length = 0;
    for (int i = 0; i < IF_FRAME_SIZE; i++) {
        if (read(fd, in_frame + i, 1) < 0) {
            return -1;
        }
        if (i <= 3) {
            if (in_frame[i] != expected_if_header[i]) {
                return -1;
            }
        } else if (in_frame[i] == F_FLAG) {
            frame_length = i + 1;
            break;
        }
    }
    if ((frame_length = destuff_bytes(in_frame, frame_length)) == -1) {
        return -1;
    }
    if (byte_xor(in_frame + 4, frame_length - 6) !=
        in_frame[frame_length - 2]) {
        printf("o xor!");
        return -1;
    }
    memcpy(data_buffer, in_frame + 4, frame_length - 6);
    return frame_length;
}

int llopen(int port, device_role role) {
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
        return -1;
    }

    /* Assemble new port configuration */
    struct termios newtio;
    bzero(&newtio, sizeof newtio);
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = CONNECTION_TIMEOUT_DS;
    newtio.c_cc[VMIN] = 0;

    /* Set new port configuration */
    if (tcflush(fd, TCIOFLUSH) == -1) {
        return -1;
    }
    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        return -1;
    }

    /* Setup connection */
    connection_role = role;
    for (int tries = 0; tries < CONNECTION_MAX_RETRIES; tries++) {
        if (connection_role == TRANSMITTER) {
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_SET);
            if (write(fd, out_frame, SUF_FRAME_SIZE) == -1) {
                perror("Write suframe");
                continue;
            }
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_UA) == -1) {
                continue;
            }
            return fd;
        } else {
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_SET) == -1) {
                continue;
            }
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_UA);
            if (write(fd, out_frame, SUF_FRAME_SIZE) == -1) {
                perror("Write suframe");
                continue;
            }
            return fd;
        }
    }

    restore_port_settings(fd);
    return -1;
}

int llclose(int fd) {
    for (int tries = 0; tries < CONNECTION_MAX_RETRIES; tries++) {
        if (connection_role == TRANSMITTER) {
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_DISC);
            if (write(fd, out_frame, 5) == -1) {
                perror("Write suframe");
                continue;
            }
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_DISC) == -1) {
                continue;
            }
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_UA);
            if (write(fd, out_frame, 5) == -1) {
                perror("Write suframe");
                continue;
            }
            return fd;
        } else {
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_DISC) == -1) {
                continue;
            }
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_DISC);
            if (write(fd, out_frame, 5) == -1) {
                perror("Write suframe");
                continue;
            }
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_UA) == -1) {
                continue;
            }
            return fd;
        }
    }

    fprintf(stderr, "Connection was improperly closed, other end may hang\n");
    restore_port_settings(fd);
    return -1;
}

int llwrite(int fd, char *buffer, int length) {
    if (connection_role == RECEIVER) {
        fprintf(stderr, "Cannot read while opened as a receiver\n");
        return -1;
    }

    if (length > IF_MAX_DATA_SIZE) {
        fprintf(stderr,
                "Cannot send packet with length greater than %d bytes\n",
                IF_MAX_DATA_SIZE);
        return -1;
    }

    int frame_length = assemble_iframe(out_frame, TRANSMITTER,
                                       IF_CONTROL(curr_fr_no), buffer, length);
    int next_fr_no = NEXT_FRAME_NUMBER(curr_fr_no);
    int bytes_written = 0;

    for (int tries = 0; tries < CONNECTION_MAX_RETRIES; tries++) {
        if ((bytes_written = write(fd, out_frame, frame_length)) == -1) {
            usleep(DEFAULT_DELAY_US);
            continue;
        }

        if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                              SUF_CONTROL_RR(next_fr_no)) == -1) {
            usleep(DEFAULT_DELAY_US);
            continue;
        }

        curr_fr_no = next_fr_no;
        break;
    }

    return curr_fr_no == next_fr_no ? (bytes_written - 6) : -1;
}

int llread(int fd, char *buffer) {
    if (connection_role == TRANSMITTER) {
        fprintf(stderr, "Cannot read while opened as a transmitter\n");
        return -1;
    }

    int bytes_read = -1;
    int next_fr_no = NEXT_FRAME_NUMBER(curr_fr_no);

    assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_RR(next_fr_no));
    printf("%x %x %x %x %x\n", out_frame[0], out_frame[1], out_frame[2],
           out_frame[3], out_frame[4]);
    for (int tries = 0; tries < CONNECTION_MAX_RETRIES; tries++) {
        if ((bytes_read = read_validate_if(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                           IF_CONTROL(curr_fr_no), buffer)) ==
            -1) {
            usleep(DEFAULT_DELAY_US);
            continue;
        }

        curr_fr_no = next_fr_no;
        if (write(fd, out_frame, 5) == -1) {
            perror("Write suframe");
            return -1;
        }
        break;
    }

    return curr_fr_no == next_fr_no ? (bytes_read - 6) : -1;
}
