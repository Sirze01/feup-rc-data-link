#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "data_link_layer.h"

/* Useful for delaying retries */
#define sleep_continue                                                         \
    usleep(20000);                                                             \
    continue

/* Protocol settings */
#define BAUDRATE B38400
#define CONNECTION_TIMEOUT_TS 30
#define CONNECTION_MAX_RETRIES 3
#define NEXT_FRAME_NUMBER(curr) (curr + 1) % 2

/* Buffers */
static struct termios oldtio;
static device_role connection_role;
static char in_frame[IF_FRAME_SIZE];
static char out_frame[IF_FRAME_SIZE];
static char curr_frame_number = 0;

/**
 * @brief Restore previously changed serial port configuration and close file
 * descriptor.
 *
 * @param fd serial port file descriptor
 * @return -1 on error, 0 otherwise
 */
static int restore_port_settings(int fd) {
    if (tcflush(fd, TCIOFLUSH) == -1) {
        return -1;
    }
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
        return -1;
    }
    return 0;
}

/**
 * @brief Read supervisioned/not numbered frame and assert content is as
 * expected.
 *
 * @param fd serial port file descriptor
 * @param addr expected address
 * @param cmd expected command
 * @return -1 on success, 0 otherwise
 */
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

/**
 * @brief Read information frame and assert control bytes are as expected.
 *
 * @param fd serial port file descriptor
 * @param addr expected address
 * @param cmd expected command
 * @param out_data_buffer buffer to write data to (no headers)
 * @return read frame length
 */
static int read_validate_if(int fd, char addr, char cmd,
                            char *out_data_buffer) {
    char expected_if_header[4] = {F_FLAG, addr, cmd, addr ^ cmd};
    bzero(in_frame, 4);
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
        return -1;
    }
    memcpy(out_data_buffer, in_frame + 4, frame_length - 6);
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
    int fd =
        open(port_path, O_RDWR | O_NOCTTY); /* Open in non canonical mode */
    if (fd == -1) {
        perror("Error opening port");
        return -1;
    }

    /* Save current port configuration for later restoration */
    if (tcgetattr(fd, &oldtio) == -1) {
        return -1;
    }

    /* Set new port configuration */
    for (;;) {
        struct termios newtio = oldtio;
        newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
        newtio.c_iflag = IGNPAR;
        newtio.c_oflag = 0;
        newtio.c_lflag = 0;                         /* Non canonical */
        newtio.c_cc[VTIME] = CONNECTION_TIMEOUT_TS; /* Timeout for reads */
        newtio.c_cc[VMIN] = 0; /* Do not block waiting for characters */
        if (tcflush(fd, TCIOFLUSH) == -1) {
            return -1;
        }
        if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
            return -1;
        }
        struct termios c;
        tcgetattr(fd, &c);
        printf("a0: %d\n", c.c_cflag);
        printf("a1: %d\n", c.c_iflag);
        printf("a2: %d\n", c.c_oflag);
        printf("a3: %d\n", c.c_lflag);
        printf("a4: %d\n", c.c_cc[VTIME]);
        printf("a5: %d\n", c.c_cc[VMIN]);
        break;
    }

    /* Setup connection */
    connection_role = role;
    for (int tries = 0; tries < CONNECTION_MAX_RETRIES; tries++) {
        if (connection_role == TRANSMITTER) {
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_SET);
            if (write(fd, out_frame, SUF_FRAME_SIZE) == -1) {
                perror("Write suframe");
                sleep_continue;
            }
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_UA) == -1) {
                sleep_continue;
            }
            return fd;
        } else {
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_SET) == -1) {
                sleep_continue;
            }
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_UA);
            if (write(fd, out_frame, SUF_FRAME_SIZE) == -1) {
                perror("Write suframe");
                sleep_continue;
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
                sleep_continue;
            }
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_DISC) == -1) {
                sleep_continue;
            }
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_UA);
            if (write(fd, out_frame, 5) == -1) {
                perror("Write suframe");
                sleep_continue;
            }
        } else {
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_DISC) == -1) {
                sleep_continue;
            }
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_DISC);
            if (write(fd, out_frame, 5) == -1) {
                perror("Write suframe");
                sleep_continue;
            }
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_UA) == -1) {
                sleep_continue;
            }
        }
        restore_port_settings(fd);
        close(fd);
        return 0;
    }
    fprintf(stderr, "Connection was improperly closed, other end may hang\n");
    restore_port_settings(fd);
    close(fd);
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

    int frame_length = assemble_iframe(
        out_frame, TRANSMITTER, IF_CONTROL(curr_frame_number), buffer, length);
    int next_frame_number = NEXT_FRAME_NUMBER(curr_frame_number);
    for (int tries = 0; tries < CONNECTION_MAX_RETRIES; tries++) {
        int written_bytes = -1;
        if ((written_bytes = write(fd, out_frame, frame_length)) == -1) {
            sleep_continue;
        }
        if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                              SUF_CONTROL_RR(next_frame_number)) == -1) {
            sleep_continue;
        }
        curr_frame_number = next_frame_number;
        return written_bytes - 6;
    }
    return -1;
}

int llread(int fd, char *buffer) {
    if (connection_role == TRANSMITTER) {
        fprintf(stderr, "Cannot read while opened as a transmitter\n");
        return -1;
    }

    int next_frame_number = NEXT_FRAME_NUMBER(curr_frame_number);
    assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_RR(next_frame_number));
    for (int tries = 0; tries < CONNECTION_MAX_RETRIES; tries++) {
        int read_bytes = -1;
        if ((read_bytes = read_validate_if(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                           IF_CONTROL(curr_frame_number),
                                           buffer)) == -1) {
            sleep_continue;
        }
        if (write(fd, out_frame, SUF_FRAME_SIZE) == -1) {
            sleep_continue;
        }
        curr_frame_number = next_frame_number;
        return read_bytes - 6;
    }
    return -1;
}
