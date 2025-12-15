#define _XOPEN_SOURCE 500

#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

#define MIN_CHUNK_SIZE 1024

// Reads exactly count bytes from the file at the given offset.
void try_read_exact(int fd, char *buffer, size_t count, off_t offset) {
    // Loop until we have read what we expect.
    size_t total_read = 0;
    while (total_read < count) {
        ssize_t c = pread(fd, buffer + total_read, count - total_read, offset + total_read);
        if (c <= 0) {
            perror("rmcat: pread");
            exit(EXIT_FAILURE);
        }
        total_read += c;
    }
}

// Writes exactly count bytes to the file at the given offset.
void try_write_exact(int fd, char *buffer, size_t count, off_t offset) {
    // Loop until we have written what we expect.
    size_t total_written = 0;
    while (total_written < count) {
        ssize_t c = pwrite(fd, buffer + total_written, count - total_written, offset + total_written);
        if (c <= 0) {
            perror("rmcat: pwrite");
            exit(EXIT_FAILURE);
        }
        total_written += c;
    }
}

void try_write_to_stdout(char *buffer, size_t count) {
    // Loop until we have written what we expect.
    size_t total_written = 0;
    while (total_written < count) {
        ssize_t c = write(STDOUT_FILENO, buffer + total_written, count - total_written);
        if (c <= 0) {
            perror("rmcat: write to stdout");
            exit(EXIT_FAILURE);
        }
        total_written += c;
    }
}

void try_truncate(int fd, off_t length) {
    if (ftruncate(fd, length) < 0) {
        perror("rmcat: ftruncate");
        exit(EXIT_FAILURE);
    }
}

// Calculate a chunk size of `max(MIN_CHUNK_SIZE, min(file_size/100, system_memory/100))
size_t try_calculate_chunk_size(size_t file_size) {
    // Get the system memory.
    struct sysinfo si;
    if (sysinfo(&si) < 0) {
        perror("rmcat: sysinfo");
        exit(EXIT_FAILURE);
    }

    size_t system_memory = si.totalram * si.mem_unit;

    size_t chunk_size;
    if (file_size < system_memory) {
        chunk_size = file_size / 100;
    } else {
        chunk_size = system_memory / 100;
    }
    if (chunk_size < MIN_CHUNK_SIZE) {
        chunk_size = MIN_CHUNK_SIZE;
    }
    return chunk_size;
}

int main(int argc, char *argv[])
{
    // Check we have the correct number of arguments.
    if (argc != 2) {
        printf("Usage: rmcat [FILE]\n");
        printf("Writes FILE to standard output, truncating the file as it is read.\n");
        printf("Copyright (C) 2025 George Bott (https://github.com/Yen/rmcat)\n");
        exit(EXIT_FAILURE);
    }

    // Open the file for reading and writing.
    int fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        perror("rmcat: open");
        exit(EXIT_FAILURE);
    }

    // Delete the file from the filesystem.
    if (unlink(argv[1]) < 0) {
        perror("rmcat: unlink");
        exit(EXIT_FAILURE);
    }

    // Get the file size.
    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("rmcat: fstat");
        exit(EXIT_FAILURE);
    }
    size_t file_size = (size_t)st.st_size;
    
    // Calculate the chunk size to use.
    size_t chunk_size = try_calculate_chunk_size(file_size);

    // Calculate the number of chunks and the size of the final chunk.
    ssize_t last_chunk = file_size / chunk_size;
    size_t final_chunk_size = file_size % chunk_size;
    if (final_chunk_size == 0) {
        last_chunk -= 1;
        final_chunk_size = chunk_size;
    }

    // Allocate the buffer for reading chunks.
    char *buffer = malloc(chunk_size);
    if (buffer == NULL) {
        perror("rmcat: malloc");
        exit(EXIT_FAILURE);
    }

    // The first pass reads chunks from the start of the file and writes them to stdout.
    // After each chunk is written, it is replaced by the chunk at the end of the file.
    // The end of the file is then after each end chunk is moved.
    // Once this process ends, roughly half of the file has been written to stdout with
    // the remaining chunks being in reverse order from the start of the file.
    ssize_t next_chunk = 0;
    while (next_chunk < last_chunk) {
        // Read the next chunk.
        try_read_exact(fd, buffer, chunk_size, next_chunk * chunk_size);
        // Write the next chunk to stdout.
        try_write_to_stdout(buffer, chunk_size);

        // If we are on the first chunk at the start, then we are on the last chunk at the end,
        // so make sure we are using the final chunk size.
        size_t last_chunk_size = next_chunk == 0 ? final_chunk_size : chunk_size;

        // Read the last chunk.
        try_read_exact(fd, buffer, last_chunk_size, last_chunk * chunk_size);
        // Write the last chunk to where the next chunk was written.
        try_write_exact(fd, buffer, last_chunk_size, next_chunk * chunk_size);
        
        // Truncate the file to remove unnecessary data.
        try_truncate(fd, last_chunk * chunk_size);

        next_chunk++;
        last_chunk--;
    }

    // The second pass now has the file with chunks in reverse order.
    // Read chunks from the end of the file and write them to stdout.
    // The end of the file is truncated after each chunk is written.
    while (last_chunk >= 0) {
        // If we are on the first chunk at the start, then this is our last chunk
        // as the chunks are reversed, so make sure we are using the final chunk size.
        size_t last_chunk_size = last_chunk == 0 ? final_chunk_size : chunk_size;

        // Read the last chunk.
        try_read_exact(fd, buffer, last_chunk_size, last_chunk * chunk_size);
        // Write the last chunk to stdout.
        try_write_to_stdout(buffer, last_chunk_size);

        // Truncate the file to remove unnecessary data.
        try_truncate(fd, last_chunk * chunk_size);
        
        last_chunk--;
    }

    return EXIT_SUCCESS;
}