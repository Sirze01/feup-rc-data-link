#include "utils.h"
#include <stdio.h>

void print_transfer_progress_bar(unsigned curr_byte, unsigned file_size) {
    static int last_percentage = -1;
    static const int percentage_bar_width = 30;
    float percentage = (float)curr_byte / (float)file_size;
    int percentage_ = (int)(percentage * 100);
    if (last_percentage != percentage_) {
        last_percentage = percentage_;
        printf("\r[");
        int pos = percentage_bar_width * percentage;
        for (int i = 0; i < percentage_bar_width; i++) {
            if (i < pos) {
                printf("=");
            } else if (i == pos) {
                printf(">");
            } else {
                printf(" ");
            }
        }
        printf("] %d%% (%u/%uB) ", percentage_, curr_byte, file_size);
    }
}

double elapsed_seconds(struct timespec *start, struct timespec *end) {
    double start_secs_decimal = (double)start->tv_nsec / 1000000000;
    double end_secs_decimal = (double)end->tv_nsec / 1000000000;
    double start_secs = (double)start->tv_sec + start_secs_decimal;
    double end_secs = (double)end->tv_sec + end_secs_decimal;
    return end_secs - start_secs;
}

void print_bytes(unsigned char *buf, int size) {
    printf("size: %d\n", size);
    for (int i = 0; i < size; i++) {
        printf("%x ", buf[i]);
    }
    printf("\n\n");
}