#include <stdbool.h>
#include <netdb.h> 
#include <unistd.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>

#include "message.h"
#include "hash_table.h"

#include <curses.h>

#define SIG_ABRT -10


typedef enum Command {
    Login, 
    Logout,
    Join,
    Leave,
    Create,
    List,
    Quit
} Command;

MessageList * controlList = NULL;
MessageList * userMessages= NULL;

_Bool forceLogout = false;

pthread_mutex_t controlMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t userMutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t conrolCond = PTHREAD_COND_INITIALIZER;
//pthread_cond_t userCond = PTHREAD_COND_INITIALIZER;


int currentOutputX = 0, currentOutputY = 0;
int currentFieldX = 0, currentFieldY = 0;

void addToMsgList(Message *m , MessageList *l){
    
    while(l->next != NULL){
        l = l->next;
    }
    l->next = (MessageList *)malloc(sizeof(MessageList));
    l->next->m = m;
    l->next->next = NULL;
}

Message * popControlList(){
    
    pthread_mutex_lock(&controlMutex);
    while(controlList->next == NULL){
        // Block
        pthread_cond_wait(&conrolCond, &controlMutex);
    }
    
    Message * res = controlList->next->m;
    
    controlList->next = controlList->next->next;
    
    pthread_mutex_unlock(&controlMutex);

    return res;
}
Message * popUserList(){
    
    pthread_mutex_lock(&userMutex);
    while(userMessages->next == NULL){
        // Non Block
        pthread_mutex_unlock(&userMutex);
        return NULL;
    }
    
    Message * res = userMessages->next->m;
    
    userMessages->next = userMessages->next->next;
    
    pthread_mutex_unlock(&userMutex);

    return res;
}

void freeMsgList(MessageList *l){
    MessageList * head = l->next;
    while(head != NULL){
        MessageList * temp = head->next;
        if(head->m->type == MESSAGE){
            //printw("Backlog Message| From: %8s Data: %s\n", head->m->source, head->m->data);
        }
        free(head->m);
        free(head);
        head = temp;
    }
}

void message_receiver_terminate_handler(int sig){
    // free list
    pthread_mutex_lock(&userMutex);
    freeMsgList(userMessages);
    pthread_mutex_unlock(&userMutex);

    pthread_mutex_lock(&controlMutex);
    freeMsgList(controlList);
    pthread_mutex_unlock(&controlMutex);

    // reset
    controlList->next = NULL;
    userMessages->next = NULL;
}

void display_messages(Message * received){
    int currY, currX;
    getyx(stdscr, currY, currX);

    if(currY == 0){
        // field
        move(currentOutputY, currentFieldX);

        printw("%s: %s\n", received->source, received->data );

        getyx(stdscr, currentOutputY, currentOutputX);

        move(currY, currX);
    }
    else{
        printw("Wrong Time!");
    }
}

void * message_receiver(void * sockfd_ptr){

    // attach a sig handler
    signal(SIG_ABRT, message_receiver_terminate_handler);

    int sockfd = *(int *)sockfd_ptr;
    char buff[MAX];
    
    while(1){
        bzero(buff, MAX); 
        int ret = read(sockfd, buff, MAX);
        if(ret <= 0)
            continue;
        struct Message* received = string_to_packet(buff);
        if (received->type == MESSAGE) { 
            pthread_mutex_lock(&userMutex);
            
            addToMsgList(received, userMessages);
            
            
            pthread_mutex_unlock(&userMutex);
        }else{
            
            pthread_mutex_lock(&controlMutex);
            if(received->type == EXIT){
                // final message, server exitted
                printw("Server Exited!\n");
                forceLogout = true;
            }
            
            addToMsgList(received, controlList);
            
            pthread_cond_signal(&conrolCond);
            
            pthread_mutex_unlock(&controlMutex);
        }
    }
    return sockfd_ptr;
}


int main() 
{
    
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, true);
    nodelay(stdscr, true);
    
    forceLogout = false;
    // base
    controlList = (MessageList *)malloc(sizeof(MessageList));
    userMessages = (MessageList *)malloc(sizeof(MessageList));
    
    // init
    controlList->m = NULL;
    controlList->next = NULL;
    userMessages->m = NULL;
    userMessages->next = NULL;
    
    int sockfd, connfd; 
    struct sockaddr_in servaddr; 

    pthread_t receiver_thread;

    _Bool quit = false;
    _Bool loged_in = false;
    char * username = NULL;
    
    
    while(!quit){
        
        Command c;
        char * command = (char *)malloc(50);
        char  * login_args [4];
        size_t len = 50;
        getyx(stdscr, currentOutputY, currentOutputX);
        if(currentOutputY == 0)
            currentOutputY = 2;
        move(0, 0);
        printw(">                                                                                                   ");
        move(0,1);
        
        char ch = getch();
        int i = 0;
        while(ch != '\n'){
            if(ch == ERR){
                // Not responded
                Message * m = popUserList();
                while(m != NULL){
                    display_messages(m);
                    m = popUserList();
                }
            }
            else if(ch == KEY_BACKSPACE || ch == 127 || ch == '\b' || ch == 7){
                
                if(i > 0){
                    i = i - 1;
                    move(0,i+1);
                    addch(' ');
                    move(0,i+1);
                }
                
            }
            else {
                command[i] = ch;
                i ++;
                // echo
                addch(ch);
            }
            ch = getch();
        }
        if(i == 0)
            continue;
        else{
            // for compatability, remove later
            command[i] = '\n';
            command[i+1] = '\0';
        }
//        getline(&command, &len, stdin);
//        printw("\nReceived : %s\n" , command);
        printw("\n==================Messages==================\n");
        move(currentOutputY, currentOutputX);
        
        if(forceLogout && loged_in){
            loged_in = false;
            pthread_kill(receiver_thread, SIG_ABRT);
            close(sockfd);
            printw("Forced Logout!\n");
            continue;
        }
        char * word_array= (char *)malloc(len +1);
        strncpy(word_array , command, len);
        char * word = strtok(word_array, " ");

        int word_count = 0;
        _Bool text = false; // Assume Command
        while (word != NULL)
        {
            if(strcmp(word, "") == 0){
                word = strtok (NULL, " ");
                continue;
            }
            // Handle word
            if(word_count == 0){ 
                if(word[0] == '/'){
                    // Is a command
                    if(strcmp(word , "/login") == 0){
                        c = Login;

                        // handle later
                    }
                    else if(strcmp(word , "/logout\n") == 0){
                        c = Logout;
                        
                        if(!loged_in){
                            printw("Not Logged in.\n");
                            break;
                        }
                        // handle
                        loged_in = false;
                        
                        text_message_from_source(sockfd, LOGOUT, "", username);
                        
                        pthread_kill(receiver_thread, SIG_ABRT);
                        close(sockfd);
                        // next command
                        break;
                    } 
                    else if(strcmp(word , "/joinsession") == 0){
                        c = Join;
                        
                        if(!loged_in){
                            printw("Not Logged in.\n");
                            break;
                        }
                        // handle later
                    } 
                    else if(strcmp(word , "/leavesession\n") == 0){
                        c = Leave;
                        
                        if(!loged_in){
                            printw("Not Logged in.\n");
                            break;
                        }
                        // send packets
                        text_message_from_source(sockfd, LEAVE_SESS, "", username);
                        
                        // handle
                        printw("left current session\n");
                        // next command
                        break;
                    } 
                    else if(strcmp(word , "/createsession") == 0){
                        c = Create;
                        
                        if(!loged_in){
                            printw("Not Logged in.\n");
                            break;
                        }
                        // handle later
                    } 
                    else if(strcmp(word , "/list\n") == 0){
                        c = List;
                        
                        if(!loged_in){
                            printw("Not Logged in.\n");
                            break;
                        }

                        text_message_from_source(sockfd, QUERY, "", username);
                        
                        Message * received = popControlList();
                        if (received->type == QU_ACK) { 
                            printw("Currently Active Sessions:\n%s", received->data);
                        } 
                        break;
                    } 
                    else if(strcmp(word , "/quit\n") == 0){
                        c = Quit;

                        // handle
                        if (loged_in == true){
                            text_message_from_source(sockfd, LOGOUT, "", username);
                        
                            loged_in = false;
                        }

                        quit = true;
                        
                        // next command
                        break;
                    } 
                    else {
                        printw("Invalid Command!\n");
                        break;
                    }
                }
                else {
                    // is text
                    text = true;
                    break;
                }
            }
            else{
                // Not first word
                if(text){
                    // 
                    printw("ERROR!");
                    exit(0);
                }
                else{
                    // handle command argument
                    if(c == Login){
                        if(word_count <= 5){
                            login_args[word_count -1] = (char *)malloc(50);
                            strcpy(login_args[word_count -1] , word);
                        }else{
                            printw("Invalid Arguments\n");
                        }
                    }
                    else if (c == Join){
                        if(word_count == 1){
                        // join with sess id word
                        // send/receive packets
                        if(word[strlen(word) -1] == '\n')
                            word[strlen(word) -1] = '\0';
                        text_message_from_source(sockfd, JOIN, word, username);
                        
                        Message * received = popControlList();
                        if (received->type == JN_ACK) {
                            printw ("Joined session: %s\n", word);
                        } 
                        else if (received->type == JN_NAK) {
                            if(strcmp(received->data, "leave_first") == 0)
                                printw ("Join failed: Leave current session first\n");
                            else
                                printw ("Join failed: session %s does not exist\n", word);
                        }
                        }
                        else {
                            printw("Invalid Arguments\n");
                        }
                    }
                    else if (c == Create) {
                        
                        if(word_count == 1){
                            // Session ID
                            // send/receive packets
                            if(word[strlen(word) -1] == '\n')
                                word[strlen(word) -1] = '\0';
                            text_message_from_source(sockfd, NEW_SESS, word, username);
         
                            Message * received = popControlList();
                            if (received->type == NS_ACK) { 
                                printw ("new session created\n");
                                printw ("session ID: %s\n", word);
                            } 
                            else if (received->type == NS_NACK) { 
                                printw ("session already exists\n");
                            } 
                            // create with sess id word
                        }
                        else {
                            printw("Invalid Arguments\n");
                        }
                    }
                    else {
                        printw("Invalid Arguments\n");
                    }
                }
            }
            
            // get next word
            word_count++;
            word = strtok (NULL, " ");
        }
        
        //  Only Login
        
        if (text){
            if(loged_in){
                if(command[strlen(command) -1] == '\n')
                    command[strlen(command) -1] = '\0';
                text_message_from_source(sockfd, MESSAGE, command, username);
         
            }else {
                printw("Login first!\n");
                break;
            }
        }
        else if(c == Login){
            if(loged_in){
                printw("Logout first!\n");
                continue;
            }
                
            if(word_count != 5){
                printw("Invalid Arguments!\n");
                continue;
            }
            // socket create and varification 
            sockfd = socket(AF_INET, SOCK_STREAM, 0); 
            if (sockfd == -1) { 
                    printw("Socket Creation Failed!\n"); 
                    continue;
            } 
            else
                    printw("Socket successfully created\n"); 

            pthread_create(&receiver_thread, NULL, message_receiver, &sockfd);
            bzero(&servaddr, sizeof(servaddr)); 

            // assign IP, PORT 
            servaddr.sin_family = AF_INET; 
            servaddr.sin_addr.s_addr = inet_addr(login_args[2]); 
            servaddr.sin_port = htons(atoi(login_args[3])); 

            // connect the client socket to server socket 
            if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) { 
                printw("Server Connection Failed!\n"); 
                close(sockfd);

                pthread_kill(receiver_thread, SIG_ABRT);
                continue;
            } 
            else{
                forceLogout = false;
                printw("Connected!\n"); 
            }
            // send/receive packets
            text_message_from_source(sockfd, LOGIN, login_args[1], login_args[0]);
            
            // wait for lo_ack or lo_nack

            Message * received;
            do{
                received = popControlList();
            }while(received->type == EXIT);
            
            if (received->type == LO_ACK) { 
                loged_in = true; 
                username = login_args[0];
                
                // Dont delete login_args 0
                continue; 
            } 
            else if(received->type == LO_NAK){
                loged_in = false;
                printw ("login failed, try again\n");
                continue;
            }
        } 
        // command done
        free(command);
        free(word_array);
    }

    // close the socket 
    close(sockfd); 
    
    
    endwin();
} 




