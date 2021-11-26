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

/* Protocol settings */
#define BAUDRATE B38400
#define CONNECTION_TIMEOUT_TS 5
#define CONNECTION_MAX_RETRIES 5
#define NEXT_FRAME_NUMBER(curr) (curr + 1) % 2

/* Buffers */
static struct termios oldtio;
static device_role connection_role;
static char in_frame[IF_FRAME_SIZE];
static char out_frame[IF_FRAME_SIZE];

/**
 * @brief Restore previously changed serial port configuration and close file
 * descriptor.
 *
 * @param fd serial port file descriptor
 */
static void restore_close_port(int fd) {
    tcsetattr(fd, TCSADRAIN, &oldtio);
    close(fd);
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
static int read_validate_suf(int fd, char addr, char cmd) {
    /*     printf("trying to read suf\n"); */
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
    char expected_suf[SUF_FRAME_SIZE] = {F_FLAG, addr, cmd, addr ^ cmd, F_FLAG};
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
 * @return read data length
 */
static int read_validate_if(int fd, char addr, char cmd,
                            char *out_data_buffer) {
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
    char expected_if_header[4] = {F_FLAG, addr, cmd, addr ^ cmd};
    for (int i = 0; i < 4; i++) {
        if (in_frame[i] != expected_if_header[i]) {
            return -1;
        }
    }

    /* Destuff data and validate BCC2 */
    char bcc2 = in_frame[frame_length - 2];
    int unstuffed_data_length = destuff_bytes(in_frame + 4, frame_length - 6);
    if (unstuffed_data_length == -1) {
        return -1;
    }
    if (byte_xor(in_frame + 4, unstuffed_data_length) != bcc2) {
        return -2;
    }

    /* Copy data to output buffer */
    memcpy(out_data_buffer, in_frame + 4, unstuffed_data_length);
    return unstuffed_data_length;
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
    int fd = open(port_path, O_RDWR | O_NOCTTY); /* Do not control terminal */
    if (fd == -1) {
        perror("Error opening port");
        return -1;
    }

    /* Save current port configuration for later restoration */
    if (tcgetattr(fd, &oldtio) == -1) {
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
        restore_close_port(fd);
        return -1;
    }

    /* Setup connection */
    connection_role = role;
    for (;;) {
        if (connection_role == TRANSMITTER) {
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_SET);
            if (write(fd, out_frame, SUF_FRAME_SIZE) == -1) {
                perror("Write suframe");
                sleep_continue;
            }
            printf("transmitter sent set\n");
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_UA) == -1) {
                sleep_continue;
            }
            printf("transmitter read ua\n");
            return fd;
        } else {
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_SET) == -1) {
                sleep_continue;
            }
            printf("receiver read set\n");
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_UA);
            if (write(fd, out_frame, SUF_FRAME_SIZE) == -1) {
                perror("Write suframe");
                sleep_continue;
            }
            printf("receiver sent ua\n");
            return fd;
        }
    }

    restore_close_port(fd);
    return -1;
}

int llclose(int fd) {
    for (;;) {
        if (connection_role == TRANSMITTER) {
            /*             printf("vou dar assemble ao disc\n"); */
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_DISC);
            write(fd, out_frame, SUF_FRAME_SIZE);
            /*             printf("escrevi disconnect\n"); */
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_DISC) == -1) {
                /*                 printf("nao consegui ler um disconnect\n");
                 */
                sleep_continue;
            }
            printf("li disconnect\n");
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_UA);
            write(fd, out_frame, SUF_FRAME_SIZE);
            printf("mandei ua\n");
        } else {
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_DISC) == -1) {
                /*                 printf("nao consegui ler um disconnect e vou
                 * tentar de novo\n"); */
                sleep_continue;
            }
            printf("li disconnect\n");
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_DISC);
            write(fd, out_frame, SUF_FRAME_SIZE);
            printf("mandei disconnect\n");
            while (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                     SUF_CONTROL_UA) == -1) {
                /*                 printf("nao consegui ler um ua e vou tentar
                 * de novo\n"); */
                sleep_continue;
            }
        }
        restore_close_port(fd);
        return 0;
    }
    fprintf(stderr, "Connection was improperly closed, other end may hang\n");
    restore_close_port(fd);
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

    static char curr_frame_number = 0;
    int next_frame_number = NEXT_FRAME_NUMBER(curr_frame_number);
    int frame_length = assemble_iframe(
        out_frame, TRANSMITTER, IF_CONTROL(curr_frame_number), buffer, length);
    for (int tries = 0; tries < CONNECTION_MAX_RETRIES; tries++) {
        write(fd, out_frame, frame_length);
        if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                              SUF_CONTROL_RR(next_frame_number)) != 0) {
            printf("queria ler um rr e nao li. vou tentar de novo\n");
            sleep_continue;
        }
        curr_frame_number = next_frame_number;
        return frame_length - 6;
    }
    return -1;
}

int llread(int fd, char *buffer) {
    if (connection_role == TRANSMITTER) {
        fprintf(stderr, "Cannot read while opened as a transmitter\n");
        return -1;
    }

    static char curr_frame_number = 0;
    int next_frame_number = NEXT_FRAME_NUMBER(curr_frame_number);
    for (int tries = 0; tries < CONNECTION_MAX_RETRIES; tries++) {
        int c = read_validate_if(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                 IF_CONTROL(curr_frame_number), buffer);
        if (c == -1) {
            sleep_continue;
        } else if (c == -2) {
            assemble_suframe(out_frame, TRANSMITTER,
                             SUF_CONTROL_REJ(curr_frame_number));
            write(fd, out_frame, SUF_FRAME_SIZE);
            sleep_continue;
        } else {
            assemble_suframe(out_frame, TRANSMITTER,
                             SUF_CONTROL_RR(next_frame_number));
            write(fd, out_frame, SUF_FRAME_SIZE);
            curr_frame_number = next_frame_number;
            return c;
        }
    }
    return -1;
}
