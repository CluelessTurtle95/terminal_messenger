//#include "packet.h"
//
//#define ERROR -1

#include <stdio.h> 
#include<unistd.h>
#include <netdb.h> 
#include <netinet/in.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <sys/types.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include "message.h" 
#include "hash_table.h"
#include <sched.h>
#include <signal.h>
#define ERROR -1

char * authenticate(UserData * d);
_Bool checkDB(Message * m);
void * newUserHandler(void * data);

_Bool remove_from_session(char * sessid, char * username);
_Bool logout(UserData * data);

void multicast(Message * m, char * source, char * sessid);

//Prepare Global Login,Session DB
HashTable * loginDB = NULL;
HashTable * sessionDB = NULL;
    
pthread_mutex_t loginDBMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sessionDBMutex = PTHREAD_MUTEX_INITIALIZER;

void exit_server_handler(int sig){
    // Send exit to all logged in
    
    printf("Forced Shutdown!\nExit sent to : \n");
    for(int i = 0 ; i < loginDB->size; i++){
        if(loginDB->lists[i] != NULL){
            CollisionList * temp = loginDB->lists[i];
            while(temp != NULL){
                int sockfd = ((UserData *)temp->element->data)->connfd;
                empty_message(sockfd, EXIT);
                printf("%s\n", temp->element->key);
                temp = temp->next;
            }
        }
    }
    
    exit(0);
}

int main(int argc, char *argv[]) 
{ 
    if(argc != 2){
        printf ("Wrong input!\n");
        exit(ERROR);
    }
      
    // Making the socket
    // IPv4, TCP, Any protocol(ip?)
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    // Checking if socket allocation succeded
    if ( sockfd < 0 ) { 
        printf("socket creation failed"); 
        exit(ERROR); 
    } 
    
    struct sockaddr_in serverAddr, clientAddr; 
    
    // Setting all fields to empty
    memset(&serverAddr, 0, sizeof(serverAddr)); 
    memset(&clientAddr, 0, sizeof(clientAddr)); 
      
    // setting server info
    serverAddr.sin_family    = AF_INET; // IPv4 
    serverAddr.sin_port = htons(atoi(argv[1])); // port number from user
    serverAddr.sin_addr.s_addr = INADDR_ANY; 

    //bind the socket to a port and check if successful
    if ( bind(sockfd, (const struct sockaddr *)&serverAddr,  
            sizeof(serverAddr)) < 0 ) 
    { 
        printf("unable to bind!\n"); 
        exit(ERROR); 
    }
    
    // Max users 50, Sessions 10
    loginDB = hash_table_init(50);
    sessionDB = hash_table_init(10);
    
    signal(SIGINT, exit_server_handler);
    signal(SIGTERM, exit_server_handler);
    signal(SIGABRT, exit_server_handler);
    
    
    // start listening
    while(true)
    {    
        // Now server is ready to listen and verification 
        if ((listen(sockfd, 5)) != 0) { 
                printf("Listen failed...\n"); 
                exit(0); 
        } 
        else
                printf("Server listening..\n"); 
        int len = sizeof(clientAddr); 

        // Accept the data packet from client and verification 
        int connfd = accept(sockfd, ( struct sockaddr *)&clientAddr, &len); 
        if (connfd < 0) { 
                printf("server acccept failed...\n"); 
                exit(0); 
        } 
        else
                printf("server acccept the client...\n"); 
        
        UserData * data = (UserData *)malloc(sizeof(UserData));
        data->connfd = connfd;
        // No username yet;
        data->username = NULL;
        // No active sessions yet
        data->sessions = NULL;
        
        pthread_create( &(data->p), NULL, newUserHandler, (void *)data);
        
        
    }
    // After chatting close the socket 
    close(sockfd); 
} 

_Bool logout(UserData * data){
    // remove from table
    _Bool removeSess = true;
    while(data->sessions != NULL){
        removeSess = remove_from_session(data->sessions->sessid, data->username);
        SessionList * temp = data->sessions;
        data->sessions = data->sessions->next;
        free(temp);
    }
    pthread_mutex_lock(&loginDBMutex);
    _Bool removeLogin = remove_item(data->username, loginDB);
    pthread_mutex_unlock(&loginDBMutex);

    if(removeLogin != removeSess)
        printf("Incomplete Logout!\n");
    return removeSess && removeLogin ;
}

// only edits sessionDB
_Bool remove_from_session(char * sessid, char * username){
    _Bool done = false;
    
    pthread_mutex_lock(&sessionDBMutex);
    SessionData * sessData = find_item(sessid, sessionDB);
    
    if(sessData == NULL){
        printf("Invalid SessionID!\n");
        pthread_mutex_unlock(&sessionDBMutex);
        return false;
    }
                
    UserList * head = sessData->connected_users;

    if(head == NULL){
        printf("Empty Session!\n");
        pthread_mutex_unlock(&sessionDBMutex);
        return false;
    }

    UserList * prev = NULL;
    while(head != NULL){
        if(strcmp(head->username, username) == 0){
            // found element, remove
            if(prev == NULL){
                // first element
                sessData->connected_users = sessData->connected_users->next;
            }
            else {
                prev->next = head->next;
            }
            free(head->username);
            free(head);
            done = true;
            break;
        }
        prev = head;
        head = head->next;
    }
    
    pthread_mutex_unlock(&sessionDBMutex);
    return done;
}

// Send message to all users in a session
void multicast(Message * m, char * source, char * sessid){
    pthread_mutex_lock(&sessionDBMutex);
    SessionData * sessData = find_item(sessid, sessionDB);
    UserList * head = sessData->connected_users;
    while(head != NULL){
        pthread_mutex_unlock(&loginDBMutex);
        int temp_sock = ((UserData *)find_item(head->username, loginDB))->connfd;
        pthread_mutex_unlock(&loginDBMutex);

        text_message_from_source(temp_sock, MESSAGE, m->data, source);
        head = head->next;
    }
    pthread_mutex_unlock(&sessionDBMutex);    
}

void handleUserRequests( UserData * data){
    _Bool waiting = false;
    clock_t start,end;
    
    while(1){
        char * buf = (char *)malloc(MAX);
        bzero(buf, MAX);
        int ret = read(data->connfd, buf, MAX);
        if(ret == 0){
            if(!waiting){
                waiting = true;
                start = clock();
                continue;
            }else{
                // Already waiting
                clock_t end = clock();
    
                float seconds = (float)(end - start) / CLOCKS_PER_SEC ;
                if(seconds > 10){
                    // timeout
                    printf("Client Timeout!\n");
                    printf("Logging out user %s\n", data->username);
                    logout(data);
                    int ret = ERROR;
                    free(buf);
                    pthread_exit(&ret);
                }
                
                sched_yield();
                free(buf);
                continue;
            }
        }
        else if(ret > 0){
            // Out of waiting
            waiting = false;
            
        }
        else if(ret < 0){
            printf("Client Unexpected Error!\n");
            printf("Logging out user %s\n", data->username);
            logout(data);
            int ret = ERROR;
            free(buf);
            pthread_exit(&ret);
        }
        Message * m = string_to_packet(buf);
        if(strcmp(m->source, "INVALID") == 0){
            printf("Received Invalid Packet!\n");
            printf("Packet Data: %s\n" , m->data);
            
            printf("Logging out user %s\n", data->username);
            logout(data);
            int ret = ERROR;
            free(buf);
            pthread_exit(&ret);
        }
        else if (m->type == MESSAGE){
            printf("User %s Sent : %s\n", data->username, m->data);
            
            SessionList * temp = data->sessions;
            while(temp != NULL){
                // Modify message to reflect session
                char * username_session = (char *)malloc(MAX);
                sprintf(username_session, "%s %s", data->username, temp->sessid);
                
                // Send
                multicast(m, username_session, temp->sessid);
                temp = temp->next;
                free(username_session);
            }
        } 
        else if(m->type == LOGIN){
            printf("Double Login attempt!\n");
            empty_message(data->connfd, LO_NAK);
        } 
        else if(m->type == LOGOUT){
            
            printf("Logging out User %s\n", data->username);
            
            _Bool done = logout(data);
            
            if(!done)
                printf("Invalid Logout!\n");
            
            printf("Now Logged In : \n");
            print_table(loginDB);
            
            free(m);
            free(buf);
            int ret = 0;
            pthread_exit(&ret);
        } 
        else if(m->type == QUERY){
            
            char * text = session_table_to_string(sessionDB);
            text_message(data->connfd, QU_ACK, text);
            free(text);
            
        }
        else if(m->type == NEW_SESS){
            SessionData * sessData = (SessionData *)malloc(sizeof(SessionData));
            // No users at first
            sessData->connected_users = NULL;
            
            pthread_mutex_lock(&sessionDBMutex);
            _Bool done = insert_item(m->data, sessData,sessionDB);
            pthread_mutex_unlock(&sessionDBMutex);
            
            if(!done){
                printf("Session Creation Failed!\n");
                empty_message(data->connfd, NS_NACK);
            }
            else {
                printf("New session Created!\n");
            
                empty_message(data->connfd, NS_ACK);
            }
                
            
        } 
        else if(m->type == JOIN){
            pthread_mutex_lock(&sessionDBMutex);
            SessionData * sessData = find_item(m->data, sessionDB);
            
            if(sessData == NULL){
                // session doesn't exist, send nack
                empty_message(data->connfd, JN_NAK);
            }
            else{
                _Bool already_exists = false;

                // exists, Join
                if(sessData->connected_users == NULL){
                    // No users
                    sessData->connected_users = (UserList *)malloc(sizeof(UserList));
                    sessData->connected_users->next = NULL;
                    sessData->connected_users->username = (char *)malloc(MAX);
                    strcpy(sessData->connected_users->username , data->username);

                }
                else{
                    UserList * head = sessData->connected_users;

                    while(head->next != NULL){
                        if(strcmp(head->username, data->username) == 0){
                            // user already exists in session
                            already_exists = true;
                        }
                        head = head->next;
                    }

                    if(strcmp(head->username, data->username) == 0){
                            // user already exists in session
                            already_exists = true;
                    }
                    if(!already_exists){
                        // Head points to last element now
                        head->next = (UserList *)malloc(sizeof(UserList));
                        head->next->next = NULL;
                        head->next->username = (char *)malloc(MAX);
                        strcpy(head->next->username, data->username);
                    }
                }

                if(!already_exists){
                    // set user session
                    SessionList * temp = (SessionList *)malloc(sizeof(SessionList));
                    temp->next = NULL;
                    strcpy(temp->sessid, m->data);
                    SessionList * head = data->sessions;
                    SessionList * prev = NULL;
                    while(head != NULL){

                        prev = head;
                        head = head->next;

                    }
                    if(prev == NULL){
                        // first element
                        data->sessions = temp;
                    }
                    else{
                        prev->next = temp;
                    }
                    // Send Ack
                    empty_message(data->connfd, JN_ACK);
                }
                else {
                    text_message(data->connfd, JN_NAK, "already_joined");
                }
            }
            pthread_mutex_unlock(&sessionDBMutex);
            
        }
        else if(m->type == LEAVE_SESS){
            if(data->sessions == NULL){
                // Not in session
                empty_message(data->connfd, LEAVE_NAK);
            }
            else{
                // check if valid session
                SessionList * head = data->sessions;
                SessionList * prev = NULL;
                _Bool isvalid = false;
                
                while(head != NULL){
                    if(strcmp(head->sessid, m->data) == 0){
                        // found session , valid
                        isvalid = true;
                        break;
                    }
                    prev = head;
                    head = head->next;

                }
                if(!isvalid){
                    // send nack
                    empty_message(data->connfd, LEAVE_NAK);
                    
                }
                else{
                    // remove from sessions
                    remove_from_session(head->sessid, data->username);
                    
                    // remove from userdata
                    if(prev != NULL)
                        prev->next = head->next;
                    else
                        data->sessions = NULL;
                    free(head);
                    
                    // send ack
                    empty_message(data->connfd, LEAVE_ACK);
                }
            }
        }
        else {
            
            printf("Not Implemented! %d %d\n", m->type, ret);
        }
        free(buf);
    }
}

// All new users begin here
void * newUserHandler(void * data){
    UserData * d = (UserData *)data;
    d->username = authenticate(d);
    if(d->username != NULL){
        // valid user
        
        //printf("Now Logged In : \n");
        //print_table(loginDB);
        
        handleUserRequests(d);
    } 
    else{
        printf("Failed Login attempt!\n");
    }
    return data;
}

char * authenticate(UserData *d)
{
    int connfd = d->connfd;
    char * buf  = (char *)malloc(sizeof(char) * 1024); 
    
    bzero(buf, MAX);
    
    int total = read(connfd, buf, MAX);
    
    Message * m = string_to_packet(buf);

    _Bool done = false;
    _Bool isValidUser = checkDB(m);
    // check if already loged in
    if(isValidUser){
        pthread_mutex_lock(&loginDBMutex);
        done = insert_item(m->source, d, loginDB);
        pthread_mutex_unlock(&loginDBMutex);
        if(!done){
            printf("User already logged in!\n");
        }
    }
    if(done){
        
        // send Ack
        empty_message(connfd, LO_ACK);
        free(buf);
        buf = (char *)malloc(MAX);
        strcpy(buf, m->source);
        return buf;
    }
    else{
        // send no ack
        
        empty_message(connfd, LO_NAK);
        free(buf);
        return NULL;
    }
    
    free(buf);
    return NULL;
}

_Bool checkDB(Message * m){
    // Declare the file pointer 
    FILE *filePointer ; 
    filePointer = fopen("UserDB", "r") ; 
      
    if ( filePointer == NULL ) 
    { 
        printf( "No User Database found!(Not accessible)" ) ; 
        return false;
    } 
    char * username = (char *)malloc(MAX);
    char * password = (char *)malloc(MAX);
    while (true){
        
        int res = fscanf(filePointer, "%s %s\n", username, password); 
        
        
        if( res == EOF)
            break;

        if(strcmp(username, m->source) == 0){
            // User found
            printf("user found!\n");
            if(strcmp(password, m->data) == 0){
                printf("authenticated!\n");
                fclose(filePointer);
                return true;
            }
            else{
                // wrong password
                fclose(filePointer);
                return false;
            }
        }
    }
    // Not found
    fclose(filePointer) ; 
    return false;      
    
}
