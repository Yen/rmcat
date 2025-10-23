# rmcat - rm+cat

A utility that deletes a file progressively while writing its contents to stdout.

The rationale for this utility was to be able to process a file, such as extracting an archive, where there is not enough space on disk to hold both the file and processed contents. In this case you would want to progressively delete the file as you process it, which is what this utility allows. (`rmcat file.tar.gz | tar xz`).

**It is important to note that if the process is interrupted, the file will be left in an inconsistent state due to the truncating algorithm. There is no way to recover from this state.**

## Implementation

The utility makes use of `ftruncate` system call to shrink the size of the file as it reads it. As `ftruncate` can only shrink a file from the end, but want to process the file from the beginning, the data for the file needs to be reversed on-disk in some way. The way this is achieved is by reading a chunk the start of the file, writing it to stdout, then replacing it with a chunk from the end of the file after which the file is truncated. Once we have walked the whole file, roughly half the file has been written to stdout with the other half now in chunk-wise reverse order at the start of the file. We can then read chunks from the end of the file (undoing this chunk-wise reverse) and write it to stdout while truncating.