#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "data_link.h"
#include "frame.h"

/* Useful for delaying retries */
#define sleep_continue                                                         \
    usleep(5000);                                                              \
    continue
#define sleep_continue_long                                                    \
    sleep(1);                                                                  \
    continue

/* Protocol settings */
#define BAUDRATE B38400
#define CONNECTION_TIMEOUT_TS 3
#define CONNECTION_MAX_RETRIES 5
#define NEXT_FRAME_NUMBER(curr) (curr + 1) % 2

/* Buffers */
static struct termios oldtio;
static device_role connection_role;
static unsigned char in_frame[IF_FRAME_SIZE];
static unsigned char out_frame[IF_FRAME_SIZE];

/* Statistics */
static unsigned if_error_count = 0;

/**
 * @brief Restore previously changed serial port configuration and close file
 * descriptor.
 *
 * @param fd serial port file descriptor
 */
static void restore_close_port(int fd) {
    if (tcsetattr(fd, TCSADRAIN, &oldtio) == -1) {
        perror("Set termios attributes");
    }
    if (close(fd) == -1) {
        perror("Close stream fd");
    }
}

/**
 * @brief Blocks until supervisioned/not numbered frame is received and assert
 * content is as expected.
 *
 * @param fd serial port file descriptor
 * @param addr expected address
 * @param cmd expected command
 * @return -1 on success, 0 otherwise
 */
static int read_validate_suf(int fd, unsigned char addr, unsigned char cmd) {
    /* Read frame */
    for (int i = 0;; i++) {
        if (i == CONNECTION_MAX_RETRIES) {
            return -1;
        }
        if (read_frame(in_frame, SUF_FRAME_SIZE, fd) == -1) {
            sleep_continue;
        } else {
            break;
        }
    }

    /* Validate frame */
    unsigned char expected_suf[SUF_FRAME_SIZE] = {F_FLAG, addr, cmd, addr ^ cmd,
                                                  F_FLAG};
    for (int i = 0; i < SUF_FRAME_SIZE; i++) {
        if (in_frame[i] != expected_suf[i]) {
            return -2;
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
 * @return -1 if read error, -2 if bcc check error, else read data length
 */
static int read_validate_if(int fd, unsigned char addr, unsigned char cmd,
                            unsigned char *out_data_buffer) {
    /* Read frame */
    int frame_length = -1;
    for (int i = 0;; i++) {
        if (i == CONNECTION_MAX_RETRIES) {
            return -1;
        }
        if ((frame_length = read_frame(in_frame, IF_FRAME_SIZE, fd)) == -1) {
            sleep_continue;
        } else {
            break;
        }
    }

    /* Validate header */
    unsigned char expected_if_header[4] = {F_FLAG, addr, cmd, addr ^ cmd};
    for (int i = 0; i < 4; i++) {
        if (in_frame[i] != expected_if_header[i]) {
            if_error_count++;
            return -1;
        }
    }

    /* Destuff data and validate BCC2 */
    int unstuffed_frame_length = destuff_bytes(in_frame, frame_length);

    if (unstuffed_frame_length == -1) {
        return -1;
    }
    unsigned char bcc2 = in_frame[unstuffed_frame_length - 2];
    if (byte_xor(in_frame + 4, unstuffed_frame_length - 6) != bcc2) {
        if_error_count++;
        return -2;
    }

    /* Copy data to output buffer */
    memcpy(out_data_buffer, in_frame + 4, unstuffed_frame_length - 6);
    return unstuffed_frame_length - 6;
}

int llgeterrors() {
    return if_error_count;
}

int llopen(int port, device_role role) {
    /* Assemble device file path */
    char port_path[PATH_MAX];
    int c = snprintf(port_path, PATH_MAX, "/dev/ttyS%d", port);
    if (c < 0 || c > PATH_MAX) {
        return -1;
    }

    /* Open port */
    int fd = open(port_path, O_RDWR | O_NOCTTY);
    if (fd == -1) {
        perror("Open stream");
        return -1;
    }

    /* Save current port configuration for later restoration */
    if (tcgetattr(fd, &oldtio) == -1) {
        perror("Get termios attributes");
        return -1;
    }

    /* Set new port configuration */
    struct termios newtio = oldtio;
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;                         /* Non canonical */
    newtio.c_cc[VTIME] = CONNECTION_TIMEOUT_TS; /* Timeout for reads */
    newtio.c_cc[VMIN] = 0; /* Do not block waiting for characters */
    if ((tcflush(fd, TCIOFLUSH) == -1) |
        (tcsetattr(fd, TCSANOW, &newtio) == -1)) {
        perror("Set termios attributes");
        restore_close_port(fd);
        return -1;
    }

    /* Setup connection */
    connection_role = role;
    for (int i = 0;; i++) {
        if (i == CONNECTION_MAX_RETRIES) {
            return -1;
        }
        if (connection_role == TRANSMITTER) {
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_SET);
            if (write(fd, out_frame, SUF_FRAME_SIZE) == -1) {
                perror("Write suframe");
            }
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_UA) == -1) {
                sleep_continue_long;
            }
            return fd;
        } else {
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_SET) == -1) {
                sleep_continue_long;
            }
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_UA);
            if (write(fd, out_frame, SUF_FRAME_SIZE) == -1) {
                perror("Write suframe");
            }
            return fd;
        }
    }

    restore_close_port(fd);
    return -1;
}

int llclose(int fd) {
    for (int i = 0;; i++) {
        if (i == CONNECTION_MAX_RETRIES) {
            return -1;
        }
        if (connection_role == TRANSMITTER) {
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_DISC);
            if (write(fd, out_frame, SUF_FRAME_SIZE) == -1) {
                perror("Write suframe");
            }
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_DISC) == -1) {
                sleep_continue_long;
            }
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_UA);
            if (write(fd, out_frame, SUF_FRAME_SIZE) == -1) {
                perror("Write suframe");
            }
        } else {
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_DISC) == -1) {
                sleep_continue_long;
            }
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_DISC);
            if (write(fd, out_frame, SUF_FRAME_SIZE) == -1) {
                perror("Write suframe");
            }
            while (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                     SUF_CONTROL_UA) == -1) {
                sleep_continue;
            }
        }
        restore_close_port(fd);
        return 0;
    }
    restore_close_port(fd);
    return -1;
}

int llwrite(int fd, unsigned char *buffer, int length) {
    if (connection_role == RECEIVER) {
        return -1;
    }
    if (length > IF_MAX_DATA_SIZE) {
        return -1;
    }

    static unsigned char curr_frame_number = 0;
    unsigned char next_frame_number = NEXT_FRAME_NUMBER(curr_frame_number);
    int frame_length = assemble_iframe(
        out_frame, TRANSMITTER, IF_CONTROL(curr_frame_number), buffer, length);

    for (int tries = 0; tries < CONNECTION_MAX_RETRIES; tries++) {
        if (write(fd, out_frame, frame_length) == -1) {
            perror("Write iframe");
        }
        if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                              SUF_CONTROL_RR(next_frame_number)) != 0) {
            sleep_continue;
        }
        curr_frame_number = next_frame_number;
        return length;
    }
    return -1;
}

int llread(int fd, unsigned char *buffer) {
    if (connection_role == TRANSMITTER) {
        return -1;
    }

    static unsigned char curr_frame_number = 0;
    unsigned char next_frame_number = NEXT_FRAME_NUMBER(curr_frame_number);
    for (int tries = 0; tries < CONNECTION_MAX_RETRIES; tries++) {
        int c = read_validate_if(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                 IF_CONTROL(curr_frame_number), buffer);
        if (c == -1) {
            sleep_continue;
        } else if (c == -2) {
            assemble_suframe(out_frame, TRANSMITTER,
                             SUF_CONTROL_REJ(curr_frame_number));
            if (write(fd, out_frame, SUF_FRAME_SIZE) == -1) {
                perror("Write suframe");
            }
            sleep_continue;
        } else {
            assemble_suframe(out_frame, TRANSMITTER,
                             SUF_CONTROL_RR(next_frame_number));
            if (write(fd, out_frame, SUF_FRAME_SIZE) == -1) {
                perror("Write suframe");
            }
            curr_frame_number = next_frame_number;
            return c;
        }
    }
    return -1;
}
