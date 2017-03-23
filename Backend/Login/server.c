/*
* Login server for WildCard
* Alpha Version 0.0.1
* Written by Alpha-V
*/

// Standard includes
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
// Networking specific includes
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

// Multithreading
#include <pthread.h>
#include <semaphore.h>

// Parameters
#define CONNECTIONBACKLOG 5
#define MAXUSERS 5

int main(int argc, char *argv[])
{
    // Check if the porgram was run with the correct command line arguements
    if(argc != 2)
    {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        return -1;
    }
    
    fprintf(stdout, "Wildcard Server Starting Up!\n");
    
    struct addrinfo hints, *res;
    int sock_fd;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    if(getaddrinfo(NULL, argv[1], &hints, &res) != 0)
    {
        perror("Error getting server address info");
        return -10;
    }
    else
    {
        if((sock_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
        {
            perror("Error creating socket");
            return -11;
        }
        else
        {
            if(bind(sock_fd, res->ai_addr, res->ai_addrlen) == -1)
            {
                perror("Error binding socket");
                return -12;
            }
            else
            {
                if(listen(sock_fd, CONNECTIONBACKLOG) == -1)
                {
                    perror("Error starting listener");
                }
                else
                {
                    // Start accepting connections
                }
            }
        }
    }
}