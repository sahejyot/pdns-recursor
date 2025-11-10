/*
 * Windows compatibility layer for POSIX regex functions
 * Maps POSIX regex API to std::regex for Windows compatibility
 */

#pragma once

#ifdef _WIN32

#include <regex>
#include <string>
#include <memory>

// Forward declarations
typedef long regoff_t;

typedef struct {
    regoff_t rm_so;  // start offset
    regoff_t rm_eo;  // end offset
} regmatch_t;

// POSIX regex types
typedef struct {
    std::unique_ptr<std::regex> regex_ptr;
    std::string pattern;
    int flags;
} regex_t;

// POSIX regex flags
#define REG_EXTENDED 0x01
#define REG_ICASE    0x02
#define REG_NOSUB    0x04
#define REG_NEWLINE  0x08

// POSIX regex error codes
#define REG_NOMATCH  1
#define REG_BADPAT   2
#define REG_ECOLLATE 3
#define REG_ECTYPE   4
#define REG_EESCAPE  5
#define REG_ESUBREG  6
#define REG_EBRACK   7
#define REG_EPAREN   8
#define REG_EBRACE   9
#define REG_BADBR    10
#define REG_ERANGE   11
#define REG_ESPACE   12
#define REG_BADRPT   13
#define REG_EEND     14
#define REG_ESIZE    15
#define REG_ERPAREN  16

// POSIX regex functions
extern "C" {
    int regcomp(regex_t* preg, const char* pattern, int cflags);
    int regexec(const regex_t* preg, const char* string, size_t nmatch, 
                regmatch_t* pmatch, int eflags);
    void regfree(regex_t* preg);
}

#else
// On POSIX systems, use the standard regex.h
#include <regex.h>
#endif
