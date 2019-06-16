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
#ifndef FILE_LIST_H
#define FILE_LIST_H

#include <stddef.h>
#include <stdint.h>

typedef struct FileNode_s {
    struct FileNode_s *next;
    uint32_t offset;
    uint32_t size;
    char path[];
} FileNode;

typedef struct FileList_s {
    FileNode *head;
    FileNode *tail;
    uint32_t count;
} FileList;

void      file_list_add   (FileList *list, FileNode *node);
void      file_list_free  (FileList *list);
void      file_list_init  (FileList *list);
FileNode* file_list_remove(FileList *list, char *path);

FileNode* file_node_create       (char *path);
FileNode* file_node_create_size_n(size_t path_len);

#endif // ANF_IMAGES_H