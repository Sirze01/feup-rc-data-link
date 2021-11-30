#include <stdio.h>

#include "utils.h"

#define clear_screen() printf("\033[H\033[J")
#define PERCENTAGE_BAR_WIDTH 50

static int last_percentage = -1;

void print_progress_bar(unsigned curr_byte, unsigned file_size) {
    float percentage = (float)curr_byte / (float)file_size;
    int percentage_ = (int)(percentage * 100);
    if (percentage_ != last_percentage) {
        last_percentage = percentage_;
        clear_screen();
        printf("[");
        int pos = PERCENTAGE_BAR_WIDTH * percentage;
        for (int i = 0; i < PERCENTAGE_BAR_WIDTH; i++) {
            if (i < pos) {
                printf("=");
            } else if (i == pos) {
                printf(">");
            } else {
                printf(" ");
            }
        }
        printf("] %d %%\n", percentage_);
        printf("\r%u bytes out of %u\n\n", curr_byte, file_size);
        fflush(stdout);
    }
}
