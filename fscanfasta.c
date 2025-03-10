#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include <errno.h>

/* Boolean type for better readability */
typedef int BOOL;
#define TRUE 1
#define FALSE 0

/* I/O structure that handles both file and memory-based operations */
typedef struct {
    FILE *fp;          /* File pointer (file mode) */
    char *buffer;      /* Memory buffer (memory mode) */
    char *ptr;         /* Current position in buffer */
    char *end;         /* End of buffer */
    size_t size;       /* Buffer size */
    BOOL useFile;      /* Mode flag (TRUE = file, FALSE = memory) */
} MyIO;

/* Date structure (g=day, m=month, a=year) */
struct data {
    char g;
    char m;
    short a;
};

/* Time structure (o=hour, m=minute, s=second) */
struct ora {
    char o;
    char m;
    char s;
};

/* Test record structure with various field types */
typedef struct {
    unsigned long pn_prog;      /* Progressive number */
    short pn_n;                 /* Secondary identifier */
    short field_short;          
    unsigned short field_ushort;
    int field_int;              
    unsigned short field_hexushort; /* Hex format */
    unsigned long field_hexulong;   /* Hex format */
    float field_float;          
    long double field_ldouble;  
    char token[64];             /* String token */
    short day, month, year;     /* Date components */
    short hour, minute, second; /* Time components */
} Record;

/* ============== I/O basic functions ============== */

/* Loads entire file into memory buffer
   Returns TRUE on success, FALSE on failure */
BOOL loadFileIntoBuffer(FILE *fp, const char *filename, MyIO *io, BOOL loadBuffer) {
    if (!loadBuffer) return FALSE;
    if (!filename) return FALSE;
    fp = fopen(filename, "rb");
    if (!fp) return FALSE;
    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    io->buffer = (char*)malloc(fileSize + 1);
    if (!io->buffer) {
        fclose(fp);
        return FALSE;
    }
    size_t rd = fread(io->buffer, 1, fileSize, fp);
    io->buffer[rd] = '\0';
    io->size = rd;
    fclose(fp);
    io->ptr = io->buffer;
    io->end = io->buffer + io->size;
    return TRUE;
}

/* Opens file in either file or memory mode
   Returns TRUE on success, FALSE on failure */
BOOL ioOpen(MyIO *io, const char *filename, BOOL readAllInMemory) {
    memset(io, 0, sizeof(*io));
    if (!filename) return FALSE;
    if (!readAllInMemory) {
        io->fp = fopen(filename, "r");
        if (!io->fp) return FALSE;
        io->useFile = TRUE;
    }
    else {
        if (!loadFileIntoBuffer(NULL, filename, io, TRUE))
            return FALSE;
        io->useFile = FALSE;
    }
    return TRUE;
}

/* Closes file or frees memory */
void ioClose(MyIO *io) {
    if (io->useFile) {
        if (io->fp) {
            fclose(io->fp);
            io->fp = NULL;
        }
    }
    else {
        if (io->buffer) {
            free(io->buffer);
            io->buffer = NULL;
        }
    }
}

/* Skips to next line in file/buffer
   Returns TRUE if successful, FALSE at EOF */
BOOL ioSkipLine(MyIO *io) {
    if (io->useFile) {
        int c;
        while ((c = fgetc(io->fp)) != EOF && c != '\n');
        return (c != EOF);
    }
    else {
        while (io->ptr < io->end && *io->ptr != '\n') {
            io->ptr++;
        }
        if (io->ptr < io->end && *io->ptr == '\n') {
            io->ptr++;
        }
        return (io->ptr < io->end);
    }
}

/* Reads a short integer */
BOOL ioReadShort(MyIO *io, short *out) {
    if (io->useFile) {
        if (!out) return FALSE;
        int ret = fscanf(io->fp, "%hd", out);
        return (ret == 1);
    }
    else {
        while (io->ptr < io->end && isspace((unsigned char)*io->ptr))
            io->ptr++;
        if (io->ptr >= io->end) return FALSE;
        char *endp;
        long val = strtol(io->ptr, &endp, 10);
        if (endp == io->ptr || endp > io->end)
            return FALSE;
        if (val < SHRT_MIN || val > SHRT_MAX)
            return FALSE;
        *out = (short)val;
        io->ptr = endp;
        return TRUE;
    }
}

/* Reads an unsigned short integer */
BOOL ioReadUShort(MyIO *io, unsigned short *out) {
    if (io->useFile) {
        if (!out) return FALSE;
        int ret = fscanf(io->fp, "%hu", out);
        return (ret == 1);
    }
    else {
        if (!out) return FALSE;
        while (io->ptr < io->end && isspace((unsigned char)*io->ptr))
            io->ptr++;
        if (io->ptr >= io->end) return FALSE;
        char *endp;
        unsigned long val = strtoul(io->ptr, &endp, 10);
        if (endp == io->ptr)
            return FALSE;
        if (endp > io->end)
            return FALSE;
        *out = (unsigned short)val;
        io->ptr = endp;
        return TRUE;
    }
}

/* Reads an integer */
BOOL ioReadInt(MyIO *io, int *out) {
    if (io->useFile) {
        if (!out) return FALSE;
        int ret = fscanf(io->fp, "%d", out);
        return (ret == 1);
    }
    else {
        if (!out) return FALSE;
        while (io->ptr < io->end && isspace((unsigned char)*io->ptr))
            io->ptr++;
        if (io->ptr >= io->end) return FALSE;
        char *endp;
        long val = strtol(io->ptr, &endp, 10);
        if (endp == io->ptr)
            return FALSE;
        if (endp > io->end)
            return FALSE;
        *out = (int)val;
        io->ptr = endp;
        return TRUE;
    }
}

/* Reads a hexadecimal unsigned short */
BOOL ioReadHexUShort(MyIO *io, unsigned short *out) {
    if (io->useFile) {
        if (!out) return FALSE;
        unsigned int tmp;
        int ret = fscanf(io->fp, "%x", &tmp);
        if (ret == 1) {
            *out = (unsigned short)tmp;
            return TRUE;
        }
        return FALSE;
    }
    else {
        if (!out) return FALSE;
        while (io->ptr < io->end && isspace((unsigned char)*io->ptr))
            io->ptr++;
        if (io->ptr >= io->end) return FALSE;
        char *endp;
        unsigned long val = strtoul(io->ptr, &endp, 16);
        if (endp == io->ptr)
            return FALSE;
        if (endp > io->end)
            return FALSE;
        *out = (unsigned short)val;
        io->ptr = endp;
        return TRUE;
    }
}

/* Reads a hexadecimal unsigned long */
BOOL ioReadHexULong(MyIO *io, unsigned long *out) {
    if (io->useFile) {
        if (!out) return FALSE;
        int ret = fscanf(io->fp, "%lx", out);
        return (ret == 1);
    }
    else {
        if (!out) return FALSE;
        while (io->ptr < io->end && isspace((unsigned char)*io->ptr))
            io->ptr++;
        if (io->ptr >= io->end) return FALSE;
        char *endp;
        unsigned long val = strtoul(io->ptr, &endp, 16);
        if (endp == io->ptr)
            return FALSE;
        if (endp > io->end)
            return FALSE;
        *out = val;
        io->ptr = endp;
        return TRUE;
    }
}

/* Reads a single character */
BOOL ioReadChar(MyIO *io, char *out) {
    if (io->useFile) {
        if (!out) return FALSE;
        int c = fgetc(io->fp);
        if (c == EOF)
            return FALSE;
        *out = (char)c;
        return TRUE;
    }
    else {
        if (!out) return FALSE;
        if (io->ptr >= io->end) return FALSE;
        *out = *io->ptr;
        io->ptr++;
        return TRUE;
    }
}

/* Removes quotes from string tokens */
static void stripQuotes(char *str) {
    if (!str) return;
    size_t len = strlen(str);
    if (len == 0) return;
    if (str[0] == '\'' || str[0] == '\"') {
        memmove(str, str + 1, len);
        len--;
    }
    if (len > 0) {
        len = strlen(str);
        if (len > 0 && (str[len - 1] == '\'' || str[len - 1] == '\"')) {
            str[len - 1] = '\0';
        }
    }
}

/* Reads a string token */
BOOL ioReadToken(MyIO *io, char *outBuffer, size_t maxLen) {
    if (!outBuffer || maxLen < 1) return FALSE;
    outBuffer[0] = '\0';
    if (io->useFile) {
        int ret = fscanf(io->fp, "%s", outBuffer);
        if (ret == 1) {
            stripQuotes(outBuffer);
            return TRUE;
        }
        return FALSE;
    }
    else {
        while (io->ptr < io->end && isspace((unsigned char)*io->ptr))
            io->ptr++;
        if (io->ptr >= io->end)
            return FALSE;
        size_t i = 0;
        while (io->ptr < io->end && !isspace((unsigned char)*io->ptr) && i < (maxLen - 1)) {
            outBuffer[i++] = *io->ptr;
            io->ptr++;
        }
        outBuffer[i] = '\0';
        stripQuotes(outBuffer);
        return (i > 0);
    }
}

/* Reads a date in DD/MM/YYYY format */
BOOL ioReadData(MyIO *io, struct data *pdata) {
    short gg, mm, aa;
    char c;
    if (!ioReadShort(io, &gg)) return FALSE;
    if (!ioReadChar(io, &c)) return FALSE;
    if (c != '/') return FALSE;
    if (!ioReadShort(io, &mm)) return FALSE;
    if (!ioReadChar(io, &c)) return FALSE;
    if (c != '/') return FALSE;
    if (!ioReadShort(io, &aa)) return FALSE;
    pdata->g = (char)gg;
    pdata->m = (char)mm;
    pdata->a = aa;
    return TRUE;
}

/* Reads a time in HH:MM:SS format */
BOOL ioReadOra(MyIO *io, struct ora *pora) {
    short hh, mm, ss;
    char c;
    if (!ioReadShort(io, &hh)) return FALSE;
    if (!ioReadChar(io, &c)) return FALSE;
    if (c != ':') return FALSE;
    if (!ioReadShort(io, &mm)) return FALSE;
    if (!ioReadChar(io, &c)) return FALSE;
    if (c != ':') return FALSE;
    if (!ioReadShort(io, &ss)) return FALSE;
    pora->o = (char)hh;
    pora->m = (char)mm;
    pora->s = (char)ss;
    return TRUE;
}

/* Reads a floating point value */
BOOL ioReadFloat(MyIO *io, float *out) {
    if (io->useFile) {
        if (!out) return FALSE;
        int ret = fscanf(io->fp, "%f", out);
        return (ret == 1);
    }
    else {
        while (io->ptr < io->end && isspace((unsigned char)*io->ptr))
            io->ptr++;
        if (io->ptr >= io->end) return FALSE;
        char *endp;
        float val = strtof(io->ptr, &endp);
        if (endp == io->ptr || endp > io->end)
            return FALSE;
        *out = val;
        io->ptr = endp;
        return TRUE;
    }
}

/* Reads a long double value */
BOOL ioReadLongDouble(MyIO *io, long double *out) {
    if (io->useFile) {
        if (!out) return FALSE;
        int ret = fscanf(io->fp, "%Lf", out);
        return (ret == 1);
    }
    else {
        while (io->ptr < io->end && isspace((unsigned char)*io->ptr))
            io->ptr++;
        if (io->ptr >= io->end) return FALSE;
        char *endp;
        long double val = strtold(io->ptr, &endp);
        if (endp == io->ptr || endp > io->end)
            return FALSE;
        *out = val;
        io->ptr = endp;
        return TRUE;
    }
}

/* ============== Record read functions ============== */

/* Reads a complete record using custom parsing functions
   Records follow format: :<hex>[<n>]( <fields...> <date> <time> 
   ps: pardon my italian comments, I was getting lost, I'm a noob */
BOOL read_record_custom(MyIO *io, Record *rec) {
    char c;
    if (!ioReadChar(io, &c) || c != ':') return FALSE;
    if (!ioReadHexULong(io, &rec->pn_prog)) return FALSE;
    if (!ioReadChar(io, &c) || c != '[') return FALSE;
    if (!ioReadShort(io, &rec->pn_n)) return FALSE;
    if (!ioReadChar(io, &c) || c != ']') return FALSE;
    if (!ioReadChar(io, &c) || c != '(') return FALSE;
    if (!ioReadShort(io, &rec->field_short)) return FALSE;
    if (!ioReadUShort(io, &rec->field_ushort)) return FALSE;
    if (!ioReadInt(io, &rec->field_int)) return FALSE;
    if (!ioReadHexUShort(io, &rec->field_hexushort)) return FALSE;
    if (!ioReadHexULong(io, &rec->field_hexulong)) return FALSE;
    if (!ioReadFloat(io, &rec->field_float)) return FALSE;
    if (!ioReadLongDouble(io, &rec->field_ldouble)) return FALSE;
    if (!ioReadToken(io, rec->token, sizeof(rec->token))) return FALSE;
    {
        struct data d;
        if (!ioReadData(io, &d)) return FALSE;
        rec->day = d.g;
        rec->month = d.m;
        rec->year = d.a;
    }
    {
        struct ora o;
        if (!ioReadOra(io, &o)) return FALSE;
        rec->hour = o.o;
        rec->minute = o.m;
        rec->second = o.s;
    }
    // Consuma il carattere di newline; se siamo a fine file va bene
    if (io->ptr < io->end) {
        if (!ioReadChar(io, &c))
            return FALSE;
        // Se il carattere non è '\n', prova a saltare eventuali spazi fino al newline
        while (c != '\n' && isspace((unsigned char)c)) {
            if (io->ptr >= io->end)
                break;
            if (!ioReadChar(io, &c))
                break;
        }
        // Se il carattere finale non è '\n' e non siamo a fine file, segnala errore
        if (c != '\n' && io->ptr < io->end)
            return FALSE;
    }
    return TRUE;
}

/* ============== Test functions ============== */

/* Creates test file with structured data of target_size bytes */
void create_test_file(const char *filename, size_t target_size) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("fopen");
        exit(1);
    }
    size_t total_written = 0;
    unsigned long rec_no = 0;
    while(total_written < target_size) {
        int n = fprintf(f,
            ":%lx[%hd]( %hd %hu %d %x %lx %f %Lf %s %02hd/%02hd/%04hd %02hd:%02hd:%02hd\n",
            rec_no, (short)5,
            (short)(rec_no % 32767), (unsigned short)(rec_no % 65535),
            (int)rec_no, (unsigned short)(rec_no % 65535), rec_no,
            (float)(rec_no * 0.1), (long double)(rec_no * 0.01),
            "token",
            (short)1, (short)1, (short)2020,
            (short)(rec_no % 24), (short)(rec_no % 60), (short)(rec_no % 60)
        );
        if(n < 0)
            break;
        total_written += n;
        rec_no++;
        if(rec_no % 100000 == 0)
            printf("%lu record created, %zu bytes written...\n", rec_no, total_written);
    }
    fclose(f);
    printf("File '%s' created: %zu byte, %lu record\n", filename, total_written, rec_no);
}

/* Tests reading performance using standard fscanf */
void test_fscanf(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("fopen");
        exit(1);
    }
    Record rec;
    unsigned long count = 0;
    clock_t start = clock();
    while (fscanf(f, ":%lx[%hd]( %hd %hu %d %x %lx %f %Lf %63s %hd/%hd/%hd %hd:%hd:%hd\n",
           &rec.pn_prog, &rec.pn_n, &rec.field_short, &rec.field_ushort,
           &rec.field_int, &rec.field_hexushort, &rec.field_hexulong,
           &rec.field_float, &rec.field_ldouble, rec.token,
           &rec.day, &rec.month, &rec.year, &rec.hour, &rec.minute, &rec.second) == 16)
    {
        count++;
    }
    clock_t end = clock();
    fclose(f);
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("fscanf: %lu record read in %.3f seconds (%.3f usec/record)\n",
           count, elapsed, (elapsed*1e6)/count);
}

/* Tests reading performance using custom memory buffer parsing */
void test_custom(const char *filename) {
    MyIO io;
    if (!ioOpen(&io, filename, TRUE)) {
        fprintf(stderr, "ioOpen filed for %s\n", filename);
        exit(1);
    }
    Record rec;
    unsigned long count = 0;
    clock_t start = clock();
    while (read_record_custom(&io, &rec)) {
        count++;
    }
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("custom: %lu record read in %.3f seconds (%.3f usec/record)\n",
           count, elapsed, (elapsed*1e6)/count);
    ioClose(&io);
}

/* Main function - creates test file if needed, then runs benchmarks */
int main(int argc, char *argv[]) {
    const char *filename = "testdata.txt";
    size_t target_size = 300UL * 1024 * 1024; // 300 MB

    FILE *fcheck = fopen(filename, "r");
    if (!fcheck) {
        printf("Generating test file '%s' (~%zu byte)...\n", filename, target_size);
        create_test_file(filename, target_size);
    } else {
        fclose(fcheck);
        printf("Test file '%s' already existing.\n", filename);
    }

    test_fscanf(filename);

    test_custom(filename);

    return 0;
}
