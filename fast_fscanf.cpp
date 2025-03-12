// fast_fscanf.cpp
#include <cstdio>
#include <cstdarg>
#include <charconv>   // for std::from_chars on integrals (C++17) & float/double (C++20)
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <cctype>

/**
 * A memory-based "fast_fscanf" that reads from a (char* buffer, size_t size)
 * instead of FILE*. It tries to mimic scanf's parsing:
 *
 *   %hd, %hu, %d, %u, %ld, %lu, %x, %hx, %lx
 *   %f, %lf, %Lf
 *   %c, %s
 *
 * Limitations:
 *  - No field width (e.g. "%3d") except for strings (%s) – see code below.
 *  - No assignment-suppression (e.g. "%*d").
 *  - No octal parsing (%o).
 *  - We handle literal punctuation vs numeric token boundaries in a single pass,
 *    so e.g. ":%x[%hd](" should parse as intended without extra spaces.
 */

/**
 * fast_fscanf_mem:
 *   - buffer, size: the memory block holding the entire text file
 *   - offset: [in/out] current parsing index within that buffer
 *   - format: the scanf-like format string
 *   - ...: pointers to the variables to fill
 *
 * Returns the number of successfully matched fields, or 0 if it fails
 * immediately, or partial count on partial failure.
 *
 * Usage example:
 *
 *   size_t offset = 0;
 *   while (fast_fscanf_mem(buffer, bufsize, &offset,
 *          ":%lx[%hd]( %hd %hu %d %hx %lx %f %Lf %63s %hd/%hd/%hd %hd:%hd:%hd\n",
 *          &rec.pn_prog, &rec.pn_n, ...) == 16)
 *   {
 *       // got one record
 *   }
 */

// -------------------------------------------------------------------------
// INTERNALS BELOW
// -------------------------------------------------------------------------

// We'll use a lightweight "scanner" struct to walk the buffer by pointer:
struct MemScanner {
    const char *ptr;   // current position
    const char *end;   // one-past the last valid character
};

/** Returns true if we're out of data. */
static inline bool ms_eof(const MemScanner &ms)
{
    return (ms.ptr >= ms.end);
}

/** Peek a character (0 if EOF). */
static inline char ms_peek(const MemScanner &ms)
{
    return ms_eof(ms) ? '\0' : *ms.ptr;
}

/** Advance one char; return it, or 0 if EOF. */
static inline char ms_getc(MemScanner &ms)
{
    if (ms_eof(ms)) return '\0';
    return *ms.ptr++;
}

/** "Ungetc" by moving back 1 char (if not at start). */
static inline void ms_ungetc(MemScanner &ms)
{
    // We'll just move ptr back by 1 if possible
    if (ms.ptr > ms.end) {
        // If we were out of range, clamp
        ms.ptr = ms.end;
    }
    if (ms.ptr > (ms.end - (ms.end - ms.ptr)) ) {
        // basically do: if (we're not at the beginning) go back
        ms.ptr--;
    }
}

/** Skip whitespace. */
static void ms_skip_whitespace(MemScanner &ms)
{
    while (!ms_eof(ms) && isspace((unsigned char)*ms.ptr)) {
        ms.ptr++;
    }
}

// -------------------------------------------------------------------------
// readChar: for '%c' – read exactly 1 char, even if it's whitespace.
// -------------------------------------------------------------------------
static bool readChar(MemScanner &ms, char &out)
{
    if (ms_eof(ms)) {
        return false;
    }
    out = ms_getc(ms);
    return true;
}

// -------------------------------------------------------------------------
// readString: for '%s' – skip leading whitespace, then read until next space
// or punctuation. Actually, real scanf stops at whitespace for "%s". We'll do
// the same. If you want punctuation-based stop for strings, you'd do something else.
//
// width > 0 => maximum length (minus 1 for null terminator).
// Returns true if we got at least 1 char.
// -------------------------------------------------------------------------
static bool readString(MemScanner &ms, char *dest, int width)
{
    if (!dest) return false;
    if (width <= 0) {
        // treat 0 or negative as "some big limit"
        width = 1024 * 1024;
    }

    ms_skip_whitespace(ms);

    int count = 0;
    while (!ms_eof(ms)) {
        char c = ms_peek(ms);
        if (isspace((unsigned char)c)) {
            // stop
            break;
        }
        // read it
        ms_getc(ms);
        if (count < (width - 1)) {
            dest[count++] = c;
            dest[count] = '\0';
        }
    }
    return (count > 0);
}

// -------------------------------------------------------------------------
// readIntegerToken: gather sign if base=10, gather digits for base 10 or 16,
// stop at first non-digit. Then ungetc that char. Return false if no digit.
// -------------------------------------------------------------------------
static bool readIntegerToken(MemScanner &ms, bool allowSign, int base,
                             char *outBuf, size_t bufSize)
{
    if (!outBuf || bufSize < 2) return false;
    ms_skip_whitespace(ms);

    size_t pos = 0;
    outBuf[0] = '\0';

    // optional sign?
    if (allowSign) {
        char c = ms_peek(ms);
        if (c == '+' || c == '-') {
            ms_getc(ms); // consume
            outBuf[pos++] = c;
            outBuf[pos]   = '\0';
        }
    }

    bool gotDigit = false;
    while (!ms_eof(ms)) {
        char c = ms_peek(ms);

        bool valid = false;
        if (base == 10) {
            valid = isdigit((unsigned char)c);
        } else if (base == 16) {
            valid = isxdigit((unsigned char)c);
        }

        if (!valid) {
            break;
        }
        // consume it
        ms_getc(ms);
        if (pos < bufSize - 1) {
            outBuf[pos++] = c;
            outBuf[pos]   = '\0';
        }
        gotDigit = true;
    }
    return gotDigit;
}

// -------------------------------------------------------------------------
// readFloatToken: gather sign, digits, one decimal point, exponent, etc.
// Stop at first character that isn't valid in a float (like punctuation).
// Then ungetc if needed. Return false if we didn't read anything numeric.
// -------------------------------------------------------------------------
static bool readFloatToken(MemScanner &ms, char *outBuf, size_t bufSize)
{
    if (!outBuf || bufSize < 2) return false;
    ms_skip_whitespace(ms);

    // We'll do a naive approach: accept sign, digits, a single '.', 'e/E' and
    // optional exponent sign. This won't be bulletproof (e.g. multiple '.'?), but
    // good enough for most input that is well-formed like "123.456e-2".
    size_t pos = 0;
    outBuf[0]  = '\0';
    bool gotAny = false;
    bool seenExponent = false;
    bool seenDot = false;

    // optional leading sign
    char c = ms_peek(ms);
    if (c == '+' || c == '-') {
        ms_getc(ms);
        outBuf[pos++] = c;
        outBuf[pos]   = '\0';
        gotAny = true;
    }

    while (!ms_eof(ms)) {
        c = ms_peek(ms);

        bool valid = false;

        if (isdigit((unsigned char)c)) {
            valid = true;
            gotAny = true;
        } else if (!seenDot && c == '.') {
            seenDot = true;
            valid   = true;
            gotAny  = true;
        } else if (!seenExponent && (c == 'e' || c == 'E')) {
            seenExponent = true;
            valid        = true;
            gotAny       = true;
        } else if ((c == '+' || c == '-') && seenExponent) {
            // sign after 'e' or 'E' is allowed if it's the immediate next char
            // so let's check previous char in outBuf if we can
            if (pos > 0 && (outBuf[pos-1] == 'e' || outBuf[pos-1] == 'E')) {
                valid  = true;
                gotAny = true;
            }
        }

        if (!valid) {
            break;
        }
        // consume it
        ms_getc(ms);
        if (pos < bufSize - 1) {
            outBuf[pos++] = c;
            outBuf[pos]   = '\0';
        }
    }

    return gotAny;
}

// -------------------------------------------------------------------------
// The core function: parse according to a simplified subset of scanf format.
// -------------------------------------------------------------------------
extern "C"
int fast_fscanf_mem(
    const char *buffer, size_t size,
    size_t *offset,
    const char *format, ...
) {
    // Build a scanner that starts at buffer + (*offset)
    MemScanner ms;
    ms.ptr = buffer + *offset;
    ms.end = buffer + size;

    va_list args;
    va_start(args, format);

    int matchedCount = 0;

    while (*format) {
        if (*format == '%') {
            // we have a conversion specifier
            format++;

            // skip optional '.' or digits in the format (like "%.2f" or "%3d")
            while (*format == '.' || isdigit((unsigned char)*format)) {
                format++;
            }

            // check length modifiers
            bool isShort      = false;  // 'h'
            bool isLong       = false;  // 'l'
            bool isLongDouble = false;  // 'L'

            if (*format == 'h') {
                isShort = true;
                format++;
            } else if (*format == 'l') {
                isLong = true;
                format++;
            } else if (*format == 'L') {
                isLongDouble = true;
                format++;
            }

            // Possibly read a width for "%s" (like "%63s")
            int strWidth = 0;
            if (isdigit((unsigned char)*format)) {
                // gather the digits
                char widthStr[16] = {0};
                int wi = 0;
                while (isdigit((unsigned char)*format) && wi < 15) {
                    widthStr[wi++] = *format++;
                }
                widthStr[wi] = '\0';
                strWidth = atoi(widthStr);
            }

            char spec = *format;
            if (spec == '\0') {
                // format ended abruptly
                break;
            }
            format++;

            bool success = false;

            switch (spec)
            {
            case 'd': {
                // decimal integer
                char tok[128] = {0};
                if (readIntegerToken(ms, true /*allowSign*/, 10, tok, sizeof(tok))) {
                    // parse with from_chars or strtol
                    if (isShort) {
                        short *p = va_arg(args, short*);
                        long val = 0;
                        auto r = std::from_chars(tok, tok + std::strlen(tok), val, 10);
                        if (r.ec == std::errc()) {
                            *p = (short)val;
                            success = true;
                        }
                    } else if (isLong) {
                        long *p = va_arg(args, long*);
                        auto r = std::from_chars(tok, tok + std::strlen(tok), *p, 10);
                        if (r.ec == std::errc()) {
                            success = true;
                        }
                    } else {
                        int *p = va_arg(args, int*);
                        long val = 0;
                        auto r = std::from_chars(tok, tok + std::strlen(tok), val, 10);
                        if (r.ec == std::errc()) {
                            *p = (int)val;
                            success = true;
                        }
                    }
                }
            } break;

            case 'u': {
                // unsigned decimal
                char tok[128] = {0};
                if (readIntegerToken(ms, false/*no sign*/, 10, tok, sizeof(tok))) {
                    if (isShort) {
                        unsigned short *p = va_arg(args, unsigned short*);
                        unsigned long val = 0;
                        auto r = std::from_chars(tok, tok + std::strlen(tok), val, 10);
                        if (r.ec == std::errc()) {
                            *p = (unsigned short)val;
                            success = true;
                        }
                    } else if (isLong) {
                        unsigned long *p = va_arg(args, unsigned long*);
                        auto r = std::from_chars(tok, tok + std::strlen(tok), *p, 10);
                        if (r.ec == std::errc()) {
                            success = true;
                        }
                    } else {
                        unsigned int *p = va_arg(args, unsigned int*);
                        unsigned long val = 0;
                        auto r = std::from_chars(tok, tok + std::strlen(tok), val, 10);
                        if (r.ec == std::errc()) {
                            *p = (unsigned int)val;
                            success = true;
                        }
                    }
                }
            } break;

            case 'x': {
                // hex integer
                char tok[128] = {0};
                if (readIntegerToken(ms, false/*no sign*/, 16, tok, sizeof(tok))) {
                    if (isShort) {
                        unsigned short *p = va_arg(args, unsigned short*);
                        unsigned long val = 0;
                        auto r = std::from_chars(tok, tok + std::strlen(tok), val, 16);
                        if (r.ec == std::errc()) {
                            *p = (unsigned short)val;
                            success = true;
                        }
                    } else if (isLong) {
                        unsigned long *p = va_arg(args, unsigned long*);
                        auto r = std::from_chars(tok, tok + std::strlen(tok), *p, 16);
                        if (r.ec == std::errc()) {
                            success = true;
                        }
                    } else {
                        unsigned int *p = va_arg(args, unsigned int*);
                        unsigned long val = 0;
                        auto r = std::from_chars(tok, tok + std::strlen(tok), val, 16);
                        if (r.ec == std::errc()) {
                            *p = (unsigned int)val;
                            success = true;
                        }
                    }
                }
            } break;

            case 'f':
            case 'g':
            case 'e': {
                // float / double parse
                char tok[256] = {0};
                if (readFloatToken(ms, tok, sizeof(tok))) {
                    if (isLongDouble) {
                        long double *p = va_arg(args, long double*);
                        // from_chars for long double isn't fully standard yet, fallback:
                        char *endp = nullptr;
                        long double val = strtold(tok, &endp);
                        if (endp != tok) {
                            *p = val;
                            success = true;
                        }
                    } else if (isLong) {
                        double *p = va_arg(args, double*);
                        // some C++ libs do partial from_chars for double:
                        double tmp;
                        auto r = std::from_chars(tok, tok + std::strlen(tok),
                                                 tmp, std::chars_format::general);
                        if (r.ec == std::errc()) {
                            *p = tmp;
                            success = true;
                        } else {
                            // fallback
                            char *endp = nullptr;
                            double val = strtod(tok, &endp);
                            if (endp != tok) {
                                *p = val;
                                success = true;
                            }
                        }
                    } else {
                        float *p = va_arg(args, float*);
                        // parse as double then cast
                        double tmp;
                        auto r = std::from_chars(tok, tok + std::strlen(tok),
                                                 tmp, std::chars_format::general);
                        if (r.ec == std::errc()) {
                            *p = (float)tmp;
                            success = true;
                        } else {
                            // fallback
                            char *endp = nullptr;
                            double val = strtod(tok, &endp);
                            if (endp != tok) {
                                *p = (float)val;
                                success = true;
                            }
                        }
                    }
                }
            } break;

            case 'c': {
                // read exactly one char
                char *p = va_arg(args, char*);
                if (readChar(ms, *p)) {
                    success = true;
                }
            } break;

            case 's': {
                // read a string up to whitespace
                char *p = va_arg(args, char*);
                if (readString(ms, p, (strWidth > 0 ? strWidth : 1024))) {
                    success = true;
                }
            } break;

            default:
                // unsupported -> do nothing
                break;
            }

            if (success) {
                matchedCount++;
            } else {
                // partial or no match, so stop
                break;
            }
        }
        else if (isspace((unsigned char)*format)) {
            // skip whitespace in format
            ms_skip_whitespace(ms);
            format++;
        }
        else if (*format == '\n') {
            // match a newline in the format
            format++;
            // let's skip any trailing spaces until newline in the input
            // or treat it strictly? We'll do a flexible approach:
            while (!ms_eof(ms)) {
                char c = ms_getc(ms);
                if (c == '\n') {
                    break;
                }
                if (!isspace((unsigned char)c)) {
                    // mismatch
                    ms_ungetc(ms);
                    break;
                }
            }
        }
        else {
            // literal character
            // skip whitespace in the input before matching a literal
            ms_skip_whitespace(ms);

            char expected = *format++;
            // read one char from input
            if (ms_eof(ms)) {
                // can't match
                break;
            }
            char c = ms_getc(ms);
            if (c != expected) {
                // mismatch
                ms_ungetc(ms);
                // stop
                break;
            }
        }
    }

    // update *offset to the new position
    *offset = (size_t)(ms.ptr - buffer);

    va_end(args);
    return matchedCount;
}