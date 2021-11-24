#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../protocol/data_link_layer.h"
#include "../protocol/frame.h"

/*
static void print_bytes(char *buf, int size) {
    printf("size: %d\n", size);
    for (int i = 0; i < size; i++) {
        printf("%x ", buf[i]);
    }
    printf("\n\n");
}
*/

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    /*
    char mbytes[12] = {0x7a, 0x7e, 0x7d, 0x0, 0x0, 0x0};
    char aux[12];
    int size = 3;
    print_bytes(mbytes, size);

    int nsize = stuff_bytes(mbytes, aux, size);

    print_bytes(mbytes, nsize);

    nsize = destuff_bytes(mbytes, aux, nsize);
    print_bytes(mbytes, nsize);
    */

    int fd;
    if ((fd = llopen(atoi(argv[1]), atoi(argv[2]))) < 0) {
        fprintf(stdout, "Fails setting connection\n");
        return -1;
    }

    fprintf(stdout, "Connection set\n");

    if (atoi(argv[2]) == 0) {
        char hello[] = "Tu és uma mensagem que viaja pelos cabos\n";
        printf("%d\n", llwrite(fd, hello, sizeof hello));
    } else {
        char arr[20];
        printf("%d -> ", llread(fd, arr));
        printf("this : %s", arr);
    }

    if (llclose(fd) < 0) {
        fprintf(stdout, "Fail closing\n");
        return -1;
    }

    fprintf(stdout, "Disconnected\n");

    return 0;
}
