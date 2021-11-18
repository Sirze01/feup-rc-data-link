#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "data_link.h"
#include "frame.h"

#define MAX_FRAME_SIZE 8096
#define BAUDRATE B38400

static struct {
    char port[20];
    int baud_rate;
    unsigned int sequence_number;
    unsigned int timeout;
    unsigned int no_retransmissions;
    struct termios oldtio;
} config;

static char out_frame[MAX_FRAME_SIZE];
static char aux_frame[MAX_FRAME_SIZE];

int llopen(int port, device_role role) {
    /* Assemble device file path */
    int c = snprintf(config.port, sizeof config.port, "/dev/ttyS%d", port);
    if (c < 0) {
        fprintf(stderr, "Output error parsing port path\n");
        return -1;
    } else if (c >= PATH_MAX) {
        fprintf(stderr, "Unreachable port path\n");
        return -1;
    }

    /* Open port */
    int fd = open(config.port, O_RDWR | O_NOCTTY);
    if (fd == -1) {
        perror("Error opening port");
        return -1;
    }

    /* Save current port configuration for later restoration */
    if (tcgetattr(fd, &config.oldtio) == -1) {
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
    if (tcsetattr(fd, TCSANOW, &config.oldtio) == -1) {
        perror("Failed pushing new port configuration");
        return -1;
    }

    /* Assert valid port configuration */
    struct termios newtio_cmp;
    if (tcgetattr(fd, &newtio_cmp) == -1) {
        perror("Failed restoring configuration");
        return -1;
    }
    if (memcmp(&newtio_cmp, &config.oldtio, sizeof config.oldtio) != 0) {
        perror("Failed restoring port configuration");
        return -1;
    }

    return -1;
}
