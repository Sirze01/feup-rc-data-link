#include <fcntl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>

#include "protocol.h"

static struct termios oldPortConfig;

int llopen(int port, char role) {
    char portPath[PATH_MAX];
    if (role != TRANSMITTER && role != RECEIVER) {
        fprintf(stderr, "Unrecogized role\n");
        return -1;
    }

    int errorCheck = snprintf(portPath, PATH_MAX, "/dev/ttyS%d", port);
    if (errorCheck < 0) {
        fprintf(stderr, "Output error parsing port path\n");
        return -1;
    } else if (errorCheck >= PATH_MAX) {
        fprintf(stderr, "Unreachable port path\n");
        return -1;
    }

    int portDescriptor = open(portPath, O_RDWR | O_NOCTTY);
    if (portDescriptor == -1) {
        perror("Error opening port");
        return -1;
    }

    struct termios newtio, newtioCmp;

    if (tcgetattr(portDescriptor, &oldPortConfig) == -1) {
        perror("Failed reading port configuration");
        return -1;
    }

    bzero(&newtio, sizeof newtio);

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN] = 0;

    if (tcflush(portDescriptor, TCIOFLUSH) == -1) {
        perror("Failed cleaning pending operations on the port");
        return -1;
    }

    if (tcsetattr(portDescriptor, TCSANOW, &newtio) == -1) {
        perror("Failed pushing new port configuration");
        return -1;
    }

    if (tcgetattr(portDescriptor, &newtioCmp) == -1) {
        perror("Failed reading port configuration");
        return -1;
    }

    if (memcmp(&newtioCmp, &newtio, sizeof newtio) < 0) {
        perror("Failed pushing new port configuration");
        return -1;
    }

    return -1;
}