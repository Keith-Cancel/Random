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
#include <stdint.h>
#include <string.h>
#include <windows.h>

#include "file-list.h"

typedef struct Key {
   char     *str;
   unsigned length;
   unsigned pos;
} Key;

int pack(char *path, Key *key, int v);
int unpack(char *src, Key *key, int v);

int main(int argc, char **argv) {
    char null_key[2] = { 0x00, '\0' };
    char *src_file = NULL;
    Key key = { null_key, 1, 0};
    int p_flag  = 0;
    int verbose = 0;
    for(int i = 1; i < argc; i++) {
        char *cur = argv[i];
        if(cur[0] != '-') {
            if(src_file == NULL) {
                src_file = cur;
            } else {
                fprintf(stderr, "packer: error: source file name already specified.\n");
            }
            continue;
        }
        unsigned len = strlen(cur);
        if(len < 2) {
            fprintf(stderr, "packer: error: empty command line option ‘-’\n");
            continue;
        }
        if(cur[1] == 'p' && len == 2) {
            p_flag = 1;
            continue;
        }
        if(cur[1] == 'k' && len == 2) {
            if(i+1 >= argc) {
                fprintf(stderr, "packer: error: missing a key after ‘-k’\n");
                continue;
            }
            i++;
            if(key.str != null_key) {
                fprintf(stderr, "packer: error: a key has already been specified.\n");
            } else {
                key.length = strlen(argv[i]);
                key.str = argv[i];
            }
            continue;
        }
        if(cur[1] == 'v' && len == 2) {
            verbose = 1;
            continue;
        }
        fprintf(stderr, "packer: error: unrecognized command line option ‘%s’\n", cur);
    }
    if(src_file == NULL) {
        fprintf(stderr, "packer: fatal error: no input file/directory\n");
        return -1;
    }
    if(verbose) {
        printf("%s: ‘%s’\n", p_flag ? "Packing directory" : "Unpacking file", src_file);
        printf("Using the key: ‘%s’\n", key.str);
    }
    if(p_flag) {
        return pack(src_file, &key, verbose);
    } else {
        return unpack(src_file, &key, verbose);
    }
    return 0;
}

FILE* fopen_check(const char *filename, const char * mode) {
    FILE *fp = fopen(filename, mode);
    if(fp == NULL || ferror(fp)) {
        fprintf(stderr, "packer: fatal error: failed to open ‘%s’\n", filename);
        exit(-1);
    }
    setvbuf(fp, NULL, _IOFBF, 4096 * 8);
    return fp;
}

void* malloc_checked(size_t size) {
    void *tmp = malloc(size);
    if(tmp == NULL) {
        fprintf(stderr, "packer: fatal error: failed to allocate memory.\n");
        exit(-1);
    }
    memset(tmp, 0, size);
    return tmp;
}

char get_next_key_char(Key *key) {
    if(key->pos >= key->length) {
        key->pos = 0;
    }
    char val = key->str[key->pos];
    key->pos++;
    return val;
}

// To think this file format XORs all the data just because.
int fread_encoded(void * ptr, size_t size, size_t count, FILE * stream, Key *key) {
    int n = fread(ptr, size, count, stream);
    uint8_t *buf = ptr;
    for(size_t i = 0; i < n * size; i++) {
        buf[i] ^= get_next_key_char(key);
    }
    return n;
}

int fwrite_encoded(const void * ptr, size_t size, size_t count, FILE * stream, Key *key) {
    uint8_t *buf = malloc_checked(count * size);
    const uint8_t *data = ptr;
    for(size_t i = 0; i < count * size; i++) {
        buf[i] = data[i] ^ get_next_key_char(key);
    }
    int n = fwrite(buf, size, count, stream);
    free(buf);
    return n;
}
// copys n bytes from file to the other
int fcopy_n_encoded(FILE *dest, FILE *src, Key* key, size_t n) {
    const int buff_sz = 4096;
    int remains       = n;
    int total         = 0;
    uint8_t buffer[buff_sz];
    do {
        int read = fread(&buffer, 1, remains < buff_sz ? remains : buff_sz, src);
        if(read == 0) {
            break;
        }
        for(int i = 0; i < read; i++) {
            buffer[i] ^= get_next_key_char(key);
        }
        int wrote = fwrite(&buffer, 1, read, dest);
        if(wrote == 0) {
            break;
        }
        total   += wrote;
        remains -= wrote;
    } while(remains > 0);
    return total;
}
// Copys whole file to dest
int fcopy_encoded(FILE *dest, FILE *src, Key* key) {
    const int buff_sz = 4096;
    int total         = 0;
    uint8_t buffer[buff_sz];
    while(1) {
        int read = fread(&buffer, 1, buff_sz, src);
        if(read == 0) {
            break;
        }
        for(int i = 0; i < read; i++) {
            buffer[i] ^= get_next_key_char(key);
        }
        int wrote = fwrite(&buffer, 1, read, dest);
        if(wrote == 0) {
            break;
        }
        total += wrote;
    }
    return total;
}

int fwrite_uint32_encoded(FILE *dest, uint32_t val, Key* key) {
    int total = 0;
    for(int i = 0; i < 4; i++) {
        uint8_t byte = 0xFF & ( val >> ( i * 8 ) );
        byte ^= get_next_key_char(key);
        if(fwrite(&byte, 1, 1, dest) == 0) {
            break;
        }
        total++;
    }
    return total;
}

int fread_uint32_encoded(FILE *src, uint32_t *val, Key* key) {
    uint32_t tmp = 0;
    uint8_t byte = 0;
    int    total = 0;
    for(int i = 0; i < 4; i++) {
        if(fread(&byte, 1, 1, src) == 0) {
            break;
        }
        char k = get_next_key_char(key);
        byte ^= k;
        tmp |= byte << (i * 8);
        total++;
    }
    *val = tmp;
    return total;
}

void dir_get_parent(char *path) {
    char *sep = path;
    do {
        if(*path == '/' || *path == '\\') {
            sep = path;
        }
        path++;
    } while(*path);
    *sep = '\0';
}

int dir_remove_extension(char *path) {
    int len = strlen(path);
    int ext_len = 0;
    while(ext_len < len) {
        ext_len++;
        char cur = path[len - ext_len];
        if(cur == '.') {
            break;
        }
        if(cur == '/' || cur == '\\' || ext_len == len) {
            ext_len = 0;
            break;
        }
    }
    len -= ext_len;
    path[len] = '\0';
    return 0;
}

int dir_create_recursive(char *path) {
    char *ptr = path;
    do {
        char cur = *ptr;
        if(cur == '/' || cur == '\\') {
            *ptr = '\0';
        }
        if(CreateDirectory(path, NULL) == ERROR_PATH_NOT_FOUND) {
            return 1;
        }
        *ptr = cur;
        ptr++;
    } while(*ptr);
    return 0;
}

#define MANIP_BUFF_SZ 2048

int unpack(char *src, Key *key, int verbose) {
    int base_sz      = strlen(src) + 1;
    char *manip_buff = malloc_checked(MANIP_BUFF_SZ);
    char base[base_sz];
    memcpy(base, src, base_sz);
    dir_remove_extension(base);
    if(verbose) {
        printf("Creating directory ‘%s’\n", base);
    }
    if(dir_create_recursive(base)) {
        fprintf(stderr, "packer: fatal error: Failed to create directory.\n");
        return -1;
    };
    FILE *fp = fopen_check(src, "rb");
    // check file magic bytes.
    char buf[4];
    if(fread_encoded(buf, 1, 4, fp, key) != 4) {
        fprintf(stderr, "packer: fatal error: This is not a pack file.\n");
        return -1;
    }
    char magic_bytes[4] = {'p', 'a', 'c', 'k'};
    for(int i = 0; i < 4; i++) {
        if(buf[i] != magic_bytes[i]) {
            fprintf(stderr, "packer: fatal error: Either the key is wrong or this is not a pack file.\n");
            return -1;
        }
    }
    // Read header
    uint32_t files      = 0;
    uint32_t ignore_len = 0;
    if(fread_uint32_encoded(fp, &files, key) != 4 || fread_uint32_encoded(fp, &ignore_len, key) != 4) {
        fprintf(stderr, "packer: fatal error: Too small to be a pack file.\n");
        return -1;
    }
    if(verbose) {
        printf("Contains %u files.\n", files);
        printf("Ignore Header Size: %u bytes\n", ignore_len);
    }
    if(ignore_len > 0) {
        if(verbose) {
            printf("Writing Ignore Header.\n");
        }
        snprintf(manip_buff, MANIP_BUFF_SZ, "%s/%s", base, "__ignore_header__");
        FILE *ignore = fopen_check(manip_buff, "wb");
        fcopy_n_encoded(ignore, fp, key, ignore_len);
        fclose(ignore);
    }
    FileList list;
    file_list_init(&list);
    for(size_t i = 0; i < files; i++) {
        uint32_t path_len, size, offset;
        if(
            fread_uint32_encoded(fp, &path_len, key) != 4 ||
            fread_uint32_encoded(fp, &size, key) != 4 ||
            fread_uint32_encoded(fp, &offset, key) != 4
        ) {
            fprintf(stderr, "packer: fatal error: This Pack file is corrupted.\n");
            return -1;
        }
        FileNode *tmp = file_node_create_size_n(path_len);
        tmp->size   = size;
        tmp->offset = offset;
        if(fread_encoded(tmp->path, 1, path_len, fp, key) != (int)path_len) {
            fprintf(stderr, "packer: fatal error: The Pack file is corrupted.\n");
            return -1;
        }
        file_list_add(&list, tmp);
    }
    FileNode *cur = list.head;
    while(cur != NULL) {
        snprintf(manip_buff, MANIP_BUFF_SZ, "%s/%s", base, cur->path);
        dir_get_parent(manip_buff);
        if(verbose) {
            printf("Creating directory ‘%s’\n", manip_buff);
        }
        dir_create_recursive(manip_buff);
        snprintf(manip_buff, MANIP_BUFF_SZ, "%s/%s", base, cur->path);
        if(verbose) {
            printf("Creating file ‘%s’\n", manip_buff);
        }
        fseek(fp, cur->offset, SEEK_SET);
        FILE *file = fopen_check(manip_buff, "wb");
        if(fcopy_n_encoded(file, fp, key, cur->size) != (int)cur->size) {
            fprintf(stderr, "packer: fatal error: Failed to write all of ‘%s’.\n", manip_buff);
            return -1;
        }
        fclose(file);
        cur = cur->next;
    }
    file_list_free(&list);
    free(manip_buff);
    return 0;
}

int path_is_dir(char *path) {
    int len = strlen(path);
    DWORD dwAttrib = GetFileAttributesA(path);
    int result = (dwAttrib != INVALID_FILE_ATTRIBUTES);
    result     = result && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY);
    result    &= (path[len-1] != '/') && (path[len-1] != '\\');
    return result;
}

int file_exists(char *path) {
    DWORD dwAttrib = GetFileAttributesA(path);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES) ? 1 : 0;
}

int get_file_list(FileList *list, const char *base, const char *sub) {
    size_t base_len    = strlen(base);
    size_t sub_len     = strlen(sub);
    // Handle empty strings
    char   base_sep[2] = { '\0', '\0' };
    char   sub_sep[2]  = { '\0', '\0' };
    // Place seperator after path if longer than zero.
    base_sep[0] = base_len > 0 ? '/' : '\0';
    sub_sep[0]  = sub_len  > 0 ? '/' : '\0';
    size_t byte_len = base_len + sub_len + 4; // max possible size
    char   full_path[byte_len];
    snprintf(full_path, byte_len, "%s%s%s%s*", base, base_sep, sub, sub_sep);
    // Start search of path
    WIN32_FIND_DATAA fdFile;
    HANDLE find = FindFirstFileA(full_path, &fdFile);
    if(find == INVALID_HANDLE_VALUE) {
        return 0;
    }
    do {
        // Skip these more of like logical files.
        if(strcmp(fdFile.cFileName, ".") == 0 || strcmp(fdFile.cFileName, "..") == 0) {
            continue;
        }
        size_t name_sz = sub_len + strlen(fdFile.cFileName) + 2;
        char name[name_sz];
        snprintf(name, name_sz, "%s%s%s", sub, sub_sep, fdFile.cFileName);
        // Enter Sub-directories
        if(fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            get_file_list(list, base, name);
            continue;
        }
        // Add file to list
        FileNode *tmp = file_node_create_size_n(strlen(name));
        tmp->size = fdFile.nFileSizeLow;
        strcpy(tmp->path, name);
        file_list_add(list, tmp);
    } while(FindNextFileA(find, &fdFile));
    FindClose(find);
    return 1;
}

int pack(char *path, Key *key, int verbose) {
    if(!path_is_dir(path)) {
        fprintf(stderr, "packer: fatal error: Not a directory: ‘%s’\n", path);
        return -1;
    }
    FileList list;
    file_list_init(&list);
    get_file_list(&list, path, "");
    FileNode *ignore = file_list_remove(&list, "__ignore_header__");
    if(verbose) {
        printf("Files to pack: %d\n", list.count);
    }
    size_t path_len = strlen(path);
    char name[path_len + 10];
    for(unsigned i = 0; i < 100; i++) {
        if(i == 0) {
            snprintf(name, path_len + 10, "%s.pack", path);
        } else {
            snprintf(name, path_len + 10, "%s(%u).pack", path, i);
        }
        if(!file_exists(name)) {
            break;
        }
        if(i >= 99) {
            fprintf(stderr, "packer: fatal error: over a 100 name conflicts: ‘%s’\n", name);
            return -1;
        }
    }
    // Calculate size of the first offset
    size_t first_offset = 12 + 12 * list.count;
    uint32_t ignore_sz  = 0;
    FileNode *cur = list.head;
    while(cur != NULL) {
        first_offset += strlen(cur->path);
        cur = cur->next;
    }
    if(ignore != NULL) {
        if(verbose) {
            printf("Has a ‘__ignore_header__’\n");
        }
        first_offset += ignore->size;
        ignore_sz     = ignore->size;
    }
    // Begin File Creation
    if(verbose) {
        printf("Creating pack file ‘%s’\n", name);
    }
    FILE *pk       = fopen_check(name, "wb");
    fwrite_encoded("pack", 1, 4, pk, key);
    fwrite_uint32_encoded(pk, list.count, key);
    fwrite_uint32_encoded(pk, ignore_sz, key);
    if(ignore != NULL) {
        char pth[path_len + 19];
        // Should probably handle the seperator like I do in get_file_list.
        snprintf(pth, path_len + 19, "%s/__ignore_header__", path);
        FILE *tmp = fopen_check(pth, "rb");
        fcopy_encoded(pk, tmp, key);
        fclose(tmp);
        free(ignore);
    }
    // Write file list
    cur = list.head;
    uint32_t cur_offset = first_offset;
    while(cur != NULL) {
        fwrite_uint32_encoded(pk, strlen(cur->path), key);
        fwrite_uint32_encoded(pk, cur->size, key);
        fwrite_uint32_encoded(pk, cur_offset, key);
        fwrite_encoded(cur->path, 1, strlen(cur->path), pk, key);
        cur_offset += cur->size;
        cur = cur->next;
    }
    // Write Files
    cur = list.head;
    while(cur != NULL) {
        if(verbose) {
            printf("Adding: %s\n", cur->path);
        }
        char pth[path_len + strlen(cur->path) + 2];
        // Should probably handle the seperator like I do in get_file_list.
        snprintf(pth, path_len + strlen(cur->path) + 2, "%s/%s", path, cur->path);
        FILE *tmp = fopen_check(pth, "rb");
        fcopy_encoded(pk, tmp, key);
        fclose(tmp);
        cur = cur->next;
    }
    fclose(pk);
    file_list_free(&list);
    return 0;
}
