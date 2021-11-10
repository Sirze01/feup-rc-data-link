#include "frame.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline char byte_xor(char *data, int size) {
    if (size < 2)
        return 0;
    char mxor = data[0];
    for (int i = 1; i < size; i++) {
        mxor ^= data[i];
    }
    return mxor;
}

int stuff_bytes(char *data, int size) {
    char *new_data = (char *)malloc(2 * size);
    if (new_data == NULL) {
        perror("Memory allocation for byte stuffying failed");
        return -1;
    }
    int new_size = 0;
    for (int i = 0; i < size; i++) {
        if (data[i] == F_FLAG) {
            new_data[new_size++] = F_ESCAPE_CHAR;
            new_data[new_size++] = 0x5e;
        } else if (data[i] == F_ESCAPE_CHAR) {
            new_data[new_size++] = F_ESCAPE_CHAR;
            new_data[new_size++] = 0x5d;
        } else {
            new_data[new_size++] = data[i];
        }
    }
    memcpy(data, new_data, new_size);
    free(new_data);
    return new_size;
}

int destuff_bytes(char *data, int size) {
    char *new_data = (char *)malloc(size);
    if (new_data == NULL) {
        perror("Memory allocation for byte stuffying failed");
        return -1;
    }
    int new_size = 0;
    for (int i = 0; i < size; i++) {
        if (data[i] == F_ESCAPE_CHAR) {
            if (i == size - 1) {
                perror("Stuffed data is corrupted");
                return -1;
            }
            if (data[i + 1] == 0x5e) {
                new_data[new_size++] = F_FLAG;
                i++;
            } else if (data[i + 1] == 0x5d) {
                new_data[new_size++] = F_ESCAPE_CHAR;
                i++;
            } else {
                perror("Stuffed data is corrupted");
                return -1;
            }
        } else {
            new_data[new_size++] = data[i];
        }
    }
    memcpy(data, new_data, new_size);
    free(new_data);
    return new_size;
}

void assemble_suframe(char *out_frame, DeviceRole role, char ctr) {

    out_frame[0] = F_FLAG;
    out_frame[1] = role == TRANSMITTER ? F_ADDRESS_TRANSMITTER_COMMANDS
                                       : F_ADDRESS_RECEIVER_COMMANDS;
    out_frame[2] = ctr;

    char fields[] = {out_frame[1], out_frame[2]};
    out_frame[3] = byte_xor(fields, sizeof fields);

    out_frame[4] = F_FLAG;
}

void assemble_iframe(char *out_frame, DeviceRole role, char ctr,
                     int unstuffed_data_size, char *unstuffed_data) {
    out_frame[0] = F_FLAG;
    out_frame[1] = role == TRANSMITTER ? F_ADDRESS_TRANSMITTER_COMMANDS
                                       : F_ADDRESS_RECEIVER_COMMANDS;
    out_frame[2] = ctr;

    char fields[] = {out_frame[1], out_frame[2]};
    out_frame[3] = byte_xor(fields, sizeof fields);

    char bcc = byte_xor(unstuffed_data, unstuffed_data_size);

    memcpy(out_frame + 4, unstuffed_data, unstuffed_data_size);
    int stuffed_data_size = stuff_bytes(out_frame + 4, unstuffed_data_size);

    out_frame[4 + stuffed_data_size] = bcc;
    out_frame[5 + stuffed_data_size] = F_FLAG;
}
