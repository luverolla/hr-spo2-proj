#include "strfmt.h"

#include <string.h>

static void str_reverse(char *str, size_t startIdx, size_t size) {
    for (size_t i = startIdx; i < (startIdx + (size / 2U)); i++) {
        size_t j = i + size - 1U;
        char tmp = str[i];
        str[i] = str[j];
        str[j] = tmp;
    }
}

strbuf mkbuf(char *buf) {
    strbuf sb;
    sb.buf = buf;
    sb.index = 0U;
    return sb;
}

/**
 * Format a number and appends to a string starting from index + 1
 * @param str - pointer to the string
 * @param index - pointer to index
 * @param value - value to format
 */
void put_uint8(strbuf *buffer, uint8_t value) {
    size_t start = buffer->index;
    uint8_t digits = 0U;
    uint8_t quot = value;

    if (quot == 0U) {
        buffer->buf[(buffer->index)] = '0';
        buffer->index++;
    } else {
        while (quot > 0U) {
            buffer->buf[buffer->index] = (quot % 10U) + '0';
            buffer->index++;
            quot /= 10U;
            digits++;
        }
    }

    if (digits > 1U) {
        str_reverse(buffer->buf, start, digits);
    }
}

void put_uint16(strbuf *buffer, uint16_t value) {
    size_t start = buffer->index;
    uint8_t digits = 0U;
    uint16_t quot = value;

    if (quot == 0U) {
        buffer->buf[buffer->index] = '0';
        buffer->index++;
    } else {
        while (quot > 0U) {
            buffer->buf[buffer->index] = (quot % 10U) + '0';
            buffer->index++;
            quot /= 10U;
            digits++;
        }
    }

    if (digits > 1U) {
        str_reverse(buffer->buf, start, digits);
    }
}

void put_uint32(strbuf *buffer, uint32_t value) {
    size_t start = buffer->index;
    uint8_t digits = 0U;
    uint32_t quot = value;

    if (quot == 0U) {
        buffer->buf[buffer->index] = '0';
        buffer->index++;
    } else {
        while (quot > 0U) {
            buffer->buf[buffer->index] = (quot % 10U) + '0';
            buffer->index++;
            quot /= 10U;
            digits++;
        }
    }

    if (digits > 1U) {
        str_reverse(buffer->buf, start, digits);
    }
}

void put_char(strbuf *buffer, char value) {
    buffer->buf[buffer->index] = value;
    buffer->index++;
}

void put_str(strbuf *buffer, const char *value) {
    (void)strcpy(&buffer->buf[buffer->index], value);
    buffer->index += strlen(value);
}

void put_strn(strbuf *buffer, const char *value, size_t n) {
    strncpy(&buffer->buf[buffer->index], value, n);
    buffer->index += n;
}

void put_end(strbuf *buffer) {
    buffer->buf[buffer->index] = '\0';
}

void str_clear(strbuf *buffer) {
    (void)memset(buffer->buf, 0, strlen(buffer->buf));
    buffer->index = 0U;
}