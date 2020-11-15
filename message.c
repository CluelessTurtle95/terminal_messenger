/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include<unistd.h>

#include "message.h"
#include "hash_table.h"

char * packet_to_string(Message * p){
    char * result = (char *)malloc(strlen(p->source) +  strlen(p->data) + 20);
    
    char * temp = (char *)malloc(strlen(p->source) +  strlen(p->data));
    strcpy(temp, p->source);
    strcat(temp, p->data);
    
    // Header
    sprintf(result , "%d:%d:%s(" , p->type , p->size , temp);
    
    return result;
}

Message * string_to_packet(char * str){
    Message * newP  = (Message *)malloc(sizeof(Message));
    char * buf= (char *)malloc(MAX);
    sscanf(str , "%d:%d:%[^(]", &newP->type , &newP->size, buf);
    if(strlen(buf) - newP->size < 0){
        // Invalid Packet
        strcpy(newP->source, "INVALID");
        strcpy(newP->data , buf); 
        return newP;
    }
    strncpy(newP->source , buf, strlen(buf) - newP->size );
    newP->source[strlen(buf) - newP->size] = '\0';
    strcpy(newP->data , buf + strlen(buf) - newP->size );
    return newP;
}


void empty_message(int connfd, MessageType m){
    struct Message* mess = (Message *)malloc(sizeof(Message));
    mess->size = 0;
    mess->type = m;
    strcpy(mess->source, "");
    strcpy(mess->data, "");
    char* sending_string = packet_to_string(mess);
    write(connfd, sending_string, strlen(sending_string));
    free(sending_string);
    free(mess);
}

void text_message(int connfd, MessageType m, char * text){
    struct Message* mess = (Message *)malloc(sizeof(Message));
    mess->size = strlen(text);
    mess->type = m;
    strcpy(mess->source, "");
    strcpy(mess->data, text);
    char* sending_string = packet_to_string(mess);
    write(connfd, sending_string, strlen(sending_string));
    free(sending_string);
    free(mess);
}

void text_message_from_source(int connfd, MessageType m, char * text, char * source){
    struct Message* mess = (Message *)malloc(sizeof(Message));
    mess->size = strlen(text);
    mess->type = m;
    strcpy(mess->source, source);
    strcpy(mess->data, text);
    char* sending_string = packet_to_string(mess);
    write(connfd, sending_string, strlen(sending_string));
    free(sending_string);
    free(mess);
}