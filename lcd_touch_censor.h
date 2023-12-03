#ifndef __TOUCH_LCD_SERVER__
#define __TOUCH_LCD_SERVER__

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <memory.h>
#include <string.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 20

// prepare for socket communication
#define PIR 4       // BCM_GPIO 23
int clnt_sock = -1;
void handle_signal(int signum);
int setup_touch_sensor(void);

// prepare for lcd print
#define I2C_ADDR   0x27
#define LCD_CHR  1
#define LCD_CMD  0
#define LINE1  0x80
#define LINE2  0xC0
#define LCD_BACKLIGHT   0x08
#define ENABLE  0b00000100
#define BUTTON_PIN 23

// variable, function for lcd print
int fd;
void lcd_init(void);
void lcd_byte(int bits, int mode);
void lcd_toggle_enable(int bits);
void lcd_m(int line);
void print_int(int i);
void cursor_to_home(void);
void print_str(const char *s);
void get_set(void);
void time_limit(void);
void delay_f(void);

#endif
