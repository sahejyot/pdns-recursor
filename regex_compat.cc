/*
 * Windows compatibility layer for POSIX regex functions
 * Implementation maps POSIX regex API to std::regex
 */

#ifdef _WIN32

#include "regex_compat.hh"
#include <stdexcept>
#include <iostream>

extern "C" {

int regcomp(regex_t* preg, const char* pattern, int cflags) {
    if (!preg || !pattern) {
        return REG_BADPAT;
    }
    
    try {
        preg->pattern = pattern;
        preg->flags = cflags;
        
        // Convert POSIX flags to std::regex flags
        std::regex_constants::syntax_option_type std_flags = std::regex_constants::ECMAScript;
        
        if (cflags & REG_EXTENDED) {
            std_flags |= std::regex_constants::extended;
        }
        if (cflags & REG_ICASE) {
            std_flags |= std::regex_constants::icase;
        }
        
        preg->regex_ptr = std::make_unique<std::regex>(pattern, std_flags);
        return 0;
    }
    catch (const std::regex_error& e) {
        // Map std::regex errors to POSIX error codes
        switch (e.code()) {
            case std::regex_constants::error_collate:
                return REG_ECOLLATE;
            case std::regex_constants::error_ctype:
                return REG_ECTYPE;
            case std::regex_constants::error_escape:
                return REG_EESCAPE;
            case std::regex_constants::error_backref:
                return REG_ESUBREG;
            case std::regex_constants::error_brack:
                return REG_EBRACK;
            case std::regex_constants::error_paren:
                return REG_EPAREN;
            case std::regex_constants::error_brace:
                return REG_EBRACE;
            case std::regex_constants::error_badrepeat:
                return REG_BADRPT;
            case std::regex_constants::error_range:
                return REG_ERANGE;
            case std::regex_constants::error_space:
                return REG_ESPACE;
            case std::regex_constants::error_badbrace:
                return REG_EBRACE;
            default:
                return REG_BADPAT;
        }
    }
    catch (...) {
        return REG_ESPACE;
    }
}

int regexec(const regex_t* preg, const char* string, size_t nmatch, 
            regmatch_t* pmatch, int eflags) {
    if (!preg || !string || !preg->regex_ptr) {
        return REG_BADPAT;
    }
    
    try {
        std::string str(string);
        std::smatch matches;
        
        if (std::regex_search(str, matches, *preg->regex_ptr)) {
            // Fill pmatch array if provided
            if (pmatch && nmatch > 0) {
                size_t max_matches = std::min(nmatch, matches.size());
                for (size_t i = 0; i < max_matches; ++i) {
                    pmatch[i].rm_so = matches.position(i);
                    pmatch[i].rm_eo = matches.position(i) + matches.length(i);
                }
            }
            return 0; // Match found
        } else {
            return REG_NOMATCH; // No match
        }
    }
    catch (...) {
        return REG_ESPACE;
    }
}

void regfree(regex_t* preg) {
    if (preg) {
        preg->regex_ptr.reset();
        preg->pattern.clear();
        preg->flags = 0;
    }
}

} // extern "C"

#endif // _WIN32
