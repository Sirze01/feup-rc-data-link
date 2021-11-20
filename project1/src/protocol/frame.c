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

int stuff_bytes(char *data, char *aux_data, int data_size) {
    int new_size = 0;
    for (int i = 0; i < data_size; i++) {
        if (data[i] == F_FLAG) {
            aux_data[new_size++] = F_ESCAPE_CHAR;
            aux_data[new_size++] = 0x5e;
        } else if (data[i] == F_ESCAPE_CHAR) {
            aux_data[new_size++] = F_ESCAPE_CHAR;
            aux_data[new_size++] = 0x5d;
        } else {
            aux_data[new_size++] = data[i];
        }
    }
    memcpy(data, aux_data, new_size);
    return new_size;
}

int destuff_bytes(char *data, char *aux_data, int data_size) {
    int new_size = 0;
    for (int i = 0; i < data_size; i++) {
        if (data[i] == F_ESCAPE_CHAR) {
            if (i == data_size - 1) {
                perror("Stuffed data is corrupted");
                return -1;
            }
            if (data[i + 1] == 0x5e) {
                aux_data[new_size++] = F_FLAG;
                i++;
            } else if (data[i + 1] == 0x5d) {
                aux_data[new_size++] = F_ESCAPE_CHAR;
                i++;
            } else {
                perror("Stuffed data is corrupted");
                return -1;
            }
        } else {
            aux_data[new_size++] = data[i];
        }
    }
    memcpy(data, aux_data, new_size);
    return new_size;
}

void assemble_suframe(char *out_frame, device_role role, char ctr) {

    out_frame[0] = F_FLAG;
    out_frame[1] = role == TRANSMITTER ? F_ADDRESS_TRANSMITTER_COMMANDS
                                       : F_ADDRESS_RECEIVER_COMMANDS;
    out_frame[2] = ctr;

    char fields[] = {out_frame[1], out_frame[2]};
    out_frame[3] = byte_xor(fields, sizeof fields);

    out_frame[4] = F_FLAG;
}

int assemble_iframe(char *out_frame, char *aux_frame, device_role role,
                    char ctr, char *unstuffed_data, int unstuffed_data_size) {
    out_frame[0] = F_FLAG;
    out_frame[1] = role == TRANSMITTER ? F_ADDRESS_TRANSMITTER_COMMANDS
                                       : F_ADDRESS_RECEIVER_COMMANDS;
    out_frame[2] = ctr;

    char fields[] = {out_frame[1], out_frame[2]};
    out_frame[3] = byte_xor(fields, sizeof fields);

    char bcc = byte_xor(unstuffed_data, unstuffed_data_size);

    memcpy(out_frame + 4, unstuffed_data, unstuffed_data_size);
    int stuffed_data_size =
        stuff_bytes(out_frame + 4, aux_frame, unstuffed_data_size);

    out_frame[4 + stuffed_data_size] = bcc;
    out_frame[5 + stuffed_data_size] = F_FLAG;

    return 6 + stuffed_data_size;
}
