#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "data_link_layer.h"

static struct termios oldtio;

device_role connection_role;

static int port_cleanup(int fd) {
    if (tcflush(fd, TCIOFLUSH) == -1) {
        return -1;
    }
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
        return -1;
    }

    if (close(fd) < 0)
        return -1;

    return 0;
}

static int port_setup(int fd) {
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

    return fd;
}

static int connection_setup(int fd, device_role role) {
    connection_role = role;
    char curr_byte;
    bool valid = true;
    bool complete = false;
    char out_frame[5];
    char in_frame[3];

    if (connection_role == TRANSMITTER) {

        assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_SET);
        write(fd, out_frame, sizeof out_frame);

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
                    if (curr_byte != F_ADDRESS_TRANSMITTER_COMMANDS) {
                        valid = false;
                    } else {
                        in_frame[i - 1] = curr_byte;
                    }
                    break;
                case 2:
                    if (curr_byte != SUF_CONTROL_UA) {
                        valid = false;
                    } else {
                        in_frame[i - 1] = curr_byte;
                    }
                    break;
                case 3:
                    in_frame[i - 1] = curr_byte;
                    if (byte_xor(in_frame, sizeof in_frame) != 0) {
                        valid = false;
                    } else {
                        complete = true;
                    }
                    break;
                case 4:
                    if (curr_byte != F_FLAG) {
                        valid = false;
                    }
                    break;
            }
        }

        if (!complete) {
            return -1;
        }

    } else {
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
                    if (curr_byte != F_ADDRESS_TRANSMITTER_COMMANDS) {
                        valid = false;
                    } else {
                        in_frame[i - 1] = curr_byte;
                    }
                    break;
                case 2:
                    if (curr_byte != SUF_CONTROL_SET) {
                        valid = false;
                    } else {
                        in_frame[i - 1] = curr_byte;
                    }
                    break;
                case 3:
                    in_frame[i - 1] = curr_byte;
                    if (byte_xor(in_frame, sizeof in_frame) != 0) {
                        valid = false;
                    } else {
                        complete = true;
                    }
                    break;
                case 4:
                    if (curr_byte != F_FLAG) {
                        valid = false;
                    }
                    break;
                default:
                    break;
            }
        }

        if (!complete) {
            return -1;
        }

        assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_UA);
        write(fd, out_frame, sizeof out_frame);
    }
    return fd;
}

static int connection_disconnect(int fd) {
    char curr_byte;
    bool valid = true;
    bool complete = false;
    char out_frame[5];
    char in_frame[3];

    if (connection_role == TRANSMITTER) {
        assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_DISC);
        write(fd, out_frame, sizeof out_frame);
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
                    if (curr_byte != F_ADDRESS_TRANSMITTER_COMMANDS) {
                        valid = false;
                    } else {
                        in_frame[i - 1] = curr_byte;
                    }
                    break;
                case 2:
                    if (curr_byte != SUF_CONTROL_DISC) {
                        valid = false;
                    } else {
                        in_frame[i - 1] = curr_byte;
                    }
                    break;
                case 3:
                    in_frame[i - 1] = curr_byte;
                    if (byte_xor(in_frame, sizeof in_frame) != 0) {
                        valid = false;
                    } else {
                        complete = true;
                    }
                    break;
                case 4:
                    if (curr_byte != F_FLAG) {
                        valid = false;
                    }
                    break;
            }
        }

        if (!complete) {
            return -1;
        }

        assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_UA);
        write(fd, out_frame, sizeof out_frame);

    } else {
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
                    if (curr_byte != F_ADDRESS_TRANSMITTER_COMMANDS) {
                        valid = false;
                    } else {
                        in_frame[i - 1] = curr_byte;
                    }
                    break;
                case 2:
                    if (curr_byte != SUF_CONTROL_DISC) {
                        valid = false;
                    } else {
                        in_frame[i - 1] = curr_byte;
                    }
                    break;
                case 3:
                    in_frame[i - 1] = curr_byte;
                    if (byte_xor(in_frame, sizeof in_frame) != 0) {
                        valid = false;
                    } else {
                        complete = true;
                    }
                    break;
                case 4:
                    if (curr_byte != F_FLAG) {
                        valid = false;
                    }
                    break;
                default:
                    break;
            }
        }

        if (!complete) {
            return -1;
        }

        assemble_suframe(out_frame, TRANSMITTER, SUF_CONTROL_DISC);
        write(fd, out_frame, sizeof out_frame);

        complete = false;

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
                    if (curr_byte != F_ADDRESS_TRANSMITTER_COMMANDS) {
                        valid = false;
                    } else {
                        in_frame[i - 1] = curr_byte;
                    }
                    break;
                case 2:
                    if (curr_byte != SUF_CONTROL_UA) {
                        valid = false;
                    } else {
                        in_frame[i - 1] = curr_byte;
                    }
                    break;
                case 3:
                    in_frame[i - 1] = curr_byte;
                    if (byte_xor(in_frame, sizeof in_frame) != 0) {
                        valid = false;
                    } else {
                        complete = true;
                    }
                    break;
                case 4:
                    if (curr_byte != F_FLAG) {
                        valid = false;
                    }
                    break;
                default:
                    break;
            }
        }

        if (!complete) {
            return -1;
        }
    }
    return fd;
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

    if (port_setup(fd) < 0) {
        fprintf(stderr, "Failed configuring port\n");
        return -1;
    }

    if (connection_setup(fd, role) < 0) {
        fprintf(stderr, "Failed establishing connection\n");
        port_cleanup(fd);
        return -1;
    }

    return fd;
}

int llclose(int fd) {

    if (connection_disconnect(fd) < 0) {
        return -1;
    }

    if (port_cleanup(fd) < 0)
        return -1;

    return 0;
}
