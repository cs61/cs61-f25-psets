#include "io61.hh"
#include <ctime>
#include <csignal>
#include <climits>
#include <cerrno>
#include <sys/time.h>
#include <sys/resource.h>

// helpers.cc
//    The io61_args() structure parses command line arguments.
//    The profile functions measure how much time and memory are used
//    by your code.


// fd_open_check(filename, mode)
//    Like `io61_open_check`, but returns a file descriptor.

int fd_open_check(const char* filename, int mode) {
    int fd;
    if (filename) {
        fd = open(filename, mode, 0666);
    } else if ((mode & O_ACCMODE) == O_RDONLY) {
        return STDIN_FILENO;
    } else {
        return STDOUT_FILENO;
    }
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        exit(1);
    }
    return fd;
}


// stdio_open_check(filename, mode)
//    Like `io61_open_check`, but returns a stdio file.

FILE* stdio_open_check(const char* filename, int mode) {
    int fd = fd_open_check(filename, mode);
    if (filename) {
        const char* modestr;
        if ((mode & O_ACCMODE) == O_RDONLY) {
            modestr = "rb";
        } else if ((mode & O_ACCMODE) == O_WRONLY) {
            modestr = "wb";
        } else {
            modestr = "r+b";
        }
        return fdopen(fd, modestr);
    } else if ((mode & O_ACCMODE) == O_RDONLY) {
        return stdin;
    } else {
        return stdout;
    }
}


// monotonic_timestamp()
//    Returns the current monotonic timestamp.

double monotonic_timestamp() {
    timespec t;
    int r = clock_gettime(CLOCK_MONOTONIC, &t);
    assert(r == 0);
    return t.tv_sec + t.tv_nsec * 1e-9;
}


// io61_read_bytewise(f, buf, sz)
//    Read a block of `sz` bytes into `buf`, but using `io61_readc` calls.

ssize_t io61_read_bytewise(io61_file* f, unsigned char* buf, size_t sz) {
    size_t nr = 0;
    while (nr != sz) {
        int ch = io61_readc(f);
        if (ch < 0) {
            break;
        }
        buf[nr] = ch;
        ++nr;
    }
    return nr;
}


// io61_write_bytewise(f, buf, sz)
//    Write a block of `sz` bytes from `buf`, but using `io61_writec` calls.

ssize_t io61_write_bytewise(io61_file* f, const unsigned char* buf, size_t sz) {
    size_t nw = 0;
    while (nw != sz) {
        if (io61_writec(f, buf[nw]) < 0) {
            break;
        }
        ++nw;
    }
    return nw;
}


// io61_args functions

io61_args::io61_args(const char* opts_, size_t bs)
    : block_size(bs), max_block_size(bs), opts(opts_) {
}

io61_args& io61_args::set_block_size(size_t bs) {
    this->block_size = this->max_block_size = bs;
    return *this;
}

io61_args& io61_args::set_seed(unsigned seed_) {
    this->engine.seed(seed_);
    this->seed = seed_;
    return *this;
}

extern "C" {
static void sigalrm_handler(int) {
}
}

std::optional<size_t> io61_args::parse_size(const char* s) {
    const char* ends = s + strlen(s);
    char* ptr = const_cast<char*>(s);
    size_t v = 0;
    errno = 0;
    if (s[0] == '0' && tolower((unsigned char) s[1]) == 'x') {
        v = strtoul(s + 2, &ptr, 16);
    } else if (isdigit((unsigned char) s[0])) {
        v = strtoul(s, &ptr, 10);
    }
    if (ptr == s && *s == '.') {
        // parse floating point
    } else if (ptr == s || (v == ULONG_MAX && errno == ERANGE)) {
        return std::nullopt;
    } else if (ptr == ends) {
        return v;
    }

    double fv;
    if (s[0] == '0' && tolower((unsigned char) s[1]) == 'x') {
        fv = v;
    } else {
        fv = strtod(s, &ptr);
        if (ptr == s || strchr(s, 'e') || strchr(s, 'E')) {
            return std::nullopt;
        }
    }

    // parse `k`/`m`/`g` suffixes
    if (ptr != ends) {
        char ch = tolower((unsigned char) *ptr);
        if (ch == 'k') {
            fv *= 1024;
        } else if (ch == 'm') {
            fv *= 1024 * 1024;
        } else if (ch == 'g') {
            fv *= 1024 * 1024 * 1024;
        } else {
            return std::nullopt;
        }
        if (ptr + 1 != ends) {
            return std::nullopt;
        }
    }

    if (round(fv) != fv || fv > SIZE_MAX) {
        return std::nullopt;
    }
    return fv;
}

io61_args& io61_args::parse(int argc, char** argv) {
    this->program_name = argv[0];
    size_t bs = this->block_size;
    size_t max_bs = this->max_block_size;
    double alarm_interval = 0;

    int arg;
    char* endptr;
    while ((arg = getopt(argc, argv, this->opts)) != -1) {
        switch (arg) {
        case 's':
            if (auto sz = parse_size(optarg)) {
                this->file_size = *sz;
            } else {
                goto usage;
            }
            break;
        case 'b': {
            auto sz = parse_size(optarg);
            if (!sz || *sz == 0) {
                goto usage;
            }
            bs = *sz;
            break;
        }
        case 'B': {
            auto sz = parse_size(optarg);
            if (!sz || *sz == 0) {
                goto usage;
            }
            max_bs = *sz;
            break;
        }
        case 'R':
            this->read_bytewise = true;
            break;
        case 'W':
            this->write_bytewise = true;
            break;
        case 't': {
            auto sz = parse_size(optarg);
            if (!sz || *sz == 0) {
                goto usage;
            }
            this->stride = *sz;
            break;
        }
        case 'l':
            this->read_lines = true;
            break;
        case 'F':
            this->flush = true;
            break;
        case 'X':
            this->exponential = true;
            break;
        case 'y':
            ++this->yield;
            break;
        case 'H':
            this->hint = true;
            break;
        case 'K':
            this->nonblocking = true;
            break;
        case 'q':
            this->quiet = true;
            break;
        case 'i':
            this->input_files.push_back(optarg);
            break;
        case 'o':
            this->output_files.push_back(optarg);
            break;
        case 'p':
            if (auto sz = parse_size(optarg)) {
                this->initial_offset = *sz;
            } else {
                goto usage;
            }
            break;
        case 'r':
            if (auto sz = parse_size(optarg)) {
                this->engine.seed(*sz);
            } else {
                goto usage;
            }
            break;
        case 'D':
            this->delay = strtod(optarg, &endptr);
            if (endptr == optarg || *endptr) {
                goto usage;
            }
            break;
        case 'a':
            alarm_interval = strtod(optarg, &endptr);
            if (endptr == optarg || *endptr) {
                goto usage;
            }
            break;
        case 'P':
            if (auto sz = parse_size(optarg)) {
                this->pipebuf_size = *sz;
            } else {
                goto usage;
            }
            break;
        case 'A':
            if (auto sz = parse_size(optarg)) {
                this->as_limit = *sz;
            } else {
                goto usage;
            }
            break;
        case '#':
        default:
            goto usage;
        }
    }


    for (int i = optind; i < argc; ++i) {
        this->input_files.push_back(argv[i]);
    }
    if (this->input_files.empty()) {
        this->input_files.push_back(nullptr);
    } else if (this->input_files.size() == 1) {
        this->input_file = this->input_files[0];
    } else if (!strchr(this->opts, '#')) {
        goto usage;
    }

    if (this->output_files.empty()) {
        this->output_files.push_back(nullptr);
    } else if (this->output_files.size() == 1) {
        this->output_file = this->output_files[0];
    } else if (!strstr(this->opts, "##")) {
        goto usage;
    }

    this->block_size = bs;
    this->max_block_size = std::max(bs, max_bs);

    if (alarm_interval > 0) {
        struct sigaction act;
        act.sa_handler = sigalrm_handler;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        int r = sigaction(SIGALRM, &act, nullptr);
        assert(r == 0);

        double sec = floor(alarm_interval);
        timeval tv = { (int) sec, (int) ((alarm_interval - sec) * 1e6) };
        itimerval timer = { tv, tv };
        r = setitimer(ITIMER_REAL, &timer, nullptr);
        assert(r == 0);
    }

    if (this->as_limit > 0) {
#if defined(RLIMIT_AS) && !__MACH__
        struct rlimit rlim;
        rlim.rlim_cur = rlim.rlim_max = this->as_limit;
        int r = setrlimit(RLIMIT_AS, &rlim);
        if (r != 0) {
            fprintf(stderr, "\n*** MEMORY LIMIT IGNORED *** %s\n\n* Run this test in Docker or on the grading server.\n\n", strerror(errno));
        }
#else
        fprintf(stderr, "\n*** MEMORY LIMIT IGNORED ***\n\n* Run this test in Docker or on the grading server.\n\n");
#endif
    }

    return *this;

 usage:
    this->usage();
    exit(1);
}

void io61_args::usage() {
    fprintf(stderr, "Usage: %s [OPTIONS] [FILE]%s\nOptions:\n",
            this->program_name, strchr(this->opts, '#') ? "..." : "");
    if (strchr(this->opts, 'i')) {
        fprintf(stderr, "    -i FILE       Read input from FILE\n");
    }
    if (strchr(this->opts, 'o')) {
        fprintf(stderr, "    -o FILE       Write output to FILE\n");
    }
    if (strchr(this->opts, 'q')) {
        fprintf(stderr, "    -q            Ignore errors\n");
    }
    if (strchr(this->opts, 's')) {
        fprintf(stderr, "    -s SIZE       Set size written\n");
    }
    if (strchr(this->opts, 'b')) {
        if (this->block_size) {
            fprintf(stderr, "    -b BLOCKSIZE  Set block size (default %zu)\n", this->block_size);
        } else {
            fprintf(stderr, "    -b BLOCKSIZE  Set block size\n");
        }
    }
    if (strchr(this->opts, 'B')) {
        if (this->max_block_size) {
            fprintf(stderr, "    -B BLOCKSIZE  Set max block size (default %zu)\n", this->max_block_size);
        } else {
            fprintf(stderr, "    -B BLOCKSIZE  Set max block size\n");
        }
    }
    if (strchr(this->opts, 't')) {
        fprintf(stderr, "    -t STRIDE     Set stride (default %zu)\n", this->stride);
    }
    if (strchr(this->opts, 'p')) {
        fprintf(stderr, "    -p POS        Set initial file position\n");
    }
    if (strchr(this->opts, 'l')) {
        fprintf(stderr, "    -l            Read by lines\n");
    }
    if (strchr(this->opts, 'R')) {
        fprintf(stderr, "    -R            Read bytewise, not blocks\n");
    }
    if (strchr(this->opts, 'W')) {
        fprintf(stderr, "    -W            Write bytewise, not blocks\n");
    }
    if (strchr(this->opts, 'F')) {
        fprintf(stderr, "    -F            Flush after each write\n");
    }
    if (strchr(this->opts, 'y')) {
        fprintf(stderr, "    -y            Yield after each write\n");
    }
    if (strchr(this->opts, 'H')) {
        fprintf(stderr, "    -H            Supply hints to library\n");
    }
    if (strchr(this->opts, 'X')) {
        fprintf(stderr, "    -X            Use powers of two for block sizes\n");
    }
    if (strchr(this->opts, 'P')) {
        fprintf(stderr, "    -P BUFSIZ     Set input pipe buffer size on Linux\n");
    }
    if (strchr(this->opts, 'A')) {
        fprintf(stderr, "    -A ASLIMIT    Set address space limit on Linux\n");
    }
    if (strchr(this->opts, 'r')) {
        fprintf(stderr, "    -r            Set random seed (default %u)\n", this->seed);
    }
    if (strchr(this->opts, 'D')) {
        fprintf(stderr, "    -D DELAY      Delay before starting\n");
    }
    if (strchr(this->opts, 'a')) {
        fprintf(stderr, "    -a TIME       Set interval timer\n");
    }
}

void io61_args::after_open() {
    if (this->delay > 0) {
        double now = monotonic_timestamp();
        double end = now + this->delay;
        while (now < end) {
            usleep((unsigned) ((end - now) * 1e6));
            now = monotonic_timestamp();
        }
        this->delay = 0;
    }
}

void io61_args::after_open(int fd, int mode) {
    (void) mode;
#ifdef F_SETPIPE_SZ
    if (this->pipebuf_size > 0) {
        int r = fcntl(fd, F_SETPIPE_SZ, this->pipebuf_size);
        (void) r;
    }
#endif
    if (this->nonblocking) {
        int r = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
        (void) r;
    }
    this->after_open();
}

void io61_args::after_open(io61_file* f, int mode) {
    this->after_open(io61_fileno(f), mode);
}

void io61_args::after_open(FILE* f, int mode) {
    this->after_open(fileno(f), mode);
}

void io61_args::after_write(int fd) {
    (void) fd;
    if (this->yield > 0) {
        usleep(this->yield);
    }
}

void io61_args::after_write(io61_file* f) {
    if (this->flush) {
        int r = io61_flush(f);
        assert(r == 0);
    }
    if (this->yield > 0) {
        usleep(this->yield);
    }
}

void io61_args::after_write(FILE* f) {
    if (this->flush) {
        int r = fflush(f);
        assert(r == 0);
    }
    if (this->yield > 0) {
        usleep(this->yield);
    }
}


namespace {

struct io61_profiler {
    double begin_at;
    io61_profiler();
    ~io61_profiler();
};

static io61_profiler profiler_instance;

io61_profiler::io61_profiler() {
    this->begin_at = monotonic_timestamp();
}

io61_profiler::~io61_profiler() {
    // Measure elapsed real, user, and system times, and report the result
    // as JSON to file descriptor 100 if it’s available.

    double real_elapsed = monotonic_timestamp() - this->begin_at;

    struct rusage usage;
    int r = getrusage(RUSAGE_SELF, &usage);
    assert(r == 0);

    struct rusage cusage;
    r = getrusage(RUSAGE_CHILDREN, &cusage);
    assert(r == 0);
    timeradd(&usage.ru_utime, &cusage.ru_utime, &usage.ru_utime);
    timeradd(&usage.ru_stime, &cusage.ru_stime, &usage.ru_stime);

    long maxrss = usage.ru_maxrss + cusage.ru_maxrss;
#if __MACH__
    maxrss = (maxrss + 1023) / 1024;
#endif

    char buf[2000];
    ssize_t len = snprintf(buf, sizeof(buf),
        "{\"time\":%.6f, \"utime\":%ld.%06ld, \"stime\":%ld.%06ld, \"maxrss\":%ld, \"minflt\":%ld, \"majflt\":%ld, \"inblock\":%ld, \"oublock\":%ld}\n",
        real_elapsed,
        usage.ru_utime.tv_sec, (long) usage.ru_utime.tv_usec,
        usage.ru_stime.tv_sec, (long) usage.ru_stime.tv_usec,
        maxrss,
        usage.ru_minflt + cusage.ru_minflt,
        usage.ru_majflt + cusage.ru_majflt,
        usage.ru_inblock + cusage.ru_inblock,
        usage.ru_oublock + cusage.ru_oublock);

    off_t off = lseek(100, 0, SEEK_CUR);
    int fd = (off != (off_t) -1 || errno == ESPIPE ? 100 : STDERR_FILENO);
    if (fd == STDERR_FILENO && !getenv("TIMING")) {
        return;
    } else if (fd == STDERR_FILENO) {
        fflush(stderr);
    }
    while (true) {
        ssize_t nw = write(fd, buf, len);
        if (nw == len) {
            break;
        }
        assert(nw == -1 && (errno == EINTR || errno == EAGAIN));
    }
}

}
