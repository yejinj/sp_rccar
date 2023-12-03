// joystick 움직임에 따라 x,y값만 출력하는 것이 아닌 방향 출력해주는 코드

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_PORT 12345

void handleClient(int clientSocket);

int main()
{
    int serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    // Create socket
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    // Initialize server address struct
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    // Bind socket to address
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
    {
        perror("Bind error");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(serverSocket, 1) == -1)
    {
        perror("Listen error");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", SERVER_PORT);

    while (1)
    {
        // Accept a connection
        if ((clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen)) == -1)
        {
            perror("Accept error");
            continue;
        }

        printf("Connection from %s\n", inet_ntoa(clientAddr.sin_addr));

        // Handle client communication
        handleClient(clientSocket);
    }

    // Close the server socket
    close(serverSocket);

    return 0;
}

void handleClient(int clientSocket)
{
    char buffer[1024];
    int bytesRead;
    int code, ch0, ch1, ch2;

    while ((bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0)
    {
        buffer[bytesRead] = '\0';
        int result = sscanf(buffer, "%d %d %d %d", &code, &ch0, &ch1, &ch2);

        // printf("rc %d, ch1: %d, ch2: %d, ch3: %d \n", clientSocket, ch0, ch1, ch2);
        if (((ch0 > 400 && ch0 < 430) && ch1 == 0) && (ch2 > 400 && ch2 < 550))
        {
            printf("Stop\n");
        }
        if (ch1 == 0 && (ch2 > 990 && ch2 < 1100))
        {
            printf("Right\n");
        }
        if (((ch0 > 400 && ch0 < 430) && ch1 == 0) && ch2 == 0)
        {
            printf("Left\n");
        }
        if ((ch0 > 1010 && ch0 < 1023) && ch1 == 0)
        {
            printf("Forward\n");
        }
        if (ch0 == 266 && ch1 == 266)
        {
            printf("Backward\n");
        }
        if (ch0 == 0)
        {
            printf("button\n");
        }
    }

    if (bytesRead == -1)
    {
        perror("Receive error");
    }
}
