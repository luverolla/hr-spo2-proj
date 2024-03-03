#ifndef HAL_UTILS_H_
#define HAL_UTILS_H_

#include "gpio.h"

#include <stdio.h>
#include <string.h>

#define GPIO_LINE(port, pin) ({ \
    GPIO_Line line = {0};       \
    line.port = (port);         \
    line.pin = (pin);           \
    line;                       \
})

#define HAL_GPIO_WriteLine(line, value) HAL_GPIO_WritePin(line->port, line->pin, value)
#define HAL_GPIO_ReadLine(line) HAL_GPIO_ReadPin(line->port, line->pin)

#define USART_PRINT(template, ...)                                                    \
    {                                                                                 \
        char msg[200];                                                                \
        (void)sprintf(msg, template, ##__VA_ARGS__);                                  \
        (void)HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY); \
    }

typedef struct GPIO_Line {
    GPIO_TypeDef *port;
    uint16_t pin;

} GPIO_Line;

#endif
