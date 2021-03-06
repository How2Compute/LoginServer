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
#include <errno.h>
#include <time.h>
// Networking specific includes
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <signal.h>
// MySQL
#include <mysql.h>
// Multithreading
#include <pthread.h>
#include <semaphore.h>
// Crypto
#include <openssl/sha.h>
# include <openssl/rand.h>

// Parameters
#define CONNECTIONBACKLOG 5
#define MAXUSERS 5

// Function Prototpyes
void sigchld_handler(int s);
void *get_in_addr(struct sockaddr *sa);
int parse(char *source, char **username, char **password);
int LoginUser(MYSQL *connection, MYSQL_STMT *stmt_connection, char *username, char *password, /* OUT */char **sessionID);
void MySQLQueryFail_Handler(MYSQL *connection);

// Just here for now
int tokencounter;

int main(int argc, char *argv[])
{
    // Check if the porgram was run with the correct command line arguements
    if(argc != 2)
    {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        return -1;
    }
    
    fprintf(stdout, "Wildcard Server Starting Up!\n");
    fprintf(stdout, "Wildcard Server Version: %s running on port %s\nMySQL Version: %s\n", "0.0.1", argv[1], mysql_get_client_info());
    
    tokencounter = 0;
    
    struct addrinfo hints, *res;
    struct sigaction sa;
    struct sockaddr_storage cli_addr;
    socklen_t addr_len;
    
    char s[INET6_ADDRSTRLEN];
    
    int sock_fd, cli_fd;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    // The MySQL *object*
    MYSQL *databasecon;
    MYSQL_STMT *stmt_databasecon;
    
    /*
    * MySQL Login data!
    */
    
    char *mysqluser = "LoginServer";
    char *mysqlpasswd = "jXLYAuuFxCA7g5gGG5BdzSMb";
    
    // Attempt to initialize the the MySQL object
    if((databasecon = mysql_init(NULL)) == NULL)
    {
        fprintf(stderr, "Error initializing database connection: %s\n", mysql_error(databasecon));
        return -20;
    }
    
    // Connect to the MySQL database
    if(mysql_real_connect(databasecon, "localhost", mysqluser, mysqlpasswd, "Accounts", 0, NULL, 0) == NULL)
    {
        fprintf(stderr, "Error connecting to database: %s\n", mysql_error(databasecon));
        mysql_close(databasecon);
        return -21;
    }
    
    if((stmt_databasecon = mysql_stmt_init(databasecon)) == NULL)
    {
        fprintf(stderr, "Error creating stmt object: %s\n", mysql_error(databasecon));
        mysql_close(databasecon);
        return -22;
    }
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
                close(sock_fd);
                return -12;
            }
            else
            {
                if(listen(sock_fd, CONNECTIONBACKLOG) == -1)
                {
                    perror("Error starting listener");
                    close(sock_fd);
                    return -13;
                }
                else    
                {
                    sa.sa_handler = sigchld_handler;
                    sigemptyset(&sa.sa_mask);
                    sa.sa_flags = SA_RESTART;
                    if(sigaction(SIGCHLD, &sa, NULL) == -1)
                    {
                        perror("Error working with sigaction");
                        return -14;
                    }
                    while(1)
                    {
                        addr_len = sizeof(cli_addr);
                        cli_fd = accept(sock_fd, (struct sockaddr *)&cli_addr, &addr_len);
                        
                        if(cli_fd == -1)
                        {
                            perror("Error accepting connection");
                        }
                        
                        inet_ntop(cli_addr.ss_family, get_in_addr((struct sockaddr *)&cli_addr), s, sizeof(s));
                        fprintf(stdout, "Recieved connection from: %s\n", s);
                        
                        if(!fork())
                        {
                            // TODO evaluate if this is indeed a safe/the best way to do it!
                            while(1)
                            {
                                int buffsize = sizeof(char) * 1024;
                                char *recvbuff = malloc(buffsize);
                                int bytesRecieved = 0;
                                
                                if((bytesRecieved = recv(cli_fd, recvbuff, buffsize, 0)) == -1)
                                {
                                    perror("Error recieving data");
                                    // Close the socket; The client shall have to open a new one for a new request
                                    break;
                                }
                                else
                                {
                                    // Copy it to a \0 terminated string
                                    char *buff = malloc((bytesRecieved + 1) * sizeof(char));
                                    
                                    // Copy all of the read characters
                                    for(int i = 0; i < bytesRecieved; i++)
                                    {
                                        buff[i] = recvbuff[i];
                                    }
                                    
                                    // Null terminate the string (to avoid buffer overflow)
                                    buff[bytesRecieved] = '\0';
                                    
                                    //printf("Recieved (%i bytes): %s\n", bytesRecieved, buff);
                                    char *username;
                                    char *password;
                                    char *token;
                                    
                                    if(parse(buff, &username, &password))
                                    {
                                        if(LoginUser(databasecon, stmt_databasecon, username, password, &token))
                                        {
                                            // The user successfully logged in! TODO: return token too
                                            //char *returnmsg = malloc(strlen("OKE_") + strlen(token) + 1);
                                            char *returnmsg = "OKE_123\0";
                                            if(send(cli_fd, returnmsg, strlen(returnmsg), 0) == -1)
                                            {
                                                perror("Error sending login confimration");
                                            }
                                            
                                            // If it successfully sent off the message, exit the loop
                                            break;
                                        }
                                        else
                                        {
                                            // The users credentials whre invalid
                                            if(send(cli_fd, "NOK", sizeof("NOK"), 0) == -1)
                                            {
                                                perror("Error sending login error");
                                            }
                                            
                                            // If it successfully sent off the message, exit the loop
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        // The parse failed so return NOK
                                        if(send(cli_fd, "NOK", sizeof("NOK"), 0) == -1)
                                        {
                                            perror("Error sending parse error");
                                        }
                                        
                                        // If it successfully sent off the message, exit the loop
                                        break;
                                    }
                                }
                            }
                            
                            // Close the connection to the client
                            close(cli_fd);
                        }
                    }
                }
            }
        }
    }
}

void sigchld_handler(int s)
{
    int saved_errno = errno;
    
    while(waitpid(-1, NULL, WNOHANG) > 0);
    
    errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa)
{
    if(sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }
    else
    {
        return &(((struct sockaddr_in6 *)sa)->sin6_addr);
    }
}

int parse(char *source, char **username, char **password)
{
    // Init a variable to 0 to keep track of where in the parse we are
    int stage = 0;
    
    // Make a buff to store part of the parse
    char buff[1024];
    // To avoid overflow
    int buffused = 0;
    
    for(int i = 0, n = strlen(source); i < n; i++)
    {
        // Should it switch stage? AKA did this part of the parse finish?
        if(source[i] == '\n')
        {
            if(stage == 0)
            {
                // Allocate the right ammount of memory to username
                *username = malloc((buffused + 1) * sizeof(char));

                // Copy the buffer to the username (cant use strcpy as buff isn't null terminated)
                for(int i = 0; i < buffused; i++)
                {
                    (*username)[i] = buff[i];
                }
                
                // Null-terminate the string
                (*username)[buffused] = '\0';
                
            }
            else if(stage == 1)
            {
                // Allocate the right ammount of memory to username
                *password = malloc((buffused + 1) * sizeof(char));
                
                // Copy the buffer to the password (cant use strcpy as buff isn't null terminated)
                for(int i = 0; i < buffused; i++)
                {
                    (*password)[i] = buff[i];
                }
                
                // Null-terminate the string
                (*password)[buffused] = '\0';
            }
            else
            {
                // Something didn't go quite right!
                stage++;
                break;
            }
            
            // Reset the buffer that has been used and increase the stage #
            buffused = 0;
            stage++;
        }
        else
        {
            // Copy the current character to the buffer and increment the buffer size
            buff[buffused] = source[i];
            buffused++;
        }
    }
    
    // Was the parse valid? (where there too many or too little stages? If so it failed)
    if(stage != 1)
    {
        return 0;
    }
    // Does it still need to process the password?
    else if(stage == 1)
    {
        // Allocate the right ammount of memory to username
        *password = malloc((buffused + 1) * sizeof(char));
        
        // Copy the buffer to the password (cant use strcpy as buff isn't null terminated)
        for(int i = 0; i < buffused; i++)
        {
            (*password)[i] = buff[i];
        }
        
        // Null-terminate the string
        (*password)[buffused] = '\0';
    }
    
    // Otherwise assume everything went fine
    return 1;
}

void MySQLQueryFail_Handler(MYSQL *connection)
{
  fprintf(stderr, "Error while executing query: %s\n", mysql_error(connection));
  mysql_close(connection);
  exit(-23);        
}

int LoginUser(MYSQL *connection, MYSQL_STMT *stmt_connection, char *username, char *password, /* OUT */char **sessionID)
{
    /* TODO fix this
    // TODO make this a prepared statement!!! And use the the below escape real etc
    // mysql_real_escape_string(username);

    char *stmtquery = malloc(strlen("SELECT * FROM PlayerAccounts WHERE UserName = \"?\"") * sizeof(char) + strlen(username) * sizeof(char) + sizeof(char));
    sprintf(stmtquery, "SELECT * FROM PlayerAccounts WHERE UserName = \"?\"");
    
    unsigned long length = strlen(stmtquery);
    
    if(mysql_stmt_prepare(stmt_connection, stmtquery, length) != 0)
    {
        fprintf(stderr, "Error preparing prepared statement: %s\n", mysql_stmt_error(stmt_connection));
    }
    
    if(mysql_stmt_param_count(stmt_connection) != 1)
    {
        fprintf(stderr, "Params: %lu\n", mysql_stmt_param_count(stmt_connection));
        // Too many or too little parameters setup!
        //fprintf(stderr, "Error preparing prepared statement: Invalid ammount of ?'s in statement!\n");
    }
    
    // Setup the bind params
    MYSQL_BIND queryVars[1];
    
    memset(&queryVars, 0, sizeof(queryVars));
    
    unsigned long strlength = strlen(username);
    
    queryVars[1].buffer_type = MYSQL_TYPE_STRING;
    queryVars[1].buffer = username;
    queryVars[1].buffer_length = strlen(username);
    queryVars[1].is_null = 0;
    queryVars[1].length = &strlength;
    exit(-100);
    */
    
    // TODO replace this!!! Buffer overflow!!!
    char *tokenbuffer = malloc(1024);
    
    sprintf(tokenbuffer, "%i", tokencounter);
    
    tokencounter++;
    
    // TODO replace the * with only the required stuff
    // Create a variable to store the query and format the query
    char *query = malloc(strlen("SELECT * FROM PlayerAccounts WHERE UserName = \"\"`") * sizeof(char) + strlen(username) * sizeof(char) + sizeof(char));
    sprintf(query, "SELECT * FROM PlayerAccounts WHERE UserName = \"%s\"", username);
    
    // Look the user up in the database
    if(mysql_query(connection, query))
    {
        MySQLQueryFail_Handler(connection);
    }
    
    MYSQL_RES *entries = mysql_store_result(connection);
    
    if(entries == NULL)
    {
        MySQLQueryFail_Handler(connection);
    }
    
    int numFields = mysql_num_fields(entries);
    MYSQL_ROW entry;
    
    while((entry = mysql_fetch_row(entries)))
    {
        // Are the passwords different sizes?
        if(strlen(entry[2]) < strlen(password))
        {
            // Then it can't be the correct password.
            return 0;
        }
        
        // entry[2] is the password row. TODO make this encrypted/hashed
        if(strncmp(entry[2], password, strlen(entry[2])) == 0)
        {
            // The user login was correct.
            
            // Insert the random token
            char *query2 = malloc(strlen("UPDATE PlayerAccounts SET SessionID=\"\" WHERE UserName = \"\"") * sizeof(char) + strlen(username) * sizeof(char) + strlen(username) * sizeof(char) + sizeof(char));
            sprintf(query2, "UPDATE PlayerAccounts SET SessionID=\"%s\" WHERE UserName = \"%s\"", tokenbuffer, username);
            
            // Attempt to insert
            // Look the user up in the database
            if(mysql_query(connection, query2))
            {
                MySQLQueryFail_Handler(connection);
            }
            
            *sessionID = tokenbuffer;
            
            return 1;
        }
        else
        {
            // The users password was incorrect.
            // But if there are more records for this user? Continue through those.
        }
    }
    
    // The username was not found!
    return 0;
    
}