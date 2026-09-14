/*
Copyright (C) 2026 waxodium <waxodium@proton.me>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdarg.h>
#include <limits.h>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <unistd.h>
#endif

typedef struct {
    char data[1024];
    int position;
} OutputBuffer;

typedef struct {
    int prefix;
    int zero_pad;
    int width;
    char specifier;
} FormatTable;

void write_chunk(const char *buf, int length) {
    if (length <= 0) return;

#if defined(_WIN32)
    DWORD written;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, length, &written, NULL);
#else
    write(1, buf, length);
#endif
}

void flush(OutputBuffer *buffer) {

    if (buffer->position > 0) {
        write_chunk(buffer->data, buffer->position);
        buffer->position = 0;
    }
}

void addchar(OutputBuffer *buffer, char c) {
    if (buffer->position >= 1023)
        flush(buffer);

    buffer->data[buffer->position++] = c;
}


FormatTable parse_format(const char **cursor){
    FormatTable format = {0};

    (*cursor)++;

    while (**cursor == '#' || **cursor == '0') {
        if (**cursor == '#')
            format.prefix = 1;
        else
            format.zero_pad = 1;

        (*cursor)++;
    }

    while (**cursor >= '0' && **cursor <= '9') {
        format.width = format.width * 10 + (**cursor - '0');
        (*cursor)++;
    }

    format.specifier = **cursor;

    return format;
}

void sout(const char *format, ...) {
    OutputBuffer stream = {
        .position = 0
    };

    va_list arguments;
    va_start(arguments, format);

    for (const char *cursor = format; *cursor != '\0'; cursor++) {
        if (stream.position >= 1023)
            flush(&stream);

        if (*cursor == '%' && *(cursor + 1) != '\0') {
            FormatTable spec = parse_format(&cursor);

            switch (spec.specifier) {
                case 's': {
                    const char *text = va_arg(arguments, const char *);

                    if (text == NULL)
                        text = "(null)";

                    while (*text != '\0')
                        addchar(&stream, *text++);

                    break;
                }

                case 'd': {
                    int number = va_arg(arguments, int);
                    char digits[sizeof(unsigned int) * CHAR_BIT];
                    int p = 0;
                    unsigned int num;

                    if (number == 0) {
                        addchar(&stream, '0');
                    } else {
                        if (number < 0) {
                            addchar(&stream, '-');
                            num = (unsigned int)(-(number + 1)) + 1;
                        } else {
                            num = (unsigned int)number;
                        }

                        while (num > 0) {
                            digits[p++] = (num % 10) + '0';
                            num /= 10;
                        }

                        while (p > 0)
                            addchar(&stream, digits[--p]);
                    }

                    break;
                }

                case 'u': {
                    unsigned int number = va_arg(arguments, unsigned int);
                    char digits[sizeof(unsigned int) * CHAR_BIT];
                    int p = 0;

                    if (number == 0) {
                        addchar(&stream, '0');
                    } else {
                        while (number > 0) {
                            digits[p++] = (number % 10) + '0';
                            number /= 10;
                        }

                        while (p > 0)
                            addchar(&stream, digits[--p]);
                    }

                    break;
                }

                case 'x':
                case 'X': {
                    unsigned int number = va_arg(arguments, unsigned int);
                    unsigned int original = number;
                    char digits[sizeof(unsigned int) * 2];
                    int p = 0;

                    if (number == 0) {
                        digits[p++] = '0';
                    } else {
                        while (number > 0) {
                            unsigned int digit = number % 16;

                            if (digit < 10)
                                digits[p++] = '0' + digit;
                            else if (spec.specifier == 'x')
                                digits[p++] = 'a' + (digit - 10);
                            else
                                digits[p++] = 'A' + (digit - 10);

                            number /= 16;
                        }
                    }

                    int prefix = spec.prefix && original != 0 ? 2 : 0;
                    int padding = spec.width - p - prefix;

                    if (padding < 0)
                        padding = 0;

                    if (spec.zero_pad) {
                        if (prefix) {
                            addchar(&stream, '0');
                            addchar(&stream, spec.specifier);
                        }

                        while (padding-- > 0)
                            addchar(&stream, '0');
                    } else {
                        while (padding-- > 0)
                            addchar(&stream, ' ');

                        if (prefix) {
                            addchar(&stream, '0');
                            addchar(&stream, spec.specifier);
                        }
                    }

                    while (p > 0)
                        addchar(&stream, digits[--p]);

                    break;
                }

                case 'c': {
                    addchar(&stream, (char)va_arg(arguments, int));
                    break;
                }

                case '%': {
                    addchar(&stream, '%');
                    break;
                }

                default: {
                    addchar(&stream, '%');
                    addchar(&stream, *cursor);
                    break;
                }
            }
        } else {
            addchar(&stream, *cursor);
        }
    }

    flush(&stream);
    va_end(arguments);
}
