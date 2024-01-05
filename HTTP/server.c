#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/select.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <stdint.h>

#define BUFFER_SIZE 10000
#define MAX_NAME_LENGTH 256
#define MAX_TIME_LENGTH 50
#define MAX_FILE_NAME_LENGTH 10

// cache structure definition
typedef struct
{
    char url[MAX_NAME_LENGTH];
    char last_modified[MAX_TIME_LENGTH];
    char expiry[MAX_TIME_LENGTH];
    char filename[MAX_FILE_NAME_LENGTH];
    int  filled;
}cache;

cache cache_table[10];
char *day_arr[7]=
{
    "Sun",
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat"
};

char *mon_array[12]=
{
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec"
};

int month_to_int(char *month) {
for (int i = 0; i < 12; i++) {
    if (strcmp(month, mon_array[i]) == 0) {
        return i + 1;
    }
}
}


int time_compare(char *old_time_string, char *new_time_string)
{
    int old_year, old_month, old_hour, old_minute, old_second,old_day;
    int new_year, new_month, new_hour, new_minute, new_second,new_day;

    char old_month_string[4];
    char new_month_string[4];

    memset(old_month_string, 0, 4);
    memset(new_month_string, 0, 4);

    sscanf(old_time_string + 5, "%d %3s %d %d:%d:%d ",&old_day,old_month_string,&old_year,&old_hour,&old_minute,&old_second);
    sscanf(new_time_string + 5, "%d %3s %d %d:%d:%d ",&new_day,new_month_string,&new_year,&new_hour,&new_minute,&new_second);
    old_month = month_to_int(old_month_string);
    new_month = month_to_int(new_month_string);

    if (old_year < new_year) return -1;
    if (old_year > new_year) return 1;
    if (old_month < new_month) return -1;
    if (old_month > new_month) return 1;
    if (old_day < new_day) return -1;
    if (old_day > new_day) return 1;
    if (old_hour < new_hour) return -1;
    if (old_hour > new_hour) return 1;
    if (old_minute < new_minute) return -1;
    if (old_minute > new_minute) return 1;
    if (old_second < new_second) return -1;
    if (old_second > new_second) return 1;
    return 0;
}
int format_request(char *request, char *host, int *port,char *url, char *path)
{
    char method[MAX_NAME_LENGTH];
    char protocol[MAX_NAME_LENGTH];
    char *uri;
    int  return_values;
    return_values = sscanf(request, "%s %s %s %s", method, path, protocol, url);

    if(strcmp(method, "GET")!=0)
        return -1;

    if(strcmp(protocol, "HTTP/1.0")!=0)
        return -1;

    uri = url;
    uri = uri + 5;  //client request type GET %s HTTP/1.0 Host:%s
    strcpy (url,uri);
    strcpy (host,url);
    strcpy(url,"/");
    strcat (url, path);
    path = url;
    printf("path:%s",path);
    strcat(url,":");
    strcat(url,host);
    printf("url:%s",url);
    *port = 80;
    return return_values;

}

int check_cache_entry(char *url)
{
    int index = -1;
    int i = 0;
    for (i = 0 ; i < 10 ; i++)
    {
        if (!strcmp (cache_table[i].url, url))
        {
            index = i;
            break;
        }
    }
    return index;
}


int cache_expiry(char *url,struct tm *timenow)
{
    int i = 0, result;
    char upd_time[MAX_TIME_LENGTH];
    for (i = 0; i < 10; i++)
    {
        if (strcmp(cache_table[i].url, url)==0)
        {
            break;
        }
    }
    memset(upd_time, 0, MAX_TIME_LENGTH);

    sprintf(upd_time, "%s, %2d %s %4d %2d:%2d:%2d GMT", day_arr[timenow->tm_wday],timenow->tm_mday, mon_array[timenow->tm_mon], timenow->tm_year+1900,timenow->tm_hour,timenow->tm_min,timenow->tm_sec);

    result = time_compare(cache_table[i].expiry, upd_time);
    return (result < 0) ? -1 : 0;
}



void error_msg(int status, int socket_fd)
{
    char message[1024];
    memset(message, 0, 1024);
    switch (status)
    {
        case 400:
            sprintf(message, "%s", "400 Bad Request");
            send(socket_fd, message, strlen(message), 0);
            break;

        case 404:
            sprintf(message, "%s", "404 Not Found");
            send(socket_fd, message, strlen(message), 0);
            break;

        default:
            break;
    }
}


int main(int argc, char *argv[])
{
    char buffer[1024];
    time_t now;
    struct tm *timenow;
    struct addrinfo hints;
    FILE *fp;
    int recvlen = -1;
    int server_fd,client_fd;
    struct sockaddr_in cliAddr;
    struct sockaddr_in addr;
    socklen_t sin_size;
    fd_set master;
    fd_set read_fds;
    int max_fd;
    int i;
    char http_buffer[BUFFER_SIZE], hostname[MAX_NAME_LENGTH], url[MAX_NAME_LENGTH], path[MAX_NAME_LENGTH];
    int cacheindex = -1;
    int port = 80;
    int status;
    int proxy_fd;
    int j,replace;
    FILE *readfp;
    char* expires=NULL;
    char *filename_replace = NULL;
    int readlen = 0;
    char modified[1024];
    char modified_request[BUFFER_SIZE];


    memset(cache_table, 0, 10 * sizeof(cache));
    if (argc<3)
    {
        printf("Insufficient arguments\n");
        exit(-1);
    }
    memset(&cliAddr, 0, sizeof cliAddr);
    memset(&addr, 0, sizeof addr);
    port = atoi(argv[2]);
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(argv[1]);
    addr.sin_family = AF_INET;

    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Cannot create socket");
        exit(-1);
    }
    if(bind(server_fd, (struct sockaddr*)&addr, sizeof(struct sockaddr)) < 0)
    {
        perror("Cannot bind socket");
        exit(-1);
    }
    if(listen(server_fd,10) < 0)
    {
        perror("listen");
        exit(-1);
    }

    FD_SET(server_fd, &master);
    max_fd = server_fd;
    memset(cache_table, 0, 10 * sizeof(cache)); //initialise cache table
    printf("Waiting for request!\n");
    sin_size = sizeof(cliAddr);
    while(1)
    {
        read_fds = master;
        if(select(max_fd+1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("Select Error: ");
            exit(-1);
        }
        for(i=0; i <=max_fd; i++)
        {
            if(FD_ISSET(i, &read_fds))
            {
                if(i == server_fd)
                {
                    client_fd = accept(server_fd, (struct sockaddr *)&cliAddr, &sin_size);
                    max_fd = client_fd > max_fd ? client_fd : max_fd;
                    FD_SET(client_fd, &master);
                    if (client_fd == -1)
                    {
                        perror("accept:");
                        continue;
                    }
                }
                else
                {
                    memset(http_buffer, 0, BUFFER_SIZE);
                    memset(hostname, 0, MAX_NAME_LENGTH);
                    memset(url, 0, MAX_NAME_LENGTH);
                    memset(path, 0, MAX_NAME_LENGTH);
                    client_fd = i;
                    if(recv(client_fd, http_buffer, sizeof(http_buffer), 0) < 0)
                    {
                        perror("recv");
                        close(client_fd);
                        return 1;
                    }
                    printf ("The client sent messages is\n%s \n", http_buffer);
                    if (format_request(http_buffer, hostname, &port, url, path) != 4)
                    {
                        error_msg(400,client_fd);
                        close(client_fd);
                        return 1;
                    }
                    if((proxy_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                    {
                        perror("proxysocket create");
                        close(client_fd);
                        return 1;
                    }
                    printf("getttingggg info.......");
                    memset(&hints, 0, sizeof(hints));
                    hints.ai_family = AF_INET;
                    hints.ai_socktype = SOCK_STREAM;
                    struct addrinfo *result = NULL;
                    printf("Host:%s\n",hostname);
                    if (getaddrinfo(hostname, "80", &hints, &result) != 0) {
                        printf("getaddrinfo error\n");
                        error_msg(404, client_fd); // Send a 404 response to the client
                        close(client_fd);
                        return 1;
                    }
                    printf("connecting to proxy.......");
                    if (connect(proxy_fd, result->ai_addr, result->ai_addrlen) < 0)
                    {
                        close(proxy_fd);
                        perror("connect error:");
                        error_msg(404, client_fd);
                        close(client_fd);
                        return 1;
                    }

                    time(&now);
                    timenow = gmtime(&now);

                    cacheindex = check_cache_entry(url);
                    if (cacheindex == -1)  // not found in cache
                    {
                        
                        memset(buffer, 0, 1024);
                        printf("File not found in cache..%s\n",hostname);
                        printf ("Sending HTTP query to web server\n");
                        sprintf(buffer, "GET /%s HTTP/1.0\r\nHost: %s\r\n\r\n", path,hostname);
                        puts(buffer);
                        if((status = send(proxy_fd, buffer, strlen(buffer), 0)) < 0)
                        {
                            perror("send");
                            close(proxy_fd);
                            return 1;
                        }
                        printf("Request sent to server.\n");
                        memset(http_buffer, 0, BUFFER_SIZE);
                        int replace=0;
                        printf("looking for replace");
                        for (int j = 0; j < 10; j++)
                        {
                            if(cache_table[j].filled == 0)
                            {
                               printf("replace%d",replace);
                               replace = j;
                                break;
                            }
                            else
                            {
                                if (time_compare(cache_table[j].last_modified,cache_table[replace].last_modified) <= 0)
                                {
                                   replace = j;
                                }
                            }
                        }
                        //printf("cache filling.....");
                        memset(&cache_table[replace], 0, sizeof(cache));
                        cache_table[replace].filled = 1;

                        filename_replace = strtok(path, "/");
                        int loop_counter = 0;
                        strcpy(cache_table[replace].filename, filename_replace);
                        //printf("URL:%s",url);
                        memcpy(cache_table[replace].url, url, MAX_NAME_LENGTH);
                        sprintf(cache_table[replace].last_modified, "%s, %02d %s %d %02d:%02d:%02d GMT", day_arr[timenow->tm_wday],timenow->tm_mday, mon_array[timenow->tm_mon], timenow->tm_year+1900,timenow->tm_hour,timenow->tm_min,timenow->tm_sec);
                       // printf("file to open %s",cache_table[replace].filename);
                        remove(cache_table[replace].filename);
                        fp=fopen(cache_table[replace].filename, "w");
                        //printf("Nexttttt....");
                        if (fp==NULL)
                        {
                            printf("failed to create cache.\n");
                            return 1;
                        }
                        while ((recvlen=recv(proxy_fd, http_buffer, BUFFER_SIZE, 0)) > 0)
                        {
                            //printf("Inside loop");
                            if(send(client_fd, http_buffer, recvlen, 0) < 0)
                            {
                                perror("Client send");
                                return 1;
                            }
                            fwrite(http_buffer, 1, recvlen, fp);
                            memset(http_buffer, 0, BUFFER_SIZE);
                        }
                        send(client_fd, http_buffer, 0, 0);
                        printf("Received successful response from server.\n");
                        fclose(fp);
                        readfp=fopen(cache_table[replace].filename,"r");
                        fread(http_buffer, 1, 2048, readfp);
                        fclose(readfp);
                        expires=strstr(http_buffer, "Expires:");
                        if (expires!=NULL)
                        {
                            memcpy(cache_table[replace].expiry, expires + 9, 29);
                        }
                        else
                        {
                            sprintf(cache_table[replace].expiry, "%s, %02d %s %4d %02d:%02d:%02d GMT", day_arr[timenow->tm_wday],timenow->tm_mday, mon_array[timenow->tm_mon], timenow->tm_year+1900,(timenow->tm_hour),(timenow->tm_min)+2,timenow->tm_sec);
                        }
                    }
                    else
                    {
                        if(cache_expiry(url,timenow)>=0)
                        {
                            printf("File found in cache and is not expired.\nSending file to the client...\n");
                            sprintf(cache_table[cacheindex].last_modified, "%s, %02d %s %4d %02d:%02d:%02d GMT", day_arr[timenow->tm_wday],timenow->tm_mday, mon_array[timenow->tm_mon], timenow->tm_year+1900,timenow->tm_hour,timenow->tm_min,timenow->tm_sec);
                            readfp=fopen(cache_table[cacheindex].filename,"r");
                            memset(http_buffer, 0, BUFFER_SIZE);
                            while ((readlen=fread(http_buffer, 1, BUFFER_SIZE, readfp)) > 0)
                            {
                                send(client_fd, http_buffer, readlen, 0);
                            }
                            printf("Sent file to client successfully\n");
                            fclose(readfp);
                        }
                        else
                        {
                            printf("File has expired, Requesting updated file from server.\n");
                            memset(modified, 0, 1024);
                            sprintf(modified, "GET /%s HTTP/1.0\r\nHost: %s\r\n\r\nIf-Modified-Since: %s\r\n\r\n", path,hostname,cache_table[cacheindex].last_modified);
                            memset(modified_request, 0, BUFFER_SIZE);
                            strcat(modified_request, modified);
                            printf ("Sending HTTP query to web server\n");
                            printf("%s\n",modified_request);
                            send(proxy_fd, modified_request, strlen(modified_request), 0);
                            memset(http_buffer, 0, BUFFER_SIZE);
                            recvlen = recv(proxy_fd, http_buffer, BUFFER_SIZE, 0);
                            expires=strstr(http_buffer, "Expires: ");
                            if (expires!=NULL)
                            {
                                memcpy(cache_table[cacheindex].expiry, expires+9, 29);
                            }
                            else
                            {
                                sprintf(cache_table[cacheindex].expiry, "%s, %02d %s %4d %02d:%02d:%02d GMT", day_arr[timenow->tm_wday],timenow->tm_mday, mon_array[timenow->tm_mon], timenow->tm_year+1900,timenow->tm_hour,timenow->tm_min,timenow->tm_sec);
                            }

                            if (recvlen > 0)
                            {
                                printf("Printing HTTP response \n %s\n",http_buffer);
                                if ((*(http_buffer + 9) == '3') && (*(http_buffer + 10) == '0') && (*(http_buffer + 11) == '4'))
                                {
                                    printf("File is still up to date. Sending file in cache.\n");
                                    readfp = fopen(cache_table[cacheindex].filename,"r");
                                    memset(http_buffer, 0, BUFFER_SIZE);
                                    while ((readlen=fread(http_buffer, 1, BUFFER_SIZE, readfp))>0)
                                    {
                                        send(client_fd, http_buffer, readlen, 0);
                                    }
                                    fclose(readfp);

                                }
                                else if((*(http_buffer + 9) == '4') && (*(http_buffer + 10) == '0') && (*(http_buffer + 11) == '4'))
                                {
                                    error_msg( 404, client_fd);
                                }
                                else if((*(http_buffer + 9) == '2') && (*(http_buffer + 10) == '0') && (*(http_buffer + 11) == '0'))
                                {
                                    printf("New file received from server.\nUpdating cache and sending file to client");
                                    send(client_fd, http_buffer, recvlen, 0);
                                    remove(cache_table[cacheindex].filename);

                                    expires = NULL;

                                    expires = strstr(http_buffer, "Expires: ");
                                    if (expires != NULL)
                                    {
                                        memcpy(cache_table[cacheindex].expiry, expires+9, 29);
                                    }
                                    else
                                    {
                                        sprintf(cache_table[cacheindex].expiry, "%s, %02d %s %4d %02d:%02d:%02d GMT", day_arr[timenow->tm_wday],timenow->tm_mday, mon_array[timenow->tm_mon], timenow->tm_year+1900,timenow->tm_hour,timenow->tm_min,timenow->tm_sec);
                                    }

                                    sprintf(cache_table[cacheindex].last_modified, "%s, %02d %s %4d %02d:%02d:%02d GMT", day_arr[timenow->tm_wday],timenow->tm_mday, mon_array[timenow->tm_mon], timenow->tm_year+1900,timenow->tm_hour,timenow->tm_min,timenow->tm_sec);
                                    fp=fopen(cache_table[cacheindex].filename, "w");
                                    fwrite(http_buffer, 1, recvlen, fp);

                                    memset(http_buffer, 0, BUFFER_SIZE);
                                    while ((recvlen = recv(proxy_fd, http_buffer, BUFFER_SIZE, 0)) > 0)
                                    {
                                        send(client_fd, http_buffer, recvlen, 0);
                                        fwrite(http_buffer, 1, recvlen, fp);
                                    }
                                    fclose(fp);

                                }
                            }
                            else
                                perror("receive:");


                        }
                    }
                    FD_CLR(client_fd,&master);
                    close(client_fd);
                    close(proxy_fd);
                    printf ("Printing Cache Table\n");
                    for (j = 0 ; j < 10 ; j++)
                    {
                        if (cache_table[j].filled)
                        {
                            printf ("\n");
                            printf ("URL                : %s\n", cache_table[j].url);
                            printf ("Last Access Time   : %s\n", cache_table[j].last_modified);
                            printf ("Expiry             : %s\n", cache_table[j].expiry);
                            printf ("File Name          : %s\n", cache_table[j].filename);
                            printf ("\n");
                        }
                    }
                }
            }
        }
    }
}