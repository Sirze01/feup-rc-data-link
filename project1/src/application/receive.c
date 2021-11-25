#include "receive.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// #include "../protocol/data_link.h"
#include "packet.h"

#define DEFAULT_OUT_FILE_NAME "out_file"

static char packet[4096];

int receive_file(char *out_file_path, int port) {
    /* Open stream */
    // llopen();

    /* Read start control packet */
    int fsize = -1;
    int namesize = -1;
    // llread();                            // to packet...
    int read_bytes = 4;                  // from llread...
    if (packet[0] != CP_CONTROL_START) { // repeat...
    } else {
        for (int i = 2, arg = 0; arg < packet[1]; arg++) {
            switch (arg) {
                case 0:
                    memcpy(&fsize, (void *)packet + i + 1, packet[i]);
                    i += packet[i] + 1;
                    break;
                case 1:
                    memcpy(out_file_path, (void *)packet + i + 1, packet[i]);
                    i += packet[i] + 1;
                default:
                    fprintf(stderr, "Unsupported control packet argument");
                    return -1;
            }
        }
    }

    /* Read from stream and write to file */
    // llread();
    //  write...

    /* Read close control packet */
    // read...

    // llclose();

    return -1;
}