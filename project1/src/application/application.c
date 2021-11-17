#include <stdio.h>

#include "../protocol/frame.h"

static void print_bytes(char *buf, int size) {
    printf("size: %d\n", size);
    for (int i = 0; i < size; i++) {
        printf("%x ", buf[i]);
    }
    printf("\n\n");
}

int main() {
    char mbytes[6] = {0x7a, 0x7e, 0x03, 0x0, 0x0, 0x0};
    int size = 3;
    print_bytes(mbytes, size);
    char out_frame[200];

    int out_frame_size =
        assemble_iframe(out_frame, TRANSMITTER, IF_CONTROL(1), size, mbytes);
    print_bytes(out_frame, out_frame_size);

    destuff_bytes(4 + out_frame, out_frame_size - 6);
    print_bytes(4 + out_frame, 3);
}
