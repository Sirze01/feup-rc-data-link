#pragma once

int send_file(char *file_path, char *file_name, int port,
              int bytes_per_packet);