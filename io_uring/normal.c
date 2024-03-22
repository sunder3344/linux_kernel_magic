#include <stdio.h>
#include <stdlib.h>

#define BUF_SIZE 4096

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source_file> <dest_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *source_file = argv[1];
    const char *dest_file = argv[2];

    FILE *source_fp = fopen(source_file, "rb");
    if (!source_fp) {
        perror("Error opening source file");
        exit(EXIT_FAILURE);
    }

    FILE *dest_fp = fopen(dest_file, "wb");
    if (!dest_fp) {
        perror("Error opening destination file");
        fclose(source_fp);
        exit(EXIT_FAILURE);
    }

    char buffer[BUF_SIZE];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), source_fp)) > 0) {
        size_t bytes_written = fwrite(buffer, 1, bytes_read, dest_fp);
        if (bytes_written != bytes_read) {
            perror("Error writing to destination file");
            fclose(source_fp);
            fclose(dest_fp);
            exit(EXIT_FAILURE);
        }
    }

    if (ferror(source_fp)) {
        perror("Error reading source file");
        fclose(source_fp);
        fclose(dest_fp);
        exit(EXIT_FAILURE);
    }

    fclose(source_fp);
    fclose(dest_fp);

    printf("File copied successfully.\n");

    return 0;
}

