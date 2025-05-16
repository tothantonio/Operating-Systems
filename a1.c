#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#define MAGIC_VALUE "Nn1J"
struct __attribute__((packed)) section_header{
    char sect_name[7];
    int sect_type;
    int sect_offset;
    int sect_size;
};

struct header{
    int version;
    int no_of_sections;
    struct section_header *section_headers;
    short header_size;
    char magic[4];
};

void free_header(struct header *header) {
    if (header && header->section_headers) {
        free(header->section_headers);
        header->section_headers = NULL;
    }
}

struct header parse(const char* path){
    struct header header = {0};
    header.section_headers = NULL;

    int fd = open(path, O_RDONLY);

    if(fd == -1){
        header.version = -5;
        return header;
    }

    lseek(fd, -4, SEEK_END);
    read(fd, header.magic, 4);

    if(strncmp(header.magic, MAGIC_VALUE, 4) != 0) {
        close(fd);
        header.version = -1;
        return header;
    }

    lseek(fd, -6, SEEK_END);
    read(fd, &header.header_size, 2);

    lseek(fd, -header.header_size, SEEK_END);

    read(fd, &header.version, 4);

    if(header.version < 31 || header.version > 75){
        close(fd);
        header.version = -2;
        return header;
    }

    read(fd, &header.no_of_sections, 1);
    if(header.no_of_sections != 2 && (header.no_of_sections < 8 || header.no_of_sections > 14)) {
        close(fd);
        header.version = -3;
        return header;
    }

    header.section_headers = (struct section_header*) malloc(header.no_of_sections * sizeof(struct section_header));

    if(!header.section_headers || 
       read(fd, header.section_headers, header.no_of_sections * sizeof(struct section_header)) 
       != header.no_of_sections * sizeof(struct section_header)) {
        close(fd);
        header.version = -3;
        free_header(&header);
        return header;
    }

    for(int i = 0; i < header.no_of_sections; i++) {
        if(header.section_headers[i].sect_type != 90 && 
           header.section_headers[i].sect_type != 13 && 
           header.section_headers[i].sect_type != 82 && 
           header.section_headers[i].sect_type != 39 && 
           header.section_headers[i].sect_type != 81) {
            close(fd);
            header.version = -4;
            free_header(&header);
            return header;
        }
    }

    close(fd);
    return header;
}
void print_header(struct header header) {
    printf("SUCCESS\n");
    printf("version=%d\n", header.version);
    printf("nr_sections=%d\n", header.no_of_sections);

    for(int i = 0; i < header.no_of_sections; i++) {
        char name[8] = {0};
        strncpy(name, header.section_headers[i].sect_name, 7);
        printf("section%d: %s %d %d\n", i + 1, name, header.section_headers[i].sect_type, header.section_headers[i].sect_size);
    }
}
void listDir(const char* path, const int rec, const long sizeThreshold, const char* name_ends_with){
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    char fullPath[1024];
    struct stat statBuf;
    static int firstCall = 1;

    dir = opendir(path);
    if(dir == NULL){
        printf("ERROR\ninvalid directory path\n");
        return;
    }

    if(firstCall){
        printf("SUCCESS\n");
        firstCall = 0;
    }

    while((entry = readdir(dir)) != NULL){
        if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0){
            snprintf(fullPath, 1024, "%s/%s", path, entry->d_name);
            if(lstat(fullPath, &statBuf) == 0){
                if((name_ends_with[0] == 0 || strcmp(entry->d_name + (strlen(entry->d_name) - strlen(name_ends_with)), name_ends_with) == 0) && 
                (sizeThreshold == -1 || (S_ISREG(statBuf.st_mode) && statBuf.st_size < sizeThreshold))) {
                    printf("%s\n", fullPath);
                }

                if(rec && S_ISDIR(statBuf.st_mode)){
                    listDir(fullPath, rec, sizeThreshold, name_ends_with);
                }
            }
        }
    }
    closedir(dir);
}

void extract(const char* path, int section, int line){
    struct header header = {0};
    int fd = -1;
    char *buffer = NULL;

    header = parse(path);
    
    if (header.version < 0) {
        printf("ERROR\ninvalid file\n");
        return;
    }

    if (section < 1 || section > header.no_of_sections) {
        printf("ERROR\ninvalid section\n");
        free_header(&header);
        return;
    }

    struct section_header sec = header.section_headers[section - 1];

    fd = open(path, O_RDONLY);
    if (fd == -1) {
        printf("ERROR\ninvalid file\n");
        free_header(&header);
        return;
    }

    lseek(fd, sec.sect_offset, SEEK_SET);

    buffer = (char*)malloc(sec.sect_size + 1);
    if (!buffer) {
        printf("ERROR\nmemory allocation failed\n");
        close(fd);
        free_header(&header);
        return;
    }

    ssize_t bytes_read = read(fd, buffer, sec.sect_size);
    close(fd);

    if (bytes_read != sec.sect_size) {
        printf("ERROR\nread failed\n");
        free(buffer);
        free_header(&header);
        return;
    }

    buffer[sec.sect_size] = '\0';

    char *start = buffer;
    char *end = NULL;
    int current_line = 1;

    for (int i = 0; i < sec.sect_size; i++) {
        if (buffer[i] == '\n' || i == sec.sect_size - 1) {
            if (current_line == line) {
                end = &buffer[i];
                break;
            }
            start = &buffer[i + 1];
            current_line++;
        }
    }

    if (!end || current_line < line) {
        printf("ERROR\ninvalid line\n");
        free(buffer);
        free_header(&header);
        return;
    }

    printf("SUCCESS\n");
    for (char *ptr = end - 1; ptr >= start; ptr--) {
        printf("%c", *ptr);
    }
    printf("\n");

    free(buffer);
    free_header(&header);
}

void findall(const char* path){
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    char fullPath[512];
    struct stat statbuf;
    static int firstCall = 1;

    dir = opendir(path);
    if(dir == NULL){
        printf("ERROR\ninvalid directory path\n");
        return;
    }

    if(firstCall){
        printf("SUCCESS\n");
        firstCall = 0;
    }

    while((entry = readdir(dir)) != NULL){
        if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0){
            snprintf(fullPath, 512, "%s/%s", path, entry->d_name);
            if(lstat(fullPath, &statbuf) == 0){
                if(S_ISDIR(statbuf.st_mode)){
                    findall(fullPath);
                } else if(S_ISREG(statbuf.st_mode)){
                    struct header header = parse(fullPath);
                    if(header.version >= 0){
                        int ok = 1;
                        for(int i = 0; i < header.no_of_sections; i++){
                            if(header.section_headers[i].sect_size > 1416){
                                ok = 0;
                                break;
                            }
                        }
                        if(ok){
                            printf("%s\n", fullPath);
                        }
                    }
                    free_header(&header);
                }
            }
        }
    }
    closedir(dir);
}

int main(int argc, char **argv) 
{
    if(argc >= 2) {
        if(strcmp(argv[1], "variant") == 0) {
            printf("75664\n");
        }
        else if(strcmp(argv[1], "list") == 0){
            char *path = NULL;
            int rec = 0;
            long sizeThreshold = -1;
            char name_ends_with[1024] = {0};
            for(int i = 2; i < argc; i++){
                if(strcmp(argv[i], "recursive") == 0){
                    rec = 1;
                }
                if(strncmp(argv[i], "path=", 5) == 0){
                    path = argv[i] + 5;
                    break;
                }
                if(strncmp(argv[i], "size_smaller=", 13) == 0){
                    sizeThreshold = atol(argv[i] + 13);
                }
                if(strncmp(argv[i], "name_ends_with=", 15) == 0) {
                    strcpy(name_ends_with, argv[i] + 15);
                }
            }
            listDir(path, rec, sizeThreshold, name_ends_with);
        }
        else if(strcmp(argv[1], "parse") == 0){
            char *path = NULL;
            for(int i = 2; i < argc; i++){
                if(strncmp(argv[i], "path=", 5) == 0){
                    path = argv[i] + 5;
                    break;
                }
            }
            struct header header = parse(path);
            if(header.version == -1){
                printf("ERROR\nwrong magic\n");
            } 
            else if(header.version == -2){
                printf("ERROR\nwrong version\n");
            }
            else if(header.version == -3){
                printf("ERROR\nwrong sect_nr\n");
            }
            else if(header.version == -4){
                printf("ERROR\nwrong sect_types\n");   
            }
            else {
                print_header(header);
            }
            if(header.section_headers != NULL){
                free_header(&header);
            }
        }
        else if(strcmp(argv[1], "extract") == 0){
            char *path = NULL;
            int section = -1;
            int line = -1;
            for(int i = 2; i < argc; i++){
                if(strncmp(argv[i], "path=", 5) == 0){
                    path = argv[i] + 5;
                }
                if(strncmp(argv[i], "section=", 8) == 0){
                    section = atoi(argv[i] + 8);
                }
                if(strncmp(argv[i], "line=", 5) == 0){
                    line = atoi(argv[i] + 5);
                }
            }
            if(path && section != -1 && line != -1){
                extract(path, section, line);
            } else printf("ERROR\ninvalid arguments\n");
        }
        else if(strcmp(argv[1], "findall") == 0){
            char *path = NULL;
            for(int i = 2; i < argc; i++){
                if(strncmp(argv[i], "path=", 5) == 0){
                    path = argv[i] + 5;
                    break;
                }
            }
            if(path){
                findall(path);
            } else printf("ERROR\ninvalid arguments\n");
        }
    }
    return 0;
}
