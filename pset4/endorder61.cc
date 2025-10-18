#include "io61.hh"

// Usage: ./endorder61 [-b BLOCKSIZE] [-B MAXBLOCKSIZE] [-s SIZE]
//                     [-o OUTFILE] [FILE]
//    Copies the input FILE to OUTFILE in blocks, shuffling its
//    contents. Before each block, jumps to the end of the input FILE
//    and reads 16 bytes; this models a task like PDF manipulation or
//    executable linking where input files have an end table that lists
//    important file offsets. Writes sequentially. Default BLOCKSIZE is
//    1024.

int main(int argc, char* argv[]) {
    // Parse arguments
    io61_args args = io61_args("b:B:s:i:o:r:A:RW", 1024).set_seed(83419)
        .parse(argc, argv);

    // Allocate buffer, open files, measure file sizes
    unsigned char* buf = new unsigned char[args.max_block_size];
    io61_file* inf = io61_open_check(args.input_file, O_RDONLY);
    if (io61_seek(inf, 0) < 0) {
        fprintf(stderr, "endorder61: input file is not seekable\n");
        exit(1);
    }
    io61_file* outf = io61_open_check(args.output_file,
                                      O_WRONLY | O_CREAT | O_TRUNC);

    if (ssize_t(args.file_size) < 0) {
        args.file_size = io61_filesize(inf);
    }
    if (ssize_t(args.file_size) <= 0) {
        fprintf(stderr, "endorder61: can't get size of input file\n");
        exit(1);
    }

    // Compute position of “file offset table”
    size_t end_block = 16 * (((args.file_size - 1) / args.block_size) + 1);
    size_t end_offset = ((args.file_size - end_block) / args.block_size)
        * args.block_size;
    size_t end_pos = end_offset;

    // Create random distribution for block sizes
    using distrib_type = std::uniform_int_distribution<size_t>;
    distrib_type block_distrib(args.block_size, args.max_block_size);

    // Copy file data
    size_t written = 0;
    while (written < args.file_size) {
        // Read 16 bytes from “file offset table”
        int r = io61_seek(inf, end_pos);
        assert(r >= 0);
        ssize_t nr = io61_read(inf, buf, 16); // ignore errors
        if (nr == 16) {
            end_pos += 16;
        }

        // Compute position and block size
        size_t bs = block_distrib(args.engine);
        if (bs > args.file_size - written) {
            bs = args.file_size - written;
        }
        size_t pos;
        if (bs < end_offset) {
            distrib_type pos_distrib(0, end_offset - bs);
            pos = pos_distrib(args.engine);
        } else {
            pos = 0;
        }

        // Copy a block
        // (By default, read/write a full block; if requested, read/write
        // bytewise with many io61_readc/writec calls.)
        r = io61_seek(inf, pos);
        assert(r >= 0);
        if (args.read_bytewise) {
            nr = io61_read_bytewise(inf, buf, bs);
        } else {
            nr = io61_read(inf, buf, bs);
        }
        if (nr <= 0) {
            break;
        }

        ssize_t nw;
        if (args.write_bytewise) {
            nw = io61_write_bytewise(outf, buf, nr);
        } else {
            nw = io61_write(outf, buf, nr);
        }
        assert(nw == nr);

        written += nw;
        args.after_write(outf);
    }

    io61_close(inf);
    io61_close(outf);
    delete[] buf;
}
