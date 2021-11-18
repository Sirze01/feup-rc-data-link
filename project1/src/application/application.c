#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../protocol/data_link.h"

#define PATH_MAX 4096

static int fd = -1;
static int port = -1;
static char file_path[PATH_MAX];
static char file_name[PATH_MAX];
static device_role role;

void parse_options(int argc, char **argv) {
    int opt;
    bool got_role = false;
    while ((opt = getopt(argc, argv, ":p:s:r:n:")) != -1) {
        switch (opt) {
            case 'p':
                errno = 0;
                port = atoi(optarg);
                if (errno != 0) {
                    perror("Port must be a valid number\n");
                    exit(-1);
                }
                break;
            case 's':
            case 'r':
                if (got_role) {
                    printf("Cannot receive and send at the same time\n");
                    exit(-1);
                }
                got_role = true;
                strncpy(file_path, optarg, PATH_MAX);
                break;
            case 'n':
                strncpy(file_name, optarg, PATH_MAX);
                break;
            case ':':
            case '?':
            default:
                printf("Usage: %s -p <port> -s|-r <filepath> -n [filename]\n",
                       argv[0]);
                exit(-1);
                break;
        }
    }
}

void assert_valid_options() {
    if (port < 0 || port > 3) {
        printf("Port should be one of 0, 1, 2, 3\n");
        exit(-1);
    }
}

int main(int argc, char **argv) {
    parse_options(argc, argv);
    assert_valid_options();
}
