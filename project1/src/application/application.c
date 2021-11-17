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
    char mbytes[12] = {0x7a, 0x7e, 0x7d, 0x0, 0x0, 0x0};
    char aux[12];
    int size = 3;
    print_bytes(mbytes, size);

    int nsize = stuff_bytes(mbytes, aux, size);

    print_bytes(mbytes, nsize);

    nsize = destuff_bytes(mbytes, aux, nsize);
    print_bytes(mbytes, nsize);
}
