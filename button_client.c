#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#define IN 0
#define OUT 1

#define LOW 0
#define HIGH 1

#define PIN 20
#define POUT 21
#define VALUE_MAX 40
#define DIRECTION_MAX 40

static int GPIOExport(int pin) {
    #define BUFFER_MAX 3
  char buffer[BUFFER_MAX];
  ssize_t bytes_written;
  int fd;

  fd = open("/sys/class/gpio/export", O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open export for writing!\n");
    return (-1);
  }

  bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
  write(fd, buffer, bytes_written);
  close(fd);
  return (0);
}

static int GPIOUnexport(int pin) {
  char buffer[BUFFER_MAX];
  ssize_t bytes_written;
  int fd;

  fd = open("/sys/class/gpio/unexport", O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open unexport for writing!\n");
    return (-1);
  }

  bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
  write(fd, buffer, bytes_written);
  close(fd);
  return (0);
}

static int GPIODirection(int pin, int dir) {
  static const char s_directions_str[] = "in\0out";

  char path[DIRECTION_MAX];
  int fd;

  snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);
  fd = open(path, O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open gpio direction for writing!\n");
    return (-1);
  }

  if (-1 ==
      write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)) {
    fprintf(stderr, "Failed to set direction!\n");
    return (-1);
  }

  close(fd);
  return (0);
}

static int GPIORead(int pin) {
  char path[VALUE_MAX];
  char value_str[3];
  int fd;

  snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
  fd = open(path, O_RDONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open gpio value for reading!\n");
    return (-1);
  }

  if (-1 == read(fd, value_str, 3)) {
    fprintf(stderr, "Failed to read value!\n");
    return (-1);
  }

  close(fd);

  return (atoi(value_str));
}

static int GPIOWrite(int pin, int value) {
  static const char s_values_str[] = "01";

  char path[VALUE_MAX];
  int fd;

  snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
  fd = open(path, O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open gpio value for writing!\n");
    return (-1);
  }

  if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) {
    fprintf(stderr, "Failed to write value!\n");
    return (-1);
  }

  close(fd);
  return (0);
}

void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

void* thread_input_to_socket(void* arg) {
    int server_socket = *(int*)arg;
    while (1) {
        if (GPIORead(PIN) == 0) {
            write(server_socket, "client button pressed", strlen("client button pressed"));
        }
        usleep(100000); // 0.1초마다 버튼 상태 체크
    }
}

void* thread_socket_to_output(void* arg) {
    int server_socket = *(int*)arg;
    while (1) {
        char buffer[1024];
        int valread = read(server_socket, buffer, 1024);
        if (valread > 0) {
            printf("Message from server: %s\n", buffer);
        }
    }
}

int main(int argc, char *argv[]) {
    int client_sock;
    struct sockaddr_in serv_addr;

    if (argc != 3) {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if ((client_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        error_handling("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        error_handling("Connection failed");
        exit(EXIT_FAILURE);
    }

    if (GPIOExport(POUT) == -1 || GPIOExport(PIN) == -1) {
        error_handling("GPIO Export failed");
        exit(EXIT_FAILURE);
    }

    usleep(10000);

    if (GPIODirection(POUT, OUT) == -1 || GPIODirection(PIN, IN) == -1) {
        error_handling("GPIO Direction failed");
        exit(EXIT_FAILURE);
    }

    if (GPIOWrite(POUT, 1) == -1) {
        error_handling("GPIO Write failed");
        return 3;
    }

    pthread_t input_thread, output_thread;
    pthread_create(&input_thread, NULL, thread_input_to_socket, (void*)&client_sock);
    pthread_create(&output_thread, NULL, thread_socket_to_output, (void*)&client_sock);

    pthread_join(input_thread, NULL);
    pthread_join(output_thread, NULL);

    close(client_sock);

    return 0;
}
