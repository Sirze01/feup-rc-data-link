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
    char mbytes[6] = {0x7d, 0x7e, 0x03, 0x0, 0x0, 0x0};
    int size = 3;
    print_bytes(mbytes, size);
    size = byte_stuff(mbytes, size);
    print_bytes(mbytes, size);
    size = byte_destuff(mbytes, size);
    print_bytes(mbytes, size);
}
