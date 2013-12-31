#ifndef __PARSER_CORE_H__
#define __PARSER_CORE_H__

struct file_info {
    char *name;
    char *raw_begin;
    char *raw_end;
    char *pos;  //current position, NULL if reached raw_end
    size_t size;
    unsigned long line_num;
};

struct file_info *open_file(const char *filename);
void close_file(struct file_info **finfo);

void parse_eat_whitechars(struct file_info *input);
void parse_eat_newline(struct file_info *input);
int discard_line(struct file_info *input);
int isdelimiter(char c);
char *parse_string(struct file_info *input, char *info);
char parse_char(struct file_info *input, char *patterns, char *info);
double parse_value(struct file_info *input, char *prefix, char *info);
double parse_value_optional(struct file_info *input, char *prefix,
                            double default_value);

#endif
