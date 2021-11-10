#include "frame.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

inline char byte_xor(char *data, int size) {
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

SUFrame assemble_suframe(DeviceRole role, char ctr) {
    SUFrame frame;

    frame.frame[0] = F_FLAG;
    frame.frame[1] = role == TRANSMITTER ? F_ADDRESS_TRANSMITTER_COMMANDS
                                         : F_ADDRESS_RECEIVER_COMMANDS;
    frame.frame[2] = ctr;

    char fields[] = {frame.frame[1], frame.frame[2]};
    frame.frame[3] = byte_xor(fields, sizeof fields);

    frame.frame[4] = F_FLAG;

    return frame;
}

IFrame assemble_iframe(DeviceRole role, char ctr, int size, char *data) {
    IFrame frame;

    frame.frame[0] = F_FLAG;
    frame.frame[1] = role == TRANSMITTER ? F_ADDRESS_TRANSMITTER_COMMANDS
                                         : F_ADDRESS_RECEIVER_COMMANDS;
    frame.frame[2] = ctr;

    char fields[] = {frame.frame[1], frame.frame[2]};
    frame.frame[3] = byte_xor(fields, sizeof fields);

    frame.data_size = size;
    memcpy(frame.frame + 4, data, frame.data_size);

    frame.frame[4 + frame.data_size] = byte_xor(data, frame.data_size);
    frame.frame[5 + frame.data_size] = F_FLAG;

    return frame;
}
