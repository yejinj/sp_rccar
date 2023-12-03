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
void lcd_m(int line); // 몇번째 줄에 출력할지
void print_int(int i); // 정수 출력 코드
void cursor_to_home(void); // 커서를 홈 위치로 이동
void print_str(const char *s); // 문자열 출력 코드
void get_set(void); // 준비 함수
void time_limit(void); // 시간 제한
void delay_f(void); // 지연 함수

int main(int argc, char *argv[])
{
    // lcd setup
    if (wiringPiSetup () == -1) exit (1);
    fd = wiringPiI2CSetup(I2C_ADDR);
    
    pinMode(BUTTON_PIN, INPUT);
    pullUpDnControl(BUTTON_PIN, PUD_UP);

    // socket setup
    int serv_sock;
    struct sockaddr_in serv_addr;
    char msg[50] = {0}; // Adjust the size based on your expected message size

    // Register the signal handler
    signal(SIGINT, handle_signal);

    if (argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    if (setup_touch_sensor() == -1)
    {
        perror("Failed to set up touch sensor");
        exit(1);
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
    {
        perror("socket() error");
        exit(1);
    }

    // socket - server, client 초기화
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    // socket communication - error handling
    if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        perror("bind() error");
        exit(1);
    }

    if (listen(serv_sock, 5) == -1)
    {
        perror("listen() error");
        exit(1);
    }

    if (clnt_sock < 0)
    {
        clnt_sock = accept(serv_sock, NULL, NULL);
        if (clnt_sock == -1)
        {
            perror("accept() error");
            exit(1);
        }
    }

    // socket established
    printf("Connection established\n");

    // 출력 전 lcd 초기화
    lcd_init();
    print_str("ready..");

    for (int i = 0; i < 3; i++) {
        delay_f();
    }

    lcd_init();
    get_set();

    int time_limit = 120; // 2m
    
    int detected = 0;

    while (time_limit >= 0) {
        lcd_m(LINE2);
        print_int(time_limit / 60);
        print_str("m ");
        print_int(time_limit % 60);
        print_str("s ");

        // Check for touch sensor detection        
        if (!digitalRead(PIR)) {       
            delay_f();
            --time_limit;
        }

        else
        {
            // 감지되었을 때
            printf("Detected\n");
            
            // Notify the client to gracefully exit
            write(clnt_sock, "Police win!", sizeof("Police win!"));

            // Close client socket and exit
            close(clnt_sock);
            close(serv_sock);

            lcd_m(LINE1);
            print_str("Police win!");

            for (int i = 0; i < 3; i++) {
                delay_f();
            }

            detected = 1;
            exit(0);
        }
    }
    
    // Time limit 끝나도 감지되지 않았을 때
    if (detected == 0) {
        write(clnt_sock, "Theif win!", sizeof("Theif win!"));

        lcd_m(LINE1);
        print_str("Theif win!");
        for (int i = 0; i < 3; i++) {
            delay_f();
        }
    }

    return 0;
}

void handle_signal(int signum)
{
    if (signum == SIGINT)
    {
        printf("Received SIGINT. Closing sockets and exiting.\n");

        // Close client socket if it's still open
        if (clnt_sock != -1)
            close(clnt_sock);

        exit(0);
    }
}

int setup_touch_sensor(void)
{
    if (wiringPiSetup() == -1)
        return -1;

    pinMode(PIR, INPUT);

    return 0;
}

void get_set(void) {
    int i;

    for (i = 3; i > 0; --i) {
        lcd_m(LINE2);
        print_int(i);
        delay_f();
    }
}

void time_limit(void) {
    int seconds = 10;
    //int seconds = 120;

    while (seconds >= 0) {
        lcd_m(LINE2);
        print_int(seconds / 60);
        print_str("m ");
        print_int(seconds % 60);
        print_str("s ");

        delay_f();
        --seconds;
    }

    if (seconds < 0) {
        lcd_init();
        cursor_to_home();

        print_str("Game Over!");
        for (int i = 0; i < 5; i++) {
            delay_f();
        }
    }
}

void print_int(int i) {
    char array1[20];
    sprintf(array1, "%d", i);
    print_str(array1);
}

void cursor_to_home(void) {
    lcd_byte(0x01, LCD_CMD);
    lcd_byte(0x02, LCD_CMD);
}

void lcd_m(int line) {
    lcd_byte(line, LCD_CMD);
}

void print_str(const char *s) {
    while (*s) lcd_byte(*(s++), LCD_CHR);
}

void lcd_byte(int bits, int mode) {
    int bits_high;
    int bits_low;

    bits_high = mode | (bits & 0xF0) | LCD_BACKLIGHT;
    bits_low = mode | ((bits << 4) & 0xF0) | LCD_BACKLIGHT;

    wiringPiI2CReadReg8(fd, bits_high);
    lcd_toggle_enable(bits_high);

    wiringPiI2CReadReg8(fd, bits_low);
    lcd_toggle_enable(bits_low);
}

void lcd_toggle_enable(int bits) {
    delayMicroseconds(500);
    wiringPiI2CReadReg8(fd, (bits | ENABLE));
    delayMicroseconds(500);
    wiringPiI2CReadReg8(fd, (bits & ~ENABLE));
    delayMicroseconds(500);
}

void lcd_init() {
    lcd_byte(0x33, LCD_CMD);
    lcd_byte(0x32, LCD_CMD);
    lcd_byte(0x06, LCD_CMD);
    lcd_byte(0x0C, LCD_CMD);
    lcd_byte(0x28, LCD_CMD);
    lcd_byte(0x01, LCD_CMD);
    delayMicroseconds(500);
}

void delay_f() {
    char buffer[BUFFER_SIZE];
    time_t temp = 0;

    while (1) {
        // 현재 시간 얻기
        time_t current_time;
        time(&current_time);

        // 초 단위로 버퍼와 현재 시간 비교
        if (temp != current_time) {
            temp = current_time;

            // 초 단위의 값을 문자열로 변환하여 버퍼에 저장
            snprintf(buffer, sizeof(buffer), "%ld", temp);

            // 현재 시간과 버퍼의 초 단위 값이 같을 때까지 1초씩 대기
            while (temp == current_time) {
                time(&current_time);
            }

            // 1초가 지난 후에 루프를 빠져나감
            break;
        }
    }
}
