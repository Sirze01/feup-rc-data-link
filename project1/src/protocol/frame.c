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

SUFrame *assemble_suframe(DeviceRole role, char ctr) {
    SUFrame *frame = (SUFrame *)malloc(sizeof(struct SUFrame));
    if (frame == NULL) {
        perror("Memory allocation for suframe failed");
        return NULL;
    }

    frame->flg = F_FLAG;
    frame->addr = role == TRANSMITTER ? IF_ADDRESS_SENDER_COMMANDS
                                      : IF_ADDRESS_RECEIVER_COMMANDS;
    frame->ctr = ctr;

    char fields[] = {frame->addr, frame->ctr};
    frame->bcc1 = byte_xor(fields, sizeof fields);

    frame->frame = (char *)malloc(SUF_FRAME_SIZE);
    frame->frame[0] = frame->flg;
    frame->frame[1] = frame->addr;
    frame->frame[2] = frame->ctr;
    frame->frame[3] = frame->bcc1;
    frame->frame[4] = frame->flg;

    return frame;
}

IFrame *assemble_iframe(DeviceRole role, char ctr, int size, char *data) {
    IFrame *frame = (IFrame *)malloc(sizeof(struct IFrame));
    if (frame == NULL) {
        perror("Memory allocation for iframe failed");
        return NULL;
    }

    frame->flg = F_FLAG;
    frame->addr = role == TRANSMITTER ? IF_ADDRESS_SENDER_COMMANDS
                                      : IF_ADDRESS_RECEIVER_COMMANDS;
    frame->ctr = ctr;

    char fields[] = {frame->addr, frame->ctr};
    frame->bcc1 = byte_xor(fields, sizeof fields);

    frame->data_size = size;
    frame->data = data;

    frame->frame =
        (char *)malloc(IF_FIELDS_SIZE + frame->data_size * sizeof(char));
    frame->frame[0] = frame->flg;
    frame->frame[1] = frame->addr;
    frame->frame[2] = frame->ctr;
    frame->frame[3] = frame->bcc1;

    for (int framei = 4, datai = 0; datai < frame->data_size;
         framei++, datai++) {
        frame->frame[framei] = frame->data[datai];
    }

    frame->bcc2 = byte_xor(frame->data, frame->data_size);
    frame->frame[3 + frame->data_size + 1] = frame->bcc2;
    frame->frame[3 + frame->data_size + 2] = frame->flg;

    return frame;
}

void destroy_SUFrame(SUFrame *frame) {
    free(frame->frame);
    free(frame);
}

void destroy_IFrame(IFrame *frame) {
    free(frame->frame);
    free(frame);
}
