#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

#include "parser_core.h"

struct file_info *open_file(const char *filename) {
    struct file_info *finfo;
    finfo = (struct file_info *)malloc(sizeof(struct file_info));
    if (!finfo) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    struct stat fstats;
    int error = stat(filename,&fstats);
    if (error) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    int fd = open(filename,O_RDONLY);
    if (fd == -1) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    char *raw_data =
        mmap(NULL,fstats.st_size,PROT_READ,MAP_POPULATE | MAP_PRIVATE,fd,0);
    if (raw_data == MAP_FAILED) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    error = close(fd);
    if (error) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }

    finfo->name = (char *)malloc((strlen(filename) + 1) * sizeof(char));
    if (!finfo->name) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    finfo->name = strcpy(finfo->name,filename);
    finfo->size = fstats.st_size;
    finfo->raw_begin = raw_data;
    finfo->raw_end = &raw_data[fstats.st_size];
    finfo->pos = finfo->raw_begin;
    finfo->line_num = 1;

    return finfo;
}

void close_file(struct file_info **finfo) {
    int error = munmap((*finfo)->raw_begin,(*finfo)->size);
    if (error) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    free((*finfo)->name);
    free(*finfo);
    *finfo = NULL;
}

void parse_eat_whitechars(struct file_info *input) {
    assert(input);
    char **buf = &input->pos;

    if (!*buf)
        return;
    char *end = input->raw_end;
    if (*buf == end) {
        *buf = NULL;
        return;
    }
    while (*buf != end) {
        if (!isspace(**buf) || **buf == '\n')
            break;
        (*buf)++;
    }
    if (*buf == end)
        *buf = NULL;
}

void parse_eat_newline(struct file_info *input) {
    /* skip all CR and LF */

    assert(input);
    char **buf = &input->pos;

    char *end = input->raw_end;
    while (*buf != end) {
        if (**buf == '\r')
            ;
        else if (**buf != '\n')
            break;
        else
            input->line_num++;
        (*buf)++;
    }
    if (*buf == end)
        *buf = NULL;
}

int discard_line(struct file_info *input) {
    assert(input);
    char **buf = &input->pos;

    int bad_chars = 0;
    char *end = input->raw_end;
    if (!(*buf))
        return 0;
    while (*buf != end) {
        if (**buf == '\n')
            break;
        else if (**buf != '\r')
            bad_chars++;
        (*buf)++;
    }
    if (*buf == end)
        *buf = NULL;
    return bad_chars;
}

int isdelimiter(char c) {
    if (isspace(c))
        return 1;
    if (isalnum(c))
        return 0;

    //custom ispunct()
    switch (c) {
    case '!':
    case '"':
    case '#':
        return 1;
    case '$':
        return 0;
    case '%':
    case '&':
    case 0x27:
    case '(':
    case ')':
    case '*':
    case '+':
    case ',':
        return 1;
    case '-':
        return 0;
    case '.':
    case '/':
    case ':':
    case ';':
    case '<':
    case '=':
    case '>':
    case '?':
    case '@':
    case '[':
    case '\\':
    case ']':
    case '^':
          return 1;
    case '_':
        return 0;
    case '`':
    case '{':
    case '|':
    case '}':
    case '~':
        return 1;
    }

    return 1;
}

char *parse_string(struct file_info *input, char *info) {
    assert(input);
    char **buf = &input->pos;

    //printf("in function: %s\n",__FUNCTION__);

    if (!*buf || isdelimiter(**buf)) {
        printf("error:%lu: expected %s - exit.\n",input->line_num,info);
        exit(EXIT_FAILURE);
    }

    char *start = *buf;
    int size = 0;
    char *end = input->raw_end;
    while (*buf != end) {
        if (isdelimiter(**buf)) {
            break;
        }
        (*buf)++;
        size++;
    }

    char *name = (char*)malloc((size+1) * sizeof(char));
    if (!name) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    name = strncpy(name,start,size);
    name[size] = '\0';

    //convert to lowercase
    int i;
    for (i=0; i<size; ++i)
        name[i] = tolower(name[i]);

    //printf("debug: new string : %s\n",name);

    parse_eat_whitechars(input);

    //printf("debug: new string \"%s\"\tsize=%d\n",*name,size);

    //printf("out of function: %s\n",__FUNCTION__);
    return name;
}

char parse_char(struct file_info *input, char *patterns, char *info) {
    assert(input);
    char **buf = &input->pos;

    //if info == NULL, do not fail if patterns not found and return NULL char '\0'

    assert(patterns && strlen(patterns));

    char *end = input->raw_end;
    if (*buf == end)
        *buf = NULL;
    if (!*buf) {
        if (!info)
            return '\0';
        printf("error:%lu: expected %s - exit.\n",input->line_num,info);
        exit(EXIT_FAILURE);
    }

    char c = tolower(**buf);

    char *p = patterns;
    while (*p != '\0' && c != tolower(*p))
        p++;

    if (*p == '\0') {
        if (!info)
            return '\0';
        printf("error:%lu: parsing %s, expected '%s' got '%c' 0x%02x (hex ascii) - exit\n",
               input->line_num,info,patterns,c,c);
        exit(EXIT_FAILURE);
    }

    (*buf)++;
    if (*buf == end)
        *buf = NULL;

    return c;
}

double parse_value(struct file_info *input, char *prefix, char *info) {
    //printf("in function: %s\n",__FUNCTION__);

    assert(input);
    char **buf = &input->pos;

    if (!*buf || isdelimiter(**buf)) {
        printf("error:%lu: expected %s - exit\n",input->line_num,info);
        exit(EXIT_FAILURE);
    }

    if (prefix) {
        int i;
        int size = strlen(prefix);
        for (i=0; i<size; ++i) {
            char c = tolower(**buf);
            char p = tolower(prefix[i]);
            if (c != p) {
                printf("error:%lu: expected prefix '%s' before value - exit\n",
                       input->line_num,prefix);
                exit(EXIT_FAILURE);
            }
            (*buf)++;
        }
    }

    char *end = input->raw_end;
    errno = 0;

    double value;
    int offset = 0;
    int status = sscanf(*buf,"%lf%n",&value,&offset);
    if (errno) {
        perror(__FUNCTION__);
        exit(EXIT_FAILURE);
    }
    if (status == EOF) {
        printf("error: sscanf() early fail\n");
        exit(EXIT_FAILURE);
    }
    if (status == 0) {
        printf("error: sscanf() bad input\n");
        exit(EXIT_FAILURE);
    }

    (*buf) += offset;

    if (*buf == end)
        *buf = NULL;
    if (*buf && !isdelimiter(**buf)) {
        printf("error:%lu: invalid character '%c' after value %lf - exit.\n",
               input->line_num,**buf,value);
        exit(EXIT_FAILURE);
    }

    parse_eat_whitechars(input);

    //printf("debug: value=%lf\n",*value);

    return value;
}

double parse_value_optional(struct file_info *input, char *prefix,
                            double default_value) {
    //printf("in function: %s\n",__FUNCTION__);

    assert(input);
    char **buf = &input->pos;

    if (!*buf || isdelimiter(**buf))
        return default_value;
    else
        return parse_value(input,prefix,"value");
}
