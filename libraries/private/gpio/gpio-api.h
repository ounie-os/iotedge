#ifndef __GPIO_API_H_
#define __GPIO_API_H_

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

extern int gpio_request(int pin);
extern int gpio_set_direction(int pin, int dir);
extern int gpio_put_status(int pin, int status);
extern int gpio_free(int pin);
extern int gpio_get_status(int pin);
#endif

