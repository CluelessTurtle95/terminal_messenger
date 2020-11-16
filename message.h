/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   message.h
 * Author: sindhupa
 *
 * Created on November 12, 2020, 10:29 PM
 */

#ifndef MESSAGE_H
#define MESSAGE_H

typedef enum MessageType {
    INVALID = -1,
    LOGIN ,
    LO_ACK ,
    LO_NAK ,
    LOGOUT ,
    EXIT ,
    JOIN ,
    JN_ACK ,
    JN_NAK,
    LEAVE_SESS ,
    LEAVE_NAK ,
    LEAVE_ACK ,
    NEW_SESS ,
    NS_ACK ,
    NS_NACK ,
    MESSAGE ,
    QUERY ,
    QU_ACK,
    INVITE,
    IN_ACK,
    IN_NAK
} MessageType;

typedef struct Message {
    MessageType type;
    unsigned int size;
    char source[50];
    char data[1024];
    
} Message;


typedef struct MessageList{
    Message * m;
    struct MessageList * next;
} MessageList;

char * packet_to_string(Message * p);
Message * string_to_packet(char * str);

void text_message_from_source(int connfd, MessageType m, char * text, char * source);
void text_message(int connfd, MessageType m, char * text);
void empty_message(int connfd, MessageType m);

#endif /* MESSAGE_H */

