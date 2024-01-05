#include <stdio.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

struct SBCP_Header
{
  unsigned int version ;
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


// Message from the server
int recv_msg(int sockfd)
{

  struct SBCP_Message serv_message;
  int status = 0;
  int nbytes = 0;
  int value, i;
  char a[] = "1";

  nbytes = read(sockfd, (struct SBCP_Message *)&serv_message, sizeof(serv_message));
  if (nbytes <= 0)
  {
    perror("Server disconnected \n");
    kill(0, SIGINT);
    exit(1);
  }

  // FWD message
  if (serv_message.header.type == 3)
  {
    if ((serv_message.attribute[0].payload != NULL || serv_message.attribute[0].payload != '\0') && (serv_message.attribute[1].payload != NULL || serv_message.attribute[1].payload != '\0') && serv_message.attribute[0].type == 4 && serv_message.attribute[1].type == 2)
    {
      printf("%s : %s ", serv_message.attribute[1].payload, serv_message.attribute[0].payload);
    }
    status = 0;
  }

  // NAK message header 5 attr type 1 for error reason
  if (serv_message.header.type == 5)
  {
    if ((serv_message.attribute[0].payload != NULL || serv_message.attribute[0].payload != '\0') && serv_message.attribute[0].type == 1)
    {
      printf("NAK : %s \n", serv_message.attribute[0].payload);
    }
    status = 1;
  }

  // OFFLINE message -> header 6 attr type 2 for username
  if (serv_message.header.type == 6)
  {
    if ((serv_message.attribute[0].payload != NULL || serv_message.attribute[0].payload != '\0') && serv_message.attribute[0].type == 2)
    {
      printf("User '%s' is now OFFLINE \n", serv_message.attribute[0].payload);
    }
    status = 0;
  }

  // ACK Message
  if (serv_message.header.type == 7)
  {
    if ((serv_message.attribute[0].payload != NULL || serv_message.attribute[0].payload != '\0') && serv_message.attribute[0].type == 4)
    {
      printf("ACK %s \n", serv_message.attribute[0].payload);
    }
    status = 0;
  }

  // ONLINE Message
  if (serv_message.header.type == 8)
  {
    if ((serv_message.attribute[0].payload != NULL || serv_message.attribute[0].payload != '\0') && serv_message.attribute[0].type == 2)
    {
      printf("User '%s' is now ONLINE \n", serv_message.attribute[0].payload);
    }
    status = 0;
  }

  // IDLE Message
  if (serv_message.header.type == 9)
  {
    if ((serv_message.attribute[0].payload != NULL || serv_message.attribute[0].payload != '\0') && serv_message.attribute[0].type == 2)
    {
      printf("User '%s' is now IDLE \n", serv_message.attribute[0].payload);
    }
    status = 0;
  }

  return status;
}

// Send a JOIN Message to server when connected to server
void join(int sockfd, const char *arg[])
{

  struct SBCP_Attr attribute_join;
  struct SBCP_Header header;
  struct SBCP_Message message;
  int return_status = 0;

  // Building 'JOIN' SBCP_Header
  header.version = '3';
  header.type = '2';
  message.header = header;

  // Building username attribute
  attribute_join.type = 2;
  attribute_join.length = strlen(arg[1]) + 1;
  strcpy(attribute_join.payload, arg[1]);
  message.attribute[0] = attribute_join;

  write(sockfd, (void *)&message, sizeof(message));

  // Wait for server's reply
  sleep(1);
  return_status = recv_msg(sockfd);
  if (return_status == 1)
  {
    close(sockfd);
    exit(0);
  }
}

// Accept user input, and send it to server for broadcasting
void send_msg(int connect)
{

  struct SBCP_Message message;
  struct SBCP_Attr attr;

  int bytes_read = 0;
  char temp[600];

  struct timeval tv;
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO, &readfds);

  select(STDIN_FILENO + 1, &readfds, NULL, NULL, NULL);
  if (FD_ISSET(STDIN_FILENO, &readfds))
  {
    bytes_read = read(STDIN_FILENO, temp, sizeof(temp));
    if (bytes_read > 0)
      temp[bytes_read] = '\0';

    strcpy(attr.payload, temp);
    attr.type = 4;
    message.attribute[0] = attr;
    write(connect, (void *)&message, sizeof(message));
  }
}

int main(int argc,char* argv[])
{
    if (argc != 4) {
       fprintf(stderr,"Please use this format: %s <username> <server_ip> <server_port>\n", argv[0]);
       exit(0);
    }
    if(argc == 4)
    {
	    int sockfd;
	    struct SBCP_Message msg;
	    struct sockaddr_in servaddr;
	    struct hostent* hostret;
	    fd_set master;
	    fd_set new;
	    fd_set read_fds;
	    FD_ZERO(&read_fds);
	    FD_ZERO(&master);
      //sockfd = socket(AF_UNSPEC,SOCK_STREAM,0);  -> For IPV4 or IPV6 
	    sockfd = socket(AF_INET,SOCK_STREAM,0);

	    if(sockfd==-1)
	    {
        	perror("Failure in socket creation\n");
	        exit(0);
	    }

	    else
	        printf("Socket creation is successful\n");
    
	    bzero(&servaddr,sizeof(servaddr));

	    servaddr.sin_family=AF_INET;
	    hostret = gethostbyname(argv[2]);
	    memcpy(&servaddr.sin_addr.s_addr, hostret->h_addr,hostret->h_length);
	    servaddr.sin_port = htons(atoi(argv[3]));


	    if(connect(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr))!=0)
	    {
	        printf("ERROR: Connecting to the server\n");
	        exit(0);
	    }
	    else
	    {
	        join(sockfd, argv);
		printf("Server connection successful \n");
	        FD_SET(sockfd, &master);
		FD_SET(STDIN_FILENO, &new);
		struct timeval tv;
		tv.tv_sec=10;
		tv.tv_usec=0;
			
		pid_t recvpid;
		int secs, usecs;
		recvpid=fork();
		if(recvpid==0){
		
	           for(;;)
			{
			read_fds=new;
			tv.tv_sec=10;
		        tv.tv_usec=0;
			secs=(int) tv.tv_sec;
		        usecs=(int) tv.tv_usec;
			
			if(select(STDIN_FILENO+1, &read_fds, NULL, NULL, &tv) == -1){
	                	perror("ERROR: select isn't functioning properrly\n");
                		exit(4);
            	    	}
			if(FD_ISSET(STDIN_FILENO, & read_fds)){
				send_msg(sockfd);
				continue;}
	
			else if(tv.tv_sec==0 && tv.tv_usec==0){
				
				printf("Time out!! No user input for %d secs %d usecs\n", secs, usecs);
				msg.header.type=9;
				msg.header.version=3;
				tv.tv_sec=10;
		        	tv.tv_usec=0;
				read_fds=new;
				write(sockfd,(void *) &msg,sizeof(msg));
				continue;
			}
			}
			
		    }
		    else{	
	           

	            for (;;){
		    read_fds = master;
	            
		    if(select(sockfd+1, &read_fds, NULL, NULL, NULL) == -1)
		    {
	                perror("select");
                	exit(0);
            	    }
		    
	            if(FD_ISSET(sockfd, &read_fds))
                    	recv_msg(sockfd);
		    
		    }
		    kill(recvpid, SIGINT);
                }

                printf("\n initConnection Ends \n");
            }
     }
     printf("\n Client close \n");
     return 0;
}