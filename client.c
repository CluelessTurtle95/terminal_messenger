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
    View,
    Invite,
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

typedef struct WindowData {
    WINDOW * win;
} WindowData;

HashTable * windowDB = NULL;
WindowData * sysWinData = NULL;


char current_sess[20];


void system_print(char * str, ...){
//    char * buf = (char *)malloc(MAX);
    
//    int y,x;
//    getyx(sysWinData->win, y,x);
//    wmove(sysWinData->win,0,0);
//    winstr(sysWinData->win, buf);
//    wprintw(sysWinData->win, "                    \n          \n             \n                ");
//    wmove(sysWinData->win,0,0);
//    wrefresh(sysWinData->win);
//    
//    wprintw(sysWinData->win, buf);
    va_list args;
    va_start(args, str);
    vw_printw(sysWinData->win, str, args);
    va_end(args);
    wrefresh(sysWinData->win);
    
    overwrite(sysWinData->win, stdscr);
    
    strcpy(current_sess, "System");
    
}

void session_print(char * user_sess, char * msg){
    char * user = (char *)malloc(MAX);
    char * session = (char *)malloc(MAX);
    sscanf(user_sess, "%s %s", user, session);
    WindowData * wd = find_item(session, windowDB);
    if(wd != NULL)
        wprintw(wd->win, "%s: %s\n", user, msg);
    else
        system_print("User Sess : %s Sess: %s", user_sess, session);
    
    free(user);
    free(session);
}


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
                int currY, currX;
                getyx(stdscr, currY, currX);

                // switch to output
                move(currentOutputY, currentFieldX);
                system_print( "Server Exited!\n");
                
                getyx(stdscr, currentOutputY, currentOutputX);
                // switch back
                move(currY, currX);
                
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
    strcpy(current_sess, "System");
    
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
    
    
    // Init windowDB
    windowDB = hash_table_init(10);
    sysWinData = (WindowData *)malloc(sizeof(WindowData));
    sysWinData->win = newwin(20, 40, 2, 0);
    insert_item("System", (void *)sysWinData, windowDB);
    
    while(!quit){
        
        Command c;
        char * command = (char *)malloc(50);
        char  * login_args [4];
        size_t len = 50;
        getyx(stdscr, currentOutputY, currentOutputX);
        if(currentOutputY == 0)
            currentOutputY = 2;
        move(1,0);
        printw("==================Messages(%s)==================", current_sess);
        move(0, 0);
        // TODO:get window size
        printw(">                                                                                                   ");
        move(0,1);
        
        char ch = getch();
        int i = 0;
        while(ch != '\n'){
            if(ch == ERR){
                // Not responded
                Message * m = popUserList();
                
                int currY, currX;
                getyx(stdscr, currY, currX);

                // switch to output
                move(currentOutputY, currentFieldX);

                while(m != NULL){
                    session_print(m->source, m->data);
//                    printw("%s: %s\n", m->source, m->data );
                    m = popUserList();
                }
                // check for logouts
                if(forceLogout && loged_in){
                    loged_in = false;
                    pthread_kill(receiver_thread, SIG_ABRT);
                    close(sockfd);
                    
                    system_print("Forced Logout!\n");
                    // put back cursor
                    getyx(stdscr, currentOutputY, currentOutputX);
                    move(currY, currX);
                    continue;
                }
                // put back cursor
                getyx(stdscr, currentOutputY, currentOutputX);
                move(currY, currX);
                
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
        move(currentOutputY, currentOutputX);
        
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
                            system_print("Not Logged in.\n");
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
                            system_print("Not Logged in.\n");
                            break;
                        }
                        // handle later
                    } 
                    else if(strcmp(word , "/leavesession") == 0){
                        c = Leave;
                        
                        if(!loged_in){
                            system_print("Not Logged in.\n");
                            break;
                        }
                        
                        // handle later
                        
                    } 
                    else if(strcmp(word , "/createsession") == 0){
                        c = Create;
                        
                        if(!loged_in){
                            system_print("Not Logged in.\n");
                            break;
                        }
                        // handle later
                    } 
                    else if(strcmp(word , "/list\n") == 0){
                        c = List;
                        
                        if(!loged_in){
                            system_print("Not Logged in.\n");
                            break;
                        }

                        text_message_from_source(sockfd, QUERY, "", username);
                        
                        Message * received = popControlList();
                        if (received->type == QU_ACK) { 
                            system_print("Currently Active Sessions:\n%s", received->data);
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
                    else if(strcmp(word, "/view") == 0){
                        c = View;
                        
                        if(!loged_in){
                            system_print("Not Logged in.\n");
                            break;
                        }
                        // handle later
                    }
                    else if(strcmp(word, "/invite") == 0){
                        c = Invite;
                        
                        if(!loged_in){
                            system_print("Not Logged in.\n");
                            break;
                        }
                        // handle later
                    }
                    else {
                        system_print("Invalid Command!\n");
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
                    system_print("ERROR!");
                    exit(0);
                }
                else{
                    // handle command argument
                    if(c == Login){
                        if(word_count <= 5){
                            login_args[word_count -1] = (char *)malloc(50);
                            strcpy(login_args[word_count -1] , word);
                        }else{
                            system_print("Invalid Arguments\n");
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
                                system_print("Joined session: %s\n", word);
                                
                                // make a window
                                WINDOW * w = newwin(20, 40, 2, 0);
                                WindowData * winData = (WindowData *)malloc(sizeof(WindowData));
                                winData->win = w;
                                
                                insert_item(word, (void *)winData, windowDB);
                            } 
                            else if (received->type == JN_NAK) {
                                if(strcmp(received->data, "already_joined") == 0)
                                    system_print("Join failed: Session already joined\n");
                                else
                                    system_print("Join failed: session %s does not exist\n", word);
                            }
                        }else {
                            system_print("Invalid Arguments\n");
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
                                system_print("new session created\n");
                                system_print("session ID: %s\n", word);
                            } 
                            else if (received->type == NS_NACK) { 
                                system_print("session already exists\n");
                            } 
                            // create with sess id word
                        }
                        else {
                            system_print("Invalid Arguments\n");
                        }
                    }
                    else if (c == Leave) {
                        // send packets
                        if(word_count == 1){
                            // Session ID
                            // send/receive packets
                            if(word[strlen(word) -1] == '\n')
                                word[strlen(word) -1] = '\0';
                            text_message_from_source(sockfd, LEAVE_SESS, word, username);
                            
                            Message * received = popControlList();
                            if(received->type == LEAVE_ACK){
                                system_print("Left %s session\n", word);
                                
                                WindowData * wd = find_item(word, windowDB);
                                delwin(wd->win);
                                remove_item(word, windowDB);
                                
                            }
                            else if(received->type == LEAVE_NAK)
                                system_print("Invalid Leave Request\n");
                            
                        }
                        else
                        {
                            system_print("Invalid Arguments\n");
                        }
                    }
                    else if (c == View){
                        if(word_count == 1){
                            if(word[strlen(word) -1] == '\n')
                                word[strlen(word) -1] = '\0';
                            WindowData * wd = find_item(word, windowDB);
                            if(wd != NULL){
                                // found
                                overwrite(wd->win, stdscr);
                                strcpy(current_sess, word);
                            }
                            else {
                                system_print( "Invalid Session\n");
                            }
                        }
                        else
                        {
                            system_print("Invalid Arguments\n");
                        }
                    }
                    else if(c == Invite){
                        // TODO
                        if(word_count == 1){
                            if(word[strlen(word) -1] == '\n')
                                word[strlen(word) -1] = '\0';
                            
                        }
                        else
                        {
                            system_print("Invalid Arguments\n");
                        }
                    }
                    else {
                        system_print("Invalid Arguments\n");
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
                system_print("Login first!\n");
                break;
            }
        }
        else if(c == Login){
            if(loged_in){
                system_print("Logout first!\n");
                continue;
            }
                
            if(word_count != 5){
                system_print("Invalid Arguments!\n");
                continue;
            }
            // socket create and varification 
            sockfd = socket(AF_INET, SOCK_STREAM, 0); 
            if (sockfd == -1) { 
                    system_print("Socket Creation Failed!\n"); 
                    continue;
            } 
            else
                    system_print("Socket successfully created\n"); 

            pthread_create(&receiver_thread, NULL, message_receiver, &sockfd);
            bzero(&servaddr, sizeof(servaddr)); 

            // assign IP, PORT 
            servaddr.sin_family = AF_INET; 
            servaddr.sin_addr.s_addr = inet_addr(login_args[2]); 
            servaddr.sin_port = htons(atoi(login_args[3])); 

            // connect the client socket to server socket 
            if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) { 
                system_print("Server Connection Failed!\n"); 
                close(sockfd);

                pthread_kill(receiver_thread, SIG_ABRT);
                continue;
            } 
            else{
                forceLogout = false;
                system_print("Connected!\n"); 
            }
            // send/receive packets
            text_message_from_source(sockfd, LOGIN, login_args[1], login_args[0]);
            
            // wait for lo_ack or lo_nack

            Message * received;
            // Ignore exits as not logged in anyway
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
                system_print("login failed, try again\n");
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