#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "data_link_layer.h"

static struct termios oldtio;

device_role connection_role;

char data_frame[IF_FRAME_SIZE];
char aux_frame[IF_FRAME_SIZE]; // Ostritch

int curr_fr_no = 0;

static int port_cleanup(int fd) {
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

static int read_SUF(int fd, char addr, char cmd) {
    char curr_byte;
    bool valid = true;
    bool complete = false;
    char in_frame[3];

    for (int i = 0; valid && i <= 4; i++) {
        if (read(fd, &curr_byte, 1) < 0) {
            break;
        }

        switch (i) {
            case 0:
                if (curr_byte != F_FLAG) {
                    valid = false;
                }
                break;
            case 1:
                if (curr_byte != addr) {
                    valid = false;
                } else {
                    in_frame[i - 1] = curr_byte;
                }
                break;
            case 2:
                if (curr_byte != cmd) {
                    valid = false;
                } else {
                    in_frame[i - 1] = curr_byte;
                }
                break;
            case 3:
                in_frame[i - 1] = curr_byte;
                if (byte_xor(in_frame, sizeof in_frame) != 0) {
                    valid = false;
                }
                break;
            case 4:
                if (curr_byte != F_FLAG) {
                    valid = false;
                } else {
                    complete = true;
                }
                break;
        }
    }

    if (!complete) {
        return -1;
    }

    return 0;
}

static int read_IF(int fd, char addr, char cmd, char *buffer) {
    char curr_byte;
    bool valid = true;
    bool complete = false;
    int pkt_length = 0;

    for (int i = 0; valid && !complete; i++) {
        if (read(fd, &curr_byte, 1) < 0) {
            break;
        }

        switch (i) {
            case 0:
                if (curr_byte != F_FLAG) {
                    valid = false;
                } else {
                    data_frame[i] = curr_byte;
                }
                break;
            case 1:
                if (curr_byte != addr) {
                    valid = false;
                } else {
                    data_frame[i] = curr_byte;
                }
                break;
            case 2:
                if (curr_byte != cmd) {
                    valid = false;
                } else {
                    data_frame[i] = curr_byte;
                }
                break;
            case 3:
                data_frame[i] = curr_byte;
                if (byte_xor(data_frame + 1, 3) != 0) {
                    valid = false;
                }
                break;
            default:
                if (curr_byte == F_FLAG) {
                    complete = true;
                    pkt_length = i + 1;
                }
                data_frame[i] = curr_byte;
                break;
        }
    }

    if (!complete) {
        return -1;
    }

    if ((pkt_length = destuff_bytes(data_frame, aux_frame, pkt_length)) == -1) {
        return -1;
    }

    if (byte_xor(data_frame + 4, pkt_length - 5) != 0) {
        return -1;
    }

    memcpy(buffer, data_frame + 4, pkt_length - 6);
    return pkt_length - 6;
}

static int connection_setup(int fd, device_role role) {
    connection_role = role;
    char out_frame[5];

    if (connection_role == TRANSMITTER) {

        assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_SET);
        write(fd, out_frame, sizeof out_frame);

        if (read_SUF(fd, F_ADDRESS_TRANSMITTER_COMMANDS, SUF_CONTROL_UA) ==
            -1) {
            return -1;
        }

    } else {
        if (read_SUF(fd, F_ADDRESS_TRANSMITTER_COMMANDS, SUF_CONTROL_SET) ==
            -1) {
            return -1;
        }

        assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_UA);
        write(fd, out_frame, sizeof out_frame);
    }
    return 0;
}

static int connection_disconnect(int fd) {
    char out_frame[5];

    if (connection_role == TRANSMITTER) {
        assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_DISC);
        write(fd, out_frame, sizeof out_frame);

        if (read_SUF(fd, F_ADDRESS_TRANSMITTER_COMMANDS, SUF_CONTROL_DISC) ==
            -1) {
            return -1;
        }

        assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_UA);
        write(fd, out_frame, sizeof out_frame);

    } else {
        if (read_SUF(fd, F_ADDRESS_TRANSMITTER_COMMANDS, SUF_CONTROL_DISC) ==
            -1) {
            return -1;
        }

        assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_DISC);
        write(fd, out_frame, sizeof out_frame);

        if (read_SUF(fd, F_ADDRESS_TRANSMITTER_COMMANDS, SUF_CONTROL_UA) ==
            -1) {
            return -1;
        }
    }
    return 0;
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
    newtio.c_cc[VTIME] = CONNECTION_TIMEOUT;
    newtio.c_cc[VMIN] = 0;

    /* Set new port configuration */
    if (tcflush(fd, TCIOFLUSH) == -1) {
        return -1;
    }
    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        return -1;
    }

    bool success = false;
    for (int tries = 0; tries < CONNECTION_MAX_RET; tries++) {
        if (connection_setup(fd, role) == -1) {
            continue;
        } else {
            success = true;
            break;
        }
    }

    return success ? fd : -1;
}

int llclose(int fd) {
    bool success = false;
    for (int tries = 0; tries < CONNECTION_MAX_RET; tries++) {
        if (connection_disconnect(fd) == -1) {
            continue;
        } else {
            success = true;
            break;
        }
    }

    if (!success) {
        fprintf(stdout,
                "Connection was inproperly closed, other end may hang\n");
    }

    if (port_cleanup(fd) == -1) {
        return -1;
    }

    return 0;
}

int llwrite(int fd, char *buffer, int length) {
    int new_length = assemble_iframe(data_frame, aux_frame, TRANSMITTER,
                                     IF_CONTROL(curr_fr_no), buffer, length);
    int next_fr_no = (curr_fr_no + 1) % 2;
    int bytes_written;

    for (int tries = 0; tries < CONNECTION_MAX_RET; tries++) {
        bytes_written = write(fd, data_frame, new_length);

        if (bytes_written == -1) {
            continue;
        }

        if (read_SUF(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                     SUF_CONTROL_RR(next_fr_no)) == -1) {
            continue;
        }

        curr_fr_no = next_fr_no;
        break;
    }

    return curr_fr_no == next_fr_no ? bytes_written - 6 : -1;
}

int llread(int fd, char *buffer) {
    int bytes_read;
    char rr_frame[5];
    int next_fr_no = (curr_fr_no + 1) % 2;

    assemble_suframe(rr_frame, TRANSMITTER, SUF_CONTROL_RR(next_fr_no));

    for (int tries = 0; tries < CONNECTION_MAX_RET; tries++) {
        bytes_read = read_IF(fd, F_ADDRESS_TRANSMITTER_COMMANDS,
                             IF_CONTROL(curr_fr_no), buffer);

        if (bytes_read == -1) {
            continue;
        }

        curr_fr_no = next_fr_no;
        write(fd, rr_frame, sizeof rr_frame);
        break;
    }

    return curr_fr_no == next_fr_no ? bytes_read : -1;
}
