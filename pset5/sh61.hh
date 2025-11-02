#ifndef SH61_HH
#define SH61_HH
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <csignal>
#include <string>
#include <fcntl.h>
#include <unistd.h>

#define TYPE_NORMAL        0   // normal command word
#define TYPE_REDIRECT_OP   1   // redirection operator (>, <, 2>)

// All other tokens are control operators that terminate the current command.
#define TYPE_SEQUENCE      2   // `;` sequence operator
#define TYPE_EOL           3   // end of command line
#define TYPE_BACKGROUND    4   // `&` background operator
#define TYPE_PIPE          5   // `|` pipe operator
#define TYPE_AND           6   // `&&` operator
#define TYPE_OR            7   // `||` operator

// If you want to handle an extended shell syntax for extra credit, here are
// some other token types to get you started.
#define TYPE_LPAREN        8   // `(` operator
#define TYPE_RPAREN        9   // `)` operator
#define TYPE_OTHER         -1

struct shell_tokenizer;
struct command_line_parser;
struct conditional_parser;
struct pipeline_parser;
struct command_parser;


// shell_parser
//    A `shell_parser` object navigates a command line according to the
//    shell grammar. Each `shell_parser` examines a region (substring) of
//    a command line. There are `shell_parser` subtypes specialized for
//    conditionals, pipelines, and commands; use `++parser` to advance a
//    parser to the next conditional, pipeline, or command, depending on.
//    parser type. Functions like `conditional_begin()` return a
//    sub-parser for conditionals within the current region.

struct shell_parser {
    explicit shell_parser(const char* str);
    shell_parser(const char* first, const char* last);

    // Test if the current region is empty
    explicit inline constexpr operator bool() const;
    inline constexpr bool empty() const;

    // Return the contents of the region as a string, for debugging
    inline std::string str() const;

    inline constexpr bool operator==(const shell_parser&) const;
    inline constexpr bool operator!=(const shell_parser&) const;
    inline constexpr bool operator==(const shell_tokenizer&) const;
    inline constexpr bool operator!=(const shell_tokenizer&) const;

    // Return the operator token type immediately following the current region
    int next_op() const;
    const char* next_op_name() const;

    // Return `shell_tokenizer` that navigates by tokens
    inline shell_tokenizer token_begin() const;

    // Return a parser/tokenizer representing the end of the region
    inline shell_parser end() const;
    inline shell_tokenizer token_end() const;

protected:
    const char* _s;
    const char* _stop;
    const char* _end;

    shell_parser first_delimited(unsigned long fl) const;
    void next_delimited(unsigned long fl);
    shell_parser(const char* first, const char* stop, const char* last);

    friend struct shell_tokenizer;
};

struct command_line_parser : public shell_parser {
    explicit inline command_line_parser(const char* str);
    inline command_line_parser(const char* first, const char* last);
    inline command_line_parser(const shell_parser&);

    conditional_parser conditional_begin() const;
    pipeline_parser pipeline_begin() const;
    command_parser command_begin() const;
};

struct conditional_parser : public shell_parser {
    explicit inline conditional_parser(const char* str);
    inline conditional_parser(const char* first, const char* last);
    inline conditional_parser(const shell_parser&);

    pipeline_parser pipeline_begin() const;
    command_parser command_begin() const;

    void operator++();
};

struct pipeline_parser : public shell_parser {
    explicit inline pipeline_parser(const char* str);
    inline pipeline_parser(const char* first, const char* last);
    inline pipeline_parser(const shell_parser&);

    command_parser command_begin() const;

    void operator++();
};

struct command_parser : public shell_parser {
    explicit inline command_parser(const char* str);
    inline command_parser(const char* first, const char* last);
    inline command_parser(const shell_parser&);

    void operator++();
};


// shell_tokenizer
//    A `shell_tokenizer` object breaks down a command line into tokens.

struct shell_tokenizer {
    explicit shell_tokenizer(const char* str);
    shell_tokenizer(const char* first, const char* last);

    // Test if there are more tokens
    inline constexpr explicit operator bool() const;
    inline constexpr bool empty() const;

    // Return the current token’s contents as a string
    std::string str() const;

    inline constexpr bool operator==(const shell_tokenizer&) const;
    inline constexpr bool operator!=(const shell_tokenizer&) const;
    inline constexpr bool operator==(const shell_parser&) const;
    inline constexpr bool operator!=(const shell_parser&) const;

    // Return the current token’s type
    inline constexpr int type() const;
    const char* type_name() const;

    // Advance to the next token, if any
    void operator++();

private:
    const char* _s;
    const char* _end;
    short _type;
    bool _quoted;
    unsigned _len;

    friend struct shell_parser;
};


// claim_foreground(pgid)
//    Mark `pgid` as the current foreground process group.

int claim_foreground(pid_t pgid);


// set_signal_handler(signo, handler)
//    Install handler `handler` for signal `signo`. `handler` can be SIG_DFL
//    to install the default handler, or SIG_IGN to ignore the signal. Return
//    0 on success, -1 on failure. See `man 2 sigaction` or `man 3 signal`.

inline int set_signal_handler(int signo, void (*handler)(int)) {
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    return sigaction(signo, &sa, NULL);
}


inline constexpr shell_parser::operator bool() const {
    return _s != _stop;
}

inline constexpr bool shell_parser::empty() const {
    return _s == _stop;
}

inline std::string shell_parser::str() const {
    return std::string(_s, _stop - _s);
}

inline constexpr bool shell_parser::operator==(const shell_parser& p) const {
    return _s == p._s && _stop == p._stop;
}

inline constexpr bool shell_parser::operator!=(const shell_parser& p) const {
    return _s != p._s || _stop != p._stop;
}

inline constexpr bool shell_parser::operator==(const shell_tokenizer& t) const {
    return _s == t._s && _stop == t._end;
}

inline constexpr bool shell_parser::operator!=(const shell_tokenizer& t) const {
    return _s != t._s || _stop != t._end;
}

inline shell_parser shell_parser::end() const {
    return shell_parser(_stop, _stop, _end);
}

inline shell_tokenizer shell_parser::token_begin() const {
    return shell_tokenizer(_s, _stop);
}

inline shell_tokenizer shell_parser::token_end() const {
    return shell_tokenizer(_stop, _stop);
}

inline constexpr shell_tokenizer::operator bool() const {
    return _s != _end;
}

inline constexpr bool shell_tokenizer::empty() const {
    return _s == _end;
}

inline constexpr int shell_tokenizer::type() const {
    return _type;
}

inline constexpr bool shell_tokenizer::operator==(const shell_tokenizer& t) const {
    return _s == t._s && _end == t._end;
}

inline constexpr bool shell_tokenizer::operator!=(const shell_tokenizer& t) const {
    return _s != t._s || _end != t._end;
}

inline constexpr bool shell_tokenizer::operator==(const shell_parser& p) const {
    return _s == p._s && _end == p._stop;
}

inline constexpr bool shell_tokenizer::operator!=(const shell_parser& p) const {
    return _s != p._s || _end != p._stop;
}

inline command_line_parser::command_line_parser(const char* str)
    : shell_parser(str) {
}

inline command_line_parser::command_line_parser(const char* first, const char* last)
    : shell_parser(first, last) {
}

inline command_line_parser::command_line_parser(const shell_parser& sp)
    : shell_parser(sp) {
}

inline conditional_parser::conditional_parser(const char* str)
    : shell_parser(str) {
}

inline conditional_parser::conditional_parser(const char* first, const char* last)
    : shell_parser(first, last) {
}

inline conditional_parser::conditional_parser(const shell_parser& sp)
    : shell_parser(sp) {
}

inline pipeline_parser::pipeline_parser(const char* str)
    : shell_parser(str) {
}

inline pipeline_parser::pipeline_parser(const char* first, const char* last)
    : shell_parser(first, last) {
}

inline pipeline_parser::pipeline_parser(const shell_parser& sp)
    : shell_parser(sp) {
}

inline command_parser::command_parser(const char* str)
    : shell_parser(str) {
}

inline command_parser::command_parser(const char* first, const char* last)
    : shell_parser(first, last) {
}

inline command_parser::command_parser(const shell_parser& sp)
    : shell_parser(sp) {
}

#endif
