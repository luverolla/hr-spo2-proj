#ifndef STRFMT_H
#define STRFMT_H

/**
 * @file strfmt.h
 * @author Luigi Verolla <luverolla@gmx.it>
 * @brief String formatting utility
 *
 * Utility function to replace the standard sprintf function for string formatting
 * compliant to MISRA 2012 rules
 */

#include <stddef.h>
#include <stdint.h>

typedef struct strbuf {
    char *buf;
    size_t index;
} strbuf;

/**
 * @brief Create a string buffer
 *
 * @param buf pointer to char array
 * @return buffer instance
 */
strbuf mkbuf(char *buf);

/**
 * @brief Appends a 8-bit unsigned integer to the string buffer
 *
 * @param buffer receiver buffer
 * @param value 8-bit unsigned value
 */
void put_uint8(strbuf *buffer, uint8_t value);

/**
 * @brief Appends a 16-bit unsigned integer to the string buffer
 *
 * @param buffer receiver buffer
 * @param value 16-bit unsigned value
 */
void put_uint16(strbuf *buffer, uint16_t value);

/**
 * @brief Appends a 32-bit unsigned integer to the string buffer
 *
 * @param buffer receiver buffer
 * @param value 32-bit unsigned value
 */
void put_uint32(strbuf *buffer, uint32_t value);

/**
 * @brief Appends a character to the string buffer
 *
 * @param buffer receiver buffer
 * @param value character to append
 */
void put_char(strbuf *buffer, char value);

/**
 * @brief Appends a string to the string buffer
 *
 * @param buffer receiver buffer
 * @param value string to append
 */
void put_str(strbuf *buffer, const char *value);

/**
 * @brief Appends a string to the string buffer
 *
 * @param buffer receiver buffer
 * @param value string to append
 * @param n number of characters to append
 */
void put_strn(strbuf *buffer, const char *value, size_t n);

/**
 * @brief Terminate the string buffer with a null character
 *
 * @param buffer receiver buffer
 */
void put_end(strbuf *buffer);

/**
 * @brief Clear the string buffer
 *
 * @param buffer receiver buffer
 */
void str_clear(strbuf *buffer);

#endif // STRFMT_H