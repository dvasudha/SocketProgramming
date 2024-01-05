#include <stdio.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct SBCP_Header
{
    unsigned int version;
    unsigned int type;
    int length;
};

struct SBCP_Attr
{
    int type;
    int length;
    char payload[512];
};

struct SBCP_Message
{
    struct SBCP_Header header;
    struct SBCP_Attr attribute[2];
};

struct InfoCli
{
    int fd;
    char username[16];
    int client_no;
};

int client_no = 0;
struct InfoCli *clients;

int username_valid(char a[])
{
    int counter = 0;
    int return_value = 0;
    for (counter = 0; counter < client_no; counter++)
    {
        if (!strcmp(a, clients[counter].username))
        {
            printf("username %s already exists \n", a);
            return_value = 1;
            break;
        }
    }
    return return_value;
}

void ACK(int connfd)
{

    struct SBCP_Message ack_msg;
    struct SBCP_Header ack_header;
    struct SBCP_Attr ack_attr;
    int counter = 0;
    char temp[180];

    ack_header.version = 3;
    ack_header.type = 7;

    ack_attr.type = 4;
    strcpy(temp, "Sent!");
    ack_attr.length = strlen(temp) + 1;
    strcpy(ack_attr.payload, temp);
    ack_msg.header = ack_header;
    ack_msg.attribute[0] = ack_attr;

    write(connfd, (void *)&ack_msg, sizeof(ack_msg));
}

// send NAK message to client when error occurs
void NAK(int connfd, int code)
{

    struct SBCP_Message nak_msg;
    struct SBCP_Header nak_header;
    struct SBCP_Attr nak_attr;
    char temp[180];
    nak_header.version = 3;
    nak_header.type = 5;
    nak_attr.type = 1; // reason
    if (code == 1)
        strcpy(temp, "Username is in use");
    if (code == 2)
        strcpy(temp, "Client count exceeded");

    nak_attr.length = strlen(temp);
    strcpy(nak_attr.payload, temp);

    nak_msg.header = nak_header;
    nak_msg.attribute[0] = nak_attr;

    write(connfd, (void *)&nak_msg, sizeof(nak_msg));

    close(connfd);
}

// Broadcasting ONLINE Message to everyone except me and the listerner
void ONLINE(fd_set master, int serverSockFd, int connfd, int maxfd)
{
    struct SBCP_Message msg;
    int j;
    printf("Server accepted the client : %s \n", clients[client_no - 1].username);
    msg.header.version = 3;
    msg.header.type = 8;
    msg.attribute[0].type = 2;
    strcpy(msg.attribute[0].payload, clients[client_no - 1].username);

    for (j = 0; j <= maxfd; j++)
    {

        if (FD_ISSET(j, &master))
        {

            if (j != serverSockFd && j != connfd)
            {
                if ((write(j, (void *)&msg, sizeof(msg))) == -1)
                {
                    perror("send");
                }
            }
        }
    }
}

// Broadcast OFFLINE message to everyone except me and listener

void OFFLINE(fd_set master, int i, int serverSockFd, int connfd, int maxfd, int client_no)
{

    struct SBCP_Message offline_msg;
    int y, j;
    for (y = 0; y < client_no; y++)
    {
        if (clients[y].fd == i)
        {
            offline_msg.attribute[0].type = 2;
            strcpy(offline_msg.attribute[0].payload, clients[y].username);
        }
    }

    printf("Socket %d belonging to user '%s'is disconnected\n", i, offline_msg.attribute[0].payload);
    offline_msg.header.version = 3;
    offline_msg.header.type = 6;

    for (j = 0; j <= maxfd; j++)
    {
        // OFFLINE Message broadcasted to everyone except to myself and the listener
        if (FD_ISSET(j, &master))
        {

            if (j != i && j != serverSockFd)
            {

                if ((write(j, (void *)&offline_msg, sizeof(offline_msg))) == -1)
                {
                    perror("ERROR: Sending");
                }
            }
        }
    }
}




int join(int connfd, int maxClients)
{

    struct SBCP_Message msg;
    struct SBCP_Attr attr;
    char temp[30];

    int return_status = 0;
    read(connfd, (struct SBCP_Message *)&msg, sizeof(msg));

    attr = msg.attribute[0];

    strcpy(temp, attr.payload);

    if (client_no == maxClients)
    {
        return_status = 2;
        printf("Client count exceeded\n");
        NAK(connfd, 2); // 2- indicates client count exceeded.
        return return_status;
    }

    return_status = username_valid(temp);
    if (return_status == 1)
        NAK(connfd, 1); // 1- indicates client present already
    else
    {
        strcpy(clients[client_no].username, temp);
        clients[client_no].fd = connfd;
        clients[client_no].client_no = client_no;
        client_no = client_no + 1;
        printf("%d", client_no);
        ACK(connfd);
    }
    return return_status;
}

int main(int argc, char const *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "Please use the correct format: %s <hostname> <port> <max_clients>\n", argv[0]);
        exit(0);
    }
    struct SBCP_Message msg, offline_msg;
    struct SBCP_Message client_msg;
    struct SBCP_Attr client_attr;

    int serverSockFd, connfd, m, k;
    unsigned int len;
    int client_status = 0;
    struct sockaddr_in servAddr, *cli;
    struct hostent *hret;

    fd_set master;
    fd_set read_fds;
    int maxfd, temp, i = 0, j = 0, x = 0, y, bytes_read, maxClients = 0;
    //sockfd = socket(AF_UNSPEC,SOCK_STREAM,0);  -> For IPV4 or IPV6 
    serverSockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSockFd == -1)
    {
        perror("ERROR: Couldn't create a socket");
        exit(0);
    }
    else
        printf("Server socket is created.\n");

    bzero(&servAddr, sizeof(servAddr));

    int enable = 1;
    if (setsockopt(serverSockFd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    {
        perror("ERROR: Setsockopt\n");
        exit(1);
    }

    servAddr.sin_family = AF_INET;
    hret = gethostbyname(argv[1]);
    memcpy(&servAddr.sin_addr.s_addr, hret->h_addr, hret->h_length);
    servAddr.sin_port = htons(atoi(argv[2]));

    maxClients = atoi(argv[3]);

    clients = (struct InfoCli *)malloc(maxClients * sizeof(struct InfoCli));
    cli = (struct sockaddr_in *)malloc(maxClients * sizeof(struct sockaddr_in));

    if ((bind(serverSockFd, (struct sockaddr *)&servAddr, sizeof(servAddr))) != 0)
    {
        perror("ERROR: Couldn't bind the socket\n");
        exit(0);
    }
    else
        printf("Binding successful.\n");

    if ((listen(serverSockFd, maxClients)) != 0)
    {
        perror("ERROR: Couldn't listen.\n");
        exit(0);
    }
    else
        printf("Listening successful.\n");

    FD_SET(serverSockFd, &master);
    maxfd = serverSockFd;

    for (;;)
    {
        read_fds = master;
        if (select(maxfd + 1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("ERROR: select");
            exit(4);
        }

        for (i = 0; i <= maxfd; i++)
        {
            if (FD_ISSET(i, &read_fds))
            {
                if (i == serverSockFd)
                {
                    // Incoming connection
                    len = sizeof(cli[client_no]);
                    connfd = accept(serverSockFd, (struct sockaddr *)&cli[client_no], &len);

                    // Checking the validity of accept
                    if (connfd < 0)
                    {
                        perror("ERROR: Couldn't accept \n");
                        exit(0);
                    }

                    // accept working properly
                    else
                    {
                        temp = maxfd;
                        FD_SET(connfd, &master);
                        if (connfd > maxfd)
                        {
                            maxfd = connfd;
                        }
                        client_status = join(connfd, maxClients);
                        if (client_status == 0){
                            ONLINE(master, serverSockFd, connfd, maxfd);
                        }
                        else
                        {
                            // Username already present.Restore maxFD and remove it from the set
                            maxfd = temp;
                            FD_CLR(connfd, &master); // clear connfd to remove this client
                        }
                    }
                }
                else
                {
                    
                    if ((bytes_read = read(i, (struct SBCP_Message *)&client_msg, sizeof(client_msg))) <= 0)
                    {
                        // got error or connection closed by client
                        if (bytes_read == 0)
                            OFFLINE(master, i, serverSockFd, connfd, maxfd, client_no);
                        else
                            perror("ERROR In receiving");

                        close(i);
                        FD_CLR(i, &master); // remove from master set
                        for (k = 0; k < client_no; k++)
                        {
                            if (clients[k].fd == i)
                            {
                                m = k;
                                break;
                            }
                        }

                        for (x = m; x < (client_no - 1); x++)
                        {
                            clients[x] = clients[x + 1];
                        }
                        client_no--;
                    }
                    else
                    {

                        int payloadLength = 0;
                        char temp[16];
                        // Checking if the existing user becomes idle
                        if (client_msg.header.type == 9)
                        {

                            msg = client_msg;
                            msg.attribute[0].type = 2;
                            for (y = 0; y < client_no; y++)
                            {
                                if (clients[y].fd == i)
                                {
                                    strcpy(msg.attribute[0].payload, clients[y].username);
                                    strcpy(temp, clients[y].username);
                                    payloadLength = strlen(clients[y].username);
                                    temp[payloadLength] = '\0';
                                    printf("User '%s' is idle\n", temp);
                                }
                            }
                        }

                        else
                        {
                            

                            client_attr = client_msg.attribute[0]; // message
                            msg = client_msg;

                            msg.header.type = 3;
                            msg.attribute[1].type = 2; // username
                            payloadLength = strlen(client_attr.payload);
                            strcpy(temp, client_attr.payload);
                            temp[payloadLength] = '\0';

                            // Message forwarded to clients
                            for (y = 0; y < client_no; y++)
                            {
                                if (clients[y].fd == i)
                                    strcpy(msg.attribute[1].payload, clients[y].username);
                            }
                            printf("User '%s' : %s", msg.attribute[1].payload, temp);
                        }

                        for (j = 0; j <= maxfd; j++)
                        {
                            if (FD_ISSET(j, &master))
                            {
                                // Message broadcasted to everyone except to myself and the listener
                                if (j != i && j != serverSockFd)
                                {
                                    if ((write(j, (void *)&msg, bytes_read)) == -1)
                                    {
                                        perror("send");
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    close(serverSockFd);

    return 0;
}