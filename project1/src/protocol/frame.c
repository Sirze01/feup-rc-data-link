#include "frame.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

static char aux_frame[IF_FRAME_SIZE];

char byte_xor(char *data, int size) {
    if (size < 2)
        return 0;
    char mxor = data[0];
    for (int i = 1; i < size; i++) {
        mxor ^= data[i];
    }
    return mxor;
}

int stuff_bytes(char *frame, int frame_size) {
    if (frame_size > IF_MAX_UNSTUFFED_FRAME_SIZE) {
        return -1;
    }

    int new_size = 0;
    for (int i = 0; i < frame_size; i++) {
        if (i != 0 && i != frame_size - 1) {
            if (frame[i] == F_FLAG) {
                aux_frame[new_size++] = F_ESCAPE_CHAR;
                aux_frame[new_size++] = 0x5e;
            } else if (frame[i] == F_ESCAPE_CHAR) {
                aux_frame[new_size++] = F_ESCAPE_CHAR;
                aux_frame[new_size++] = 0x5d;
            } else {
                aux_frame[new_size++] = frame[i];
            }
        } else {
            aux_frame[new_size++] = frame[i];
        }
    }
    memcpy(frame, aux_frame, new_size);
    return new_size;
}

int destuff_bytes(char *frame, int frame_size) {
    int new_size = 0;
    for (int i = 0; i < frame_size; i++) {
        if (frame[i] == F_ESCAPE_CHAR) {
            if (i == frame_size - 1) {
                fprintf(stderr, "Stuffed data is corrupted");
                return -1;
            }
            if (frame[i + 1] == 0x5e) {
                aux_frame[new_size++] = F_FLAG;
                i++;
            } else if (frame[i + 1] == 0x5d) {
                aux_frame[new_size++] = F_ESCAPE_CHAR;
                i++;
            } else {
                fprintf(stderr, "Stuffed data is corrupted");
                return -1;
            }
        } else {
            aux_frame[new_size++] = frame[i];
        }
    }
    memcpy(frame, aux_frame, new_size);
    return new_size;
}

void assemble_suframe(char *out_frame, device_role role, char ctr) {

    out_frame[0] = F_FLAG;
    out_frame[1] = role == 0 ? F_ADDRESS_TRANSMITTER_COMMANDS
                             : F_ADDRESS_RECEIVER_COMMANDS;
    out_frame[2] = ctr;

    char fields[] = {out_frame[1], out_frame[2]};
    out_frame[3] = byte_xor(fields, sizeof fields);

    out_frame[4] = F_FLAG;
}

int assemble_iframe(char *out_frame, device_role role, char ctr,
                    char *unstuffed_data, int unstuffed_data_size) {
    out_frame[0] = F_FLAG;
    out_frame[1] = role == 0 ? F_ADDRESS_TRANSMITTER_COMMANDS
                             : F_ADDRESS_RECEIVER_COMMANDS;
    out_frame[2] = ctr;

    char fields[] = {out_frame[1], out_frame[2]};
    out_frame[3] = byte_xor(fields, sizeof fields);

    char bcc = byte_xor(unstuffed_data, unstuffed_data_size);

    memcpy(out_frame + 4, unstuffed_data, unstuffed_data_size);

    out_frame[4 + unstuffed_data_size] = bcc;
    out_frame[5 + unstuffed_data_size] = F_FLAG;

    int stuffed_frame_size = stuff_bytes(out_frame, 6 + unstuffed_data_size);
    return stuffed_frame_size;
}

int read_frame(char *out_frame, int max_frame_size, int fd) {
    if (max_frame_size < SUF_FRAME_SIZE) {
        return -1;
    }

    /* Discard until flag */
    out_frame[0] = 0;
    while (out_frame[0] != F_FLAG) {
        if (read(fd, out_frame, 1) <= 0) {
            return -1;
        }
    }

    /* Read until flag */
    for (int i = 1;; i++) {
        if (i == max_frame_size) {
            return -1;
        }
        if (read(fd, out_frame + i, 1) <= 0) {
            return -1;
        }
        if (out_frame[i] == F_FLAG) {
            return i + 1;
        }
    }
}