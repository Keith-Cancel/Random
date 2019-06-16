/*
MIT License
Copyright (c) 2019 Keith J. Cancel
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file-list.h"

void file_list_init(FileList *list) {
    list->count = 0;
    list->head  = NULL;
    // first member of struct is always at index zero regardless of packing slop.
    list->tail = (FileNode*)&list->head;
}

void file_list_add(FileList *list, FileNode *node) {
    list->tail->next = node;
    list->tail = node;
    list->count++;
}

void file_list_free(FileList *list) {
    FileNode *cur = list->head;
    while(cur != NULL) {
        FileNode *tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    file_list_init(list);
}

FileNode* file_list_remove(FileList *list, char *path) {
    FileNode* cur = list->head;
    FileNode* prev = NULL;
    while(cur != NULL && strcmp(path, cur->path) != 0) {
        prev = cur;
        cur  = cur->next;
    }
    if(cur == NULL) {
        return NULL;
    }
    if(prev == NULL) {
        list->head = cur->next;
    } else {
        prev->next = cur->next;
    }
    list->count--;
    cur->next = NULL;
    return cur;
}

FileNode* file_node_create(char *path) {
    return file_node_create_size_n(strlen(path));
}

FileNode* file_node_create_size_n(size_t path_len) {
    FileNode *tmp = malloc(sizeof(FileNode) + path_len + 1);
    if(tmp == NULL) {
        fprintf(stderr, "packer: fatal error: failed to allocate memory for file node.\n");
        exit(-1);
    }
    memset(tmp->path, '\0', path_len + 1);
    tmp->next   = NULL;
    tmp->offset = 0;
    tmp->size   = 0;
    return tmp;
}
