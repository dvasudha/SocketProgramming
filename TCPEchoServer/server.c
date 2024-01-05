#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#define PORT 5007


int writen(int socket_fd, char message[], int message_length)
{

    int status = send(socket_fd, message, message_length, 0);
    if (status == -1)
    {

        if (errno == EINTR)
        {

            printf("EINTR Error - rewritting");
            status = writen(socket_fd, message, message_length);
        }
    }
    return status;
}

int readline(int socket_fd, char buffer[], int max_bytes)
{
    char each_byte_read; 
    int read_status;
    printf("Reading maximum of %d at a time\n", max_bytes);
    int read_bytes = 0; // this can go to maximum max_bytes
    while (read_bytes < max_bytes)
    {
        
        read_status = read(socket_fd, &each_byte_read, 1);

        if (read_status == -1)
        {
            if (errno == EINTR)
            {

                printf("EINTR Error - rereading");
                continue;
            }
            else
            {

                break;
            }
        }
        //EOF
        if ( each_byte_read == '\0')
        { 
            break;
        }
        buffer[read_bytes] = each_byte_read;
        read_bytes++;
        if (each_byte_read == '\n')
        { // end of line
            break;
        }
    }
    buffer[read_bytes] = '\0';
    printf("%s",buffer);
  return read_status;
}


 
int main()
{
    int server_fd;
 	char buffer[1024];
    struct sockaddr_in serverAddr;

    int clientSocket;
 	int cnt=0;
    // Client socket address structures
    struct sockaddr_in cliAddr;
 
    // Stores byte size of server socket address
    socklen_t addr_size;
 
    // Child process id
    pid_t childpid;
 
    // Creates a TCP socket id from IPV4 family
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
 
    // Error handling if socket id is not valid
    if (server_fd < 0) {
        printf("Error in connection.\n");
        exit(1);
    }
 
    printf("Server Socket is created.\n");
 
    // Initializing address structure with NULL
    memset(&serverAddr, '\0',
           sizeof(serverAddr));
 
    // Assign port number and IP address
    // to the socket created
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
 
    serverAddr.sin_addr.s_addr
        = INADDR_ANY;
 	printf("IP address is: %s\n", inet_ntoa(serverAddr.sin_addr));
    if ((bind(server_fd,(struct sockaddr*)&serverAddr, sizeof(serverAddr))) < 0) {
        printf("Error in binding.\n");
        exit(1);
    }
 
    // Listening for connections (upto 10)
    if (listen(server_fd, 10) == 0) {
        printf("Listening...\n\n");
    }
 
    while (1) {
 
        // Accept clients and
        // store their information in cliAddr
        clientSocket = accept(
            server_fd, (struct sockaddr*)&cliAddr,
            &addr_size);
        if (clientSocket < 0) {
            exit(1);
        }

        printf("Connection accepted from %s:%d\n",
               inet_ntoa(cliAddr.sin_addr),
              ntohs(cliAddr.sin_port) );
 

        printf("Clients connected: %d\n\n",
               ++cnt);
 
        // Creates a child process
        childpid = fork ();
        if (childpid == 0) {
            close(server_fd);
            char buffer[50]={0};
            while (1)
            {
            //printf("\n reading from client \n");
            int response = readline(clientSocket,buffer,50);
            if (response == 0)
            {
               
               printf("\n Closing Connection with client %s:%d \n",inet_ntoa(cliAddr.sin_addr),
              ntohs(cliAddr.sin_port) );
               break;
            }

        	char message[] =  "Responding to client on ip:";
            strcat(message,inet_ntoa(cliAddr.sin_addr));
			writen(clientSocket, message ,strlen(message));
            }

        }
    }
 
    // Close the client socket id
    close(clientSocket);
    return 0;
}
