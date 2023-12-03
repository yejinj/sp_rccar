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
#include <stdbool.h>

#include "../touch_lcd_server.h"

#define IN 0
#define OUT 1

#define LOW 0
#define HIGH 1

#define PIN 20
#define POUT 21
#define VALUE_MAX 40
#define DIRECTION_MAX 40

bool server_ready_state = false;
bool client_ready_state = false;
bool stop_skill = true;
bool chaos_skill = true;

int game_start = 0;

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

void* thread_input_to_rc_clnt_socket(void* arg) {
    int rc_clnt_sock = *(int*)arg;
    int centi_sec_counter = 0;

    while (1) {
        //0.01초 마다 실행해야 하는 작업---------------------------------------------------
        if (GPIORead(PIN) == 0 ) { //조이스틱 값이 변경되었을 때
            write(rc_clnt_sock, "조이스틱 값", strlen("조이스틱 값"));
        }
        if(stop_skill){
          write(rc_clnt_sock, "stop_skill", strlen("stop_skill"));
          stop_skill = false;
        }
        if(chaos_skill){
          write(rc_clnt_sock, "chaos_skill", strlen("chaos_skill"));
          chaos_skill = false;
        }
        //0.01초 마다 실행해야 하는 작업---------------------------------------------------

        //0.1초마다
        if((centi_sec_counter%10)==0){

        }
        //0.1초마다

        if((centi_sec_counter%100)==0){
        //1초 마다 실행해야 하는 작업------------------------------------------------------
        if(!countdown_start){
            countdown = 3; //카운트 다운 초기화
        }
        
        else{
            lcd_m(LINE2);
            print_int(countdown);
            // count down 3초 보내기          
            // printf("Countdown: %d seconds\n", countdown);
            countdown--;
        }
        
        //countdown이 0이면 game start
        if (!countdown) {
            game_start = 1;
            printf("Game Start!\n");
        }

        if (game_start) {
            int time_limit = 120; // 2분
            int detected = 0;

            while (time_limit >= 0) {
                lcd_m(LINE2);
                print_int(time_limit / 60);
                print_str("m ");
                print_int(time_limit % 60);
                print_str("s ");

                // Check for touch sensor detection        
                if (!digitalRead(PIR)) {       
                    --time_limit;
                }

                else
                {
                    // 감지되었을 때
                    printf("Detected\n");
                    
                    // Notify the client to gracefully exit
                    write(clnt_sock, "Police win!", sizeof("Police win!"));

                    lcd_m(LINE1);
                    print_str("Police win!");

                    // Close client socket and exit
                    close(clnt_sock);
                    close(serv_sock);

                    detected = 1;
                    exit(0);
                }
            }
            
            // Time limit 끝나도 감지되지 않았을 때
            if (detected == 0) {
                write(clnt_sock, "Theif win!", sizeof("Theif win!"));

                lcd_m(LINE1);
                print_str("Theif win!");
            }
        }
        //1초 마다 실행해야 하는 작업------------------------------------------------------
        }
        
        centi_sec_counter++;
        usleep(10000); // 0.01초마다 버튼 상태 체크
    }
}

void* thread_rc_clnt_socket_to_output(void* arg) {
    int rc_clnt_sock = *(int*)arg;
    while (1) {
        char buffer[1024];
        int valread = read(rc_clnt_sock, buffer, 1024);
        if (valread > 0) {
          //rc카에서 읽어드린 값 
          if(strcmp(buffer,"터치센서건드림")){
            //game over
          }
        }
    }
}

void* thread_input_to_ctrl_clnt_socket(void* arg) {
    int ctrl_clnt_sock = *(int*)arg;
    int centi_sec_counter = 0;
    int countdown = 3;

    while (1) {
        //0.01초 마다 실행해야 하는 작업---------------------------------------------------
        //0.01초 마다 실행해야 하는 작업---------------------------------------------------

        //0.1초마다
        if((centi_sec_counter%10)==0){
            if (GPIORead(PIN) == 0) {
              server_ready_state = !server_ready_state;
              printf("server button state changed: %s\n", server_ready_state ? "true" : "false");
            }
            if (GPIORead(멈춤 스킬버튼핀) == 0) {
              printf("server stop skill button pressed");
              write(ctrl_clnt_sock, "server stop skill button pressed", strlen("sever stop skill button pressed"));
            }
            if (GPIORead(카오스 스킬버튼핀) == 0) {
              printf("sever chaos skill button pressed");
              write(ctrl_clnt_sock, "server chaos skill button pressed", strlen("sever chaos skill button pressed"));
            }
        }
        //0.1초마다
      
        if((centi_sec_counter%100)==0){
          //1초 마다 실행해야 하는 작업------------------------------------------------------
          if(!(server_ready_state && client_ready_state)){
            countdown = 3;//카운트 다운 초기화
            printf("Server Ready State: %s, Client Ready State: %s\n",
                server_ready_state ? "true" : "false",
                client_ready_state ? "true" : "false");
          }
          
          else{
            //count down 3초 보내기          
            write(ctrl_clnt_sock, "Countdown Start", strlen("Countdown Start"));
            printf("Countdown: %d seconds\n", countdown);
            countdown--;
          }

          //countdown이 0이면 game start
          if (countdown==0) {
            printf("Game Start!\n");
            write(ctrl_clnt_sock, "Game Start!", strlen("Game Start!"));
          }
          //1초 마다 실행해야 하는 작업------------------------------------------------------
        }

        centi_sec_counter++;
        usleep(10000); // 0.01초마다 버튼 상태 체크
    }
}

void* thread_ctrl_clnt_socket_to_output(void* arg) {
    int ctrl_clnt_sock = *(int*)arg;
    while (1) {
        char buffer[1024];
        int valread = read(ctrl_clnt_sock, buffer, 1024);
        if (valread > 0) {
          if (strcmp(buffer, "client start button pressed") == 0) {
                client_ready_state = !client_ready_state;
                printf("Client button state changed: %s\n", client_ready_state ? "true" : "false");
          }
          if (strcmp(buffer, "client stop skill button pressed") == 0) {
            //server모터멈춤
          }
          if (strcmp(buffer, "client chaos skill button pressed") == 0) {
            //server모터 반대로
          }
        }
    }
}

int main(int argc, char *argv[]) {
    int ctrl_serv_sock, ctrl_clnt_sock, rc_serv_sock, rc_clnt_sock;
    struct sockaddr_in ctrl_serv_addr;
    struct sockaddr_in ctrl_clnt_addr;
    struct sockaddr_in rc_serv_addr;
    struct sockaddr_in rc_clnt_addr;
    socklen_t ctrl_clnt_addr_size = sizeof(ctrl_clnt_addr);
    socklen_t rc_clnt_addr_size = sizeof(rc_clnt_addr);

    if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if ((ctrl_serv_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        error_handling("Control Socket creation failed");
        exit(EXIT_FAILURE);
    }

    if ((rc_serv_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        error_handling("RC Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&ctrl_serv_addr, 0, sizeof(ctrl_serv_addr));
    ctrl_serv_addr.sin_family = AF_INET;
    ctrl_serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    ctrl_serv_addr.sin_port = htons(atoi(argv[1]));

    memset(&rc_serv_addr, 0, sizeof(rc_serv_addr));
    rc_serv_addr.sin_family = AF_INET;
    rc_serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    rc_serv_addr.sin_port = htons(atoi(argv[2]));

    if (bind(ctrl_serv_sock, (struct sockaddr *)&ctrl_serv_addr, sizeof(ctrl_serv_addr)) < 0) {
        error_handling("Control Bind failed");
        exit(EXIT_FAILURE);
    }

    if (bind(rc_serv_sock, (struct sockaddr *)&rc_serv_addr, sizeof(rc_serv_addr)) < 0) {
        error_handling("RC Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(ctrl_serv_sock, 3) < 0) {
        error_handling("Listen failed");
        exit(EXIT_FAILURE);
    }
    
    if (listen(rc_serv_sock, 3) < 0) {
        error_handling("Listen failed");
        exit(EXIT_FAILURE);
    }

    if ((ctrl_clnt_sock = accept(ctrl_serv_sock, (struct sockaddr *)&ctrl_clnt_addr, &ctrl_clnt_addr_size)) < 0) {
        error_handling("Accept client socket failed");
        exit(EXIT_FAILURE);
    }

    if ((rc_clnt_sock = accept(rc_serv_sock, (struct sockaddr *)&rc_clnt_addr, &rc_clnt_addr_size)) < 0) {
        error_handling("Accept rc socket failed");
        exit(EXIT_FAILURE);
    }

    if (GPIOExport(POUT) == -1 || GPIOExport(PIN) == -1) {
        error_handling("GPIO Export failed");
        exit(EXIT_FAILURE);
    }

    if (GPIODirection(POUT, OUT) == -1 || GPIODirection(PIN, IN) == -1) {
        error_handling("GPIO Direction failed");
        exit(EXIT_FAILURE);
    }

    if (GPIOWrite(POUT, 1) == -1) {
        error_handling("GPIO Write failed");
        return 3;
    }

    pthread_t rc_input_thread, rc_output_thread, clnt_input_thread, clnt_output_thread;
    //rc카 라즈베리파이 입출력 thread
    pthread_create(&rc_input_thread, NULL, thread_input_to_rc_clnt_socket, (void*)&rc_clnt_sock);
    pthread_create(&rc_output_thread, NULL, thread_rc_clnt_socket_to_output, (void*)&rc_clnt_sock);

    //도둑조종기쪽으로 라즈베리파이 입출력 thread
    pthread_create(&clnt_input_thread, NULL, thread_input_to_ctrl_clnt_socket, (void*)&ctrl_clnt_sock);
    pthread_create(&clnt_output_thread, NULL, thread_ctrl_clnt_socket_to_output, (void*)&ctrl_clnt_sock);

    pthread_join(rc_input_thread, NULL);
    pthread_join(rc_output_thread, NULL);
    pthread_join(clnt_input_thread, NULL);
    pthread_join(clnt_output_thread, NULL);

    close(rc_clnt_sock);
    close(rc_serv_sock);
    close(ctrl_clnt_sock);   
     close(ctrl_serv_sock);

    if (GPIOUnexport(POUT) == -1 || GPIOUnexport(PIN) == -1) {
        return 4;
    }

    return 0;
}
