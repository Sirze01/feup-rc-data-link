#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "data_link.h"
#include "frame.h"

#define NEXT_FRAME_NUMBER(curr) (curr + 1) % 2

/* Protocol settings controllable via compiler flags */
#define DEFAULT_BAUD B38400
#define DEFAULT_TIMEOUT 5
#define DEFAULT_TRIES 60
#define DEFAULT_FER 0
#define DEFAULT_DELAY 0
#ifndef BAUDRATE
#define BAUDRATE DEFAULT_BAUD
#endif
#ifndef CONNECTION_TIMEOUT_TS
#define CONNECTION_TIMEOUT_TS DEFAULT_TIMEOUT
#endif
#ifndef CONNECTION_MAX_TRIES
#define CONNECTION_MAX_TRIES DEFAULT_TRIES
#endif
#ifndef INDUCED_FER
#define INDUCED_FER DEFAULT_FER
#endif
#ifndef INDUCED_DELAY_US
#define INDUCED_DELAY_US DEFAULT_DELAY
#endif

/* Buffers */
static struct termios oldtio;
static device_role connection_role;
static unsigned char in_frame[IF_FRAME_SIZE];
static unsigned char out_frame[IF_FRAME_SIZE];
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
        perror("Close stream");
    }
}

/**
 * @brief Blocks until supervisioned/not numbered frame is received and
 * assert content is as expected.
 *
 * @param fd serial port file descriptor
 * @param addr expected address
 * @param cmd expected command
 * @return -1 on read error, -2 on validate error, 0 otherwise
 */
static int read_validate_suf(int fd, unsigned char addr, unsigned char cmd) {
    /* Read frame */
    if (read_frame(in_frame, SUF_FRAME_SIZE, fd) == -1) {
        return -1;
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
 * @return -1 if read error, -2 if header error, -3 if bcc2 error, else read
 * data length
 */
static int read_validate_if(int fd, unsigned char addr, unsigned char cmd,
                            unsigned char *out_data_buffer) {
    /* Read frame */
    int frame_length = -1;
    if ((frame_length = read_frame(in_frame, IF_FRAME_SIZE, fd)) == -1) {
        return -1;
    }

    /* Abort or delay if induced errors are active */
    if (INDUCED_FER > 0) {
        int r = rand() % 100;
        if (r < INDUCED_FER) {
            return -1;
        }
    }
    if (INDUCED_DELAY_US > 0) {
        usleep(INDUCED_DELAY_US);
    }

    /* Validate header */
    unsigned char expected_if_header[4] = {F_FLAG, addr, cmd, addr ^ cmd};
    for (int i = 0; i < 4; i++) {
        if (in_frame[i] != expected_if_header[i]) {
            return -2;
        }
    }

    /* Destuff data and validate BCC2 */
    int unstuffed_frame_length = destuff_frame(in_frame, frame_length);
    if (unstuffed_frame_length == -1) {
        return -1;
    }
    unsigned char bcc2 = in_frame[unstuffed_frame_length - 2];
    if (byte_xor(in_frame + 4, unstuffed_frame_length - 6) != bcc2) {
        return -3;
    }

    /* Copy data to output buffer */
    memcpy(out_data_buffer, in_frame + 4, unstuffed_frame_length - 6);
    return unstuffed_frame_length - 6;
}

int llgeterrors() {
    return if_error_count;
}

int llopen(int port, device_role role) {
    /* Alert if default protocol settings were changed */
    if (BAUDRATE != DEFAULT_BAUD) {
        printf("Alert: baudrate was changed!\n");
    }
    if (CONNECTION_MAX_TRIES != DEFAULT_TRIES) {
        printf("Alert: max tries were changed to %d!\n", CONNECTION_MAX_TRIES);
    }
    if (CONNECTION_TIMEOUT_TS != DEFAULT_TIMEOUT) {
        printf("Alert: connection timeout was changed to %d ts!\n",
               CONNECTION_TIMEOUT_TS);
    }
    if (INDUCED_FER != DEFAULT_FER) {
        printf("Alert: FER is being induced on the receiver side with %d%% "
               "probability!\n",
               INDUCED_FER);
    }
    if (INDUCED_DELAY_US != DEFAULT_DELAY) {
        printf("Alert: delay is being induced on the receiver side: %d us per "
               "frame process!\n",
               INDUCED_DELAY_US);
    }

    /* Prepare random induced error generation */
    srand(time(NULL));

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
    for (int i = 0; i < CONNECTION_MAX_TRIES; i++) {
        if (connection_role == TRANSMITTER) {
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_SET);
            write(fd, out_frame, SUF_FRAME_SIZE);
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_UA) < 0) {
                continue;
            }
            return fd;
        } else {
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_SET) < 0) {
                continue;
            }
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_UA);
            write(fd, out_frame, SUF_FRAME_SIZE);
            return fd;
        }
    }

    restore_close_port(fd);
    return -1;
}

int llread(int fd, unsigned char *buffer) {
    if (connection_role == TRANSMITTER) {
        return -1;
    }

    static unsigned char curr_frame_number = 0;
    unsigned char next_frame_number = NEXT_FRAME_NUMBER(curr_frame_number);
    for (int tries = 0; tries < CONNECTION_MAX_TRIES; tries++) {
        int c = read_validate_if(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                 IF_CONTROL(curr_frame_number), buffer);
        if (c == -1) {
            continue;
        } else if (c == -2) {
            assemble_suframe(out_frame, TRANSMITTER,
                             SUF_CONTROL_RR(curr_frame_number));
            write(fd, out_frame, SUF_FRAME_SIZE);
            continue;
        } else if (c == -3) {
            if_error_count++;
            assemble_suframe(out_frame, TRANSMITTER,
                             SUF_CONTROL_REJ(curr_frame_number));
            write(fd, out_frame, SUF_FRAME_SIZE);
            continue;
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

    for (int tries = 0; tries < CONNECTION_MAX_TRIES; tries++) {
        write(fd, out_frame, frame_length);
        if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                              SUF_CONTROL_RR(next_frame_number)) < 0) {
            continue;
        }
        curr_frame_number = next_frame_number;
        return length;
    }
    return -1;
}

int llclose(int fd) {
    for (int i = 0; i < CONNECTION_MAX_TRIES; i++) {
        if (connection_role == TRANSMITTER) {
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_DISC);
            write(fd, out_frame, SUF_FRAME_SIZE);
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_DISC) < 0) {
                continue;
            }
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_UA);
            write(fd, out_frame, SUF_FRAME_SIZE);
        } else {
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_DISC) < 0) {
                continue;
            }
            assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_DISC);
            write(fd, out_frame, SUF_FRAME_SIZE);
            if (read_validate_suf(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                                  SUF_CONTROL_UA) < 0) {
                continue;
            }
        }
        restore_close_port(fd);
        return 0;
    }
    restore_close_port(fd);
    return -1;
}
