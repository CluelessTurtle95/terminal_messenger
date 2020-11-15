/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

#include<string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "hash_table.h"
#include "message.h"

// linked list functions
void free_list(CollisionList* list);
CollisionList* create_list_item ();
CollisionList* insert_list(CollisionList* list, item* element);
void print_list(CollisionList* list);
void * find_list(CollisionList* list, char * word);
CollisionList* remove_list(CollisionList* list, char * word, _Bool * removed);

char * list_session_to_string(CollisionList* list);
char * user_list_to_string(UserList * l);

// Hash key Function
long hash( char *str);

// https://stackoverflow.com/questions/7666509/hash-function-for-string
long hash( char *str)
{
    long hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

struct HashTable * hash_table_init(int s){
    struct HashTable * table;

    table = (struct HashTable *)malloc(sizeof(struct HashTable));
    
    table->size = s ; // change to another number
    table->count = 0;
    
    table->lists = (struct CollisionList **)malloc(table->size * sizeof(struct CollisionList *));
    for(int i = 0 ; i < table->size; i++){
        table->lists[i] = NULL;
    }
    
    return table;
}

_Bool insert_item(char * word, void * d, struct HashTable * table){

    // create an item
    item * element = (item *)malloc(sizeof(item));
    element->key = (char *)malloc( (strlen(word) + 1) * sizeof(char));
    strcpy(element->key , word );
    element->data = d;
    
    // insert
    int index = hash(word) ;
    index = (index & 0x7fffffff) % table->size;
    
    
    CollisionList * p = insert_list(table->lists[index] , element);
    if(p != NULL)
        table->lists[index] = p;
    else{
        return false;
    }
    table->count++;
    return true;
}

_Bool remove_item(char * word, HashTable * table){
    int index = hash(word);
    index = (index & 0x7fffffff) % table->size;
    
    _Bool removed;
    table->lists[index] = remove_list(table->lists[index], word, &removed);
    return removed;
}

void * find_item(char * word, HashTable * table){
    int index = hash(word);
    index = (index & 0x7fffffff) % table->size;
    
    return find_list(table->lists[index], word);
}

void print_table(struct HashTable *wc)
{
    for(int i = 0 ; i < wc->size; i++){
        if(wc->lists[i] != NULL){
            print_list(wc->lists[i]);
        }
            
    }
}

char * session_table_to_string(HashTable * table){
    char * buf_tot = (char *)malloc(5 * MAX);
    bzero(buf_tot, 5*MAX);
    
    for(int i = 0 ; i < table->size; i++){
        if(table->lists[i] != NULL){
            char * temp = list_session_to_string(table->lists[i]);
            if(i == 0)
                strcpy(buf_tot, temp);
            else
                strcat(buf_tot, temp);
            free(temp);
        }
            
    }
    return buf_tot;
}

void hash_table_destroy(struct HashTable *wc)
{
    for(int i = 0 ; i < wc->size; i++){
        if(wc->lists[i] != NULL){
            free_list(wc->lists[i]);
            wc->lists[i] = NULL;
        }
    }
    if(wc->lists != NULL)
        free(wc->lists);
    free(wc);
}

void print_list(CollisionList* list){
    CollisionList * temp = list;
    while(temp != NULL){
        printf("%s:%d\n" , temp->element->key , ((UserData *)temp->element->data)->connfd);
        temp = temp->next;
    }
}

char * user_list_to_string(UserList * l){
    char * buf = (char *)malloc(MAX);
    char * buf_tot = (char *)malloc(MAX);
    _Bool first = true;
    while(l != NULL){
        if(l->next != NULL)
            sprintf(buf, "%s, ", l->username);
        else
            sprintf(buf, "%s", l->username);
        if(first){
            first = false;
            strcpy(buf_tot, buf);
        }else{
            strcat(buf_tot, buf);
        }
        l = l->next;
    }
    free(buf);
    return buf_tot;
}

char * list_session_to_string(CollisionList* list){
    CollisionList * temp = list;
    char * buf = (char *)malloc(MAX);
    char * buf_tot = (char *)malloc(5 * MAX);
    _Bool first = true; 
    int i = 0;
    while(temp != NULL){
        i++;
        char * temp_str = user_list_to_string( ((SessionData *)temp->element->data)->connected_users );
        sprintf(buf,"Session ID:%6s Users::%s\n" , temp->element->key, temp_str);
        free(temp_str);
        if(first){
            first = false;
            strcpy(buf_tot, buf);
        }
        else {
            strcat(buf_tot, buf);
        }
        temp = temp->next;
    }
    free(buf);
    return buf_tot;
}
 
CollisionList* create_list_item () {
    // Allocates memory for a LinkedCollisionList pointer
    CollisionList* list = (CollisionList*) malloc (sizeof(CollisionList));
    return list;
}

CollisionList* remove_list(CollisionList* list, char * word, _Bool * removed){
    *removed = false;
    if(!list){
        // empty list, can't remove
        return NULL;
    }
    CollisionList* prev = NULL;
    CollisionList* temp = list;
    
    do{
        // check if item if found already
        if( strcmp(temp->element->key , word) == 0 ){
            // found
            // Same word exists, Remove
            if(prev == NULL){
                // first element
                list = temp->next;
            }
            else{
                prev->next = temp->next;
            }
            free(temp->element->key);
            free(temp->element->data);
            free(temp->element);
            *removed = true;
            return list;
        }
        // if not traverse through the list
        prev = temp;
        temp = temp->next;
    }while(temp != NULL);
    
    // temp points to last element in the list, element was not found
    return list;
}

void * find_list(CollisionList* list, char * word){
    if(!list){
        // empty list, can't find
        return NULL;
    }
    CollisionList* prev = NULL;
    CollisionList* temp = list;
    
    do{
        // check if item if found already
        if( strcmp(temp->element->key , word) == 0 ){
            // found
            // Same word exists
            
            return temp->element->data;
        }
        // if not traverse through the list
        prev = temp;
        temp = temp->next;
    }while(temp != NULL);
    
    // Not found
    return NULL;
}


CollisionList* insert_list(CollisionList* list, item* element) {
    // Check if list is empty
    if (!list) {
        CollisionList* head = create_list_item();
        head->element = element;
        head->next = NULL;
        list = head;
        return list;
    } 
     
    
    CollisionList* prev = list;
    CollisionList* temp = list->next;
    
    do{
        // check if item if found already
        if( strcmp(prev->element->key , element->key) == 0 ){
            // found
            // Same word exists, no collision
            // Same user already logged in
            
            free(element);
            return NULL;
        }
        
        // if not traverse through the list
        if(temp != NULL){
            prev = temp;
            temp = temp->next;
        }
        else{
            break;
        }
    }while(1);
    
    // temp points to last element in the list, element was not found
    // new item
    
    
    
    CollisionList* node = create_list_item();
    node->element = element;
    node->next = NULL;
    prev->next = node;
    
    //printf("Collision! Elements (list):\n");
    //print_list(list);
    
    return list;
}

void free_list(CollisionList* list) {
    CollisionList* prev = list;
    if(list == NULL){
        return;
    }
    while (list) {
        prev = list;
        list = list->next;
        free(prev->element->key);
        free(prev->element->data);
        free(prev->element);
        free(prev);
    }
}


