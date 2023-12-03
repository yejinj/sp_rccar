#include <string.h>
#include <wiringPi.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define JOY_CODE 0

static const char *DEVICE = "/dev/spidev0.0";
static uint8_t MODE = 0;
static uint8_t BITS = 8;
static uint32_t CLOCK = 1000000;
static uint16_t DELAY = 5;

int sockfd;
int joystick_fd;

int initJoystick();
void readJoystick();
void sendJoystickState();

static int prepare(int fd)
{
    if (ioctl(fd, SPI_IOC_WR_MODE, &MODE) == -1)
    {
        perror("Can't set MODE");
        return -1;
    }

    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &BITS) == -1)
    {
        perror("Can't set number of BITS");
        return -1;
    }

    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &CLOCK) == -1)
    {
        perror("Can't set write CLOCK");
        return -1;
    }

    if (ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &CLOCK) == -1)
    {
        perror("Can't set read CLOCK");
        return -1;
    }

    return 0;
}

uint8_t control_bits_differential(uint8_t channel)
{
    return (channel & 7) << 4;
}

uint8_t control_bits(uint8_t channel)
{
    return 0x8 | control_bits_differential(channel);
}

int readadc(int fd, uint8_t channel)
{
    uint8_t tx[] = {1, control_bits(channel), 0};
    uint8_t rx[3];

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = ARRAY_SIZE(tx),
        .delay_usecs = DELAY,
        .speed_hz = CLOCK,
        .bits_per_word = BITS,
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) == 1)
    {
        perror("IO Error");
        abort();
    }

    return ((rx[1] << 8) & 0x300) | (rx[2] & 0xFF);
}

int main(int argc, char *argv[])
{
    sockfd = initSocket(argc, argv);

    if (sockfd == -1)
    {
        fprintf(stderr, "Socket initialization failed\n");
        return 1;
    }

    joystick_fd = initJoystick();

    while (1)
    {
        int ch[3] = {
            0,
        };
        readJoystick(joystick_fd, ch, 3);
        sendJoystickState(ch);

        delay(100); // 0.1초 딜레이
    }

    close(sockfd);
    return 0;
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

int initJoystick()
{
    int fd = open(DEVICE, O_RDWR);
    if (fd <= 0)
    {
        perror("Device open error");
        return -1;
    }

    if (prepare(fd) == -1)
    {
        perror("Device prepare error");
        return -1;
    }

    return fd;
}
//채널값 ch[0] ch[1] ch[2] 가져오기
void readJoystick(int fd, int ch[], int size)
{
    for (int i = 0; i < size; i++)
    {
        int value = readadc(fd, i);
        ch[i] = value;
    }
    printf("code%d: %d %d %d\n", JOY_CODE, ch[0], ch[1], ch[2]);
}

void sendJoystickState(int ch[])
{
    char message[256];
    sprintf(message, "%d %d %d %d", JOY_CODE, ch[0], ch[1], ch[2]);
    send(sockfd, message, strlen(message), 0);
}

int initSocket(int argc, char *argv[])
{
    int server_sock;
    struct sockaddr_in server_addr;

    if (argc != 3)
    {
        printf("Usage : %s <port>\n", argv[1]);
        exit(-1);
    }

    if ((server_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        error_handling("Socket creation failed");
        exit(-1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    server_addr.sin_port = htons(atoi(argv[2]));

    if (connect(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Socket connection error");
        return -1;
    }

    return server_sock;
}
