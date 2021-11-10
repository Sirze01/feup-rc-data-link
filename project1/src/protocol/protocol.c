#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "protocol.h"

static struct termios oldtio;

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
    newtio.c_cc[VTIME] = 0;
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