/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   hash_table.h
 * Author: sindhupa
 *
 * Created on November 13, 2020, 1:28 AM
 */

#ifndef HASH_TABLE_H
#define HASH_TABLE_H


typedef struct SessionList{
    char sessid[20];
    struct SessionList * next;
} SessionList;

typedef struct UserData {
    int connfd;
    pthread_t p;
    SessionList * sessions;
    char * username;
} UserData;

typedef struct UserList {
    char * username;
    struct UserList * next;
} UserList;

typedef struct SessionData {
    UserList * connected_users;
    
    // other info
} SessionData;


struct item {
    char * key;
    void * data;
};

typedef struct item item;

// linked list for collisions
struct CollisionList {
    item* element; 
    struct CollisionList* next;
};

typedef struct CollisionList CollisionList;


typedef struct HashTable {
    long size;
    long count;
    struct CollisionList ** lists;
} HashTable;

#define MAX 1024 

// hash table functions
struct HashTable * hash_table_init(int s);
_Bool insert_item(char * word, void * d, struct HashTable * table);

void print_table(struct HashTable *wc);
void hash_table_destroy(struct HashTable *wc);
char * session_table_to_string(HashTable * table);

_Bool remove_item(char * word, HashTable * table);
void * find_item(char * word, HashTable * table);

#endif /* HASH_TABLE_H */

