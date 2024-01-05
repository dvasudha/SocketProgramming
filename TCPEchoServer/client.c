#include<errno.h>
#include<sys/socket.h>
#include<stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include<stdlib.h>

int main(int a, char* b[])
{
    //for creating the socket
    int c;
    c = socket(AF_INET, SOCK_STREAM, 0);
    if (c == -1)
    {
        perror("The socket could not be created");
        exit(-1);
    }
    puts("Socket has been created");

    if (a < 3) {  // to ensure proper arguments
        printf("Please specify server's IPv4 address and port number\n");
        return 0;
    }


    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(atoi(b[2]));
    serveraddr.sin_addr.s_addr = inet_addr(b[1]);


    //For connecting to the server
    if (connect(c, (struct sockaddr*)&serveraddr, sizeof serveraddr) == -1)
    {
        perror("Could not connect to the server");
        exit(-1);
    }

    puts("Connected to the server");

    //This is for sending and receiving the message
    char clientmessage[100];
    char servermsg[100];
    int msg_len, bytes_sent, bytes_recv;

    while (1)
    {
        puts("Please Enter your text here");

        //For checking if the EOF is present in the input
        if ((fgets(clientmessage, 100, stdin) == NULL) || feof(stdin))
        {
            puts("\n This is the end of the file ");
            break;
        }
        msg_len = strlen(clientmessage);


        
        bytes_sent = send(c, clientmessage, msg_len, 0);
        while (bytes_sent == -1)
        {
            puts("Unable to send message, trying again");
            bytes_sent = send(c, clientmessage, msg_len, 0);
        }
        puts("The Client has sent - ");
        puts(clientmessage);

        //For receiving the message from the server
        bytes_recv = recv(c, servermsg, 100, 0);
        while (bytes_recv == -1)
        {
            puts("Unable to send message, trying again");
            bytes_recv = recv(c, servermsg, 100, 0);
        }

        if (bytes_recv == 1) { //This If conditions checks if the message is empty
            if (strlen(servermsg) == 0) {
                continue;
            }
        }
        puts("This is the Response from the server - ");
        puts(servermsg);

        //This is for clearing the clientmessage and servermsg buffer
        memset(&clientmessage, '\0', sizeof(clientmessage));
        memset(&servermsg, '\0', sizeof(servermsg));
    }
    
    puts("Closing the connection");
    close(c);
}
