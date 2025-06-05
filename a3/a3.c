#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define RESP_PIPE "RESP_PIPE_75664"
#define REQ_PIPE "REQ_PIPE_75664"

const unsigned int VERSION = 75664;

int main(){

    if(mkfifo(RESP_PIPE, 0644) != 0){
        perror("ERROR\ncannot create the response pipe\n");
        return 1;
    }

    int fd_req = open(REQ_PIPE, O_RDONLY);

    if(fd_req == -1){
        perror("ERROR\ncannot open the request pipe");
        return 1;
    }

    int fd_resp = open(RESP_PIPE, O_WRONLY);

    if(fd_resp == -1){
        perror("ERROR\ncannot open the response pipe");
        return 1;
    }

    char* src = "BEGIN!";

    write(fd_resp, src, strlen(src));
    printf("SUCCESS\n");

    unsigned int shm_size;
    unsigned int file_size;
    int shmFD;
    int fd;
    volatile char* sharedChar = NULL;
    char* file = NULL;
    for(;;){
        char dst[250];
        for(int i = 0; i < 250; i++){
            read(fd_req, &dst[i], 1);
            if(dst[i] == '!' || i == 249){
                dst[i] = 0;
                break;
            }
        }
        char var[] = "PING!";
        if(strcmp(dst, "PING") == 0){
            write(fd_resp, &var, strlen(var));
            write(fd_resp, &VERSION, sizeof(VERSION));
            write(fd_resp, "PONG!", strlen("PONG!"));
        } else if(strcmp(dst, "CREATE_SHM") == 0){
            read(fd_req, &shm_size, sizeof(unsigned int));
            shmFD = shm_open("/yTJuDV", O_CREAT | O_RDWR, 0664);
            if(shmFD < 0){
                write(fd_resp, "CREATE_SHM!ERROR!", strlen("CREATE_SHM!ERROR!"));
            }
            ftruncate(shmFD, shm_size);
            sharedChar = (volatile char*)mmap(0, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmFD, 0);

            if(sharedChar == (void*)-1){
                write(fd_resp, "CREATE_SHM!ERROR!", strlen("CREATE_SHM!ERROR!"));
            }
            
            write(fd_resp, "CREATE_SHM!SUCCESS!", strlen("CREATE_SHM!SUCCESS!"));
        } else if(strcmp(dst, "WRITE_TO_SHM") == 0){
            unsigned int offset = 0;
            unsigned int value = 0;

            read(fd_req, &offset, sizeof(unsigned int));
            read(fd_req, &value, sizeof(unsigned int));

            if(offset >= 0 && (offset + sizeof(unsigned int)) <= shm_size){
                lseek(shmFD, offset, SEEK_SET);
                write(shmFD, &value, sizeof(unsigned int));
                lseek(shmFD, 0, SEEK_SET);
                write(fd_resp, "WRITE_TO_SHM!SUCCESS!", strlen("WRITE_TO_SHM!SUCCESS!"));
            } else {
                write(fd_resp, "WRITE_TO_SHM!ERROR!", strlen("WRITE_TO_SHM!ERROR!"));
            }
        } else if(strcmp(dst, "MAP_FILE") == 0){
            char path[250];
            for(int i = 0; i < 250; i++){
                read(fd_req, &path[i], 1);
                if(path[i] == '!'){
                    path[i] = 0;
                    break;
                }
            }
            fd = open(path, O_RDONLY);
            if(fd == -1){
                write(fd_resp, "MAP_FILE!ERROR!", strlen("MAP_FILE!ERROR!"));     
                continue; 
            }

            file_size = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            file = (char*)mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
            if(file == (void*)-1){
                write(fd_resp, "MAP_FILE!ERROR!", strlen("MAP_FILE!ERROR!"));
                close(fd);
                continue;
            } 
            write(fd_resp, "MAP_FILE!SUCCESS!", strlen("MAP_FILE!SUCCESS!"));
        } else if(strcmp(dst, "READ_FROM_FILE_OFFSET") == 0){
            unsigned int offset = 0;
            unsigned int no_of_bytes = 0;

            read(fd_req, &offset, sizeof(unsigned int));
            read(fd_req, &no_of_bytes, sizeof(unsigned int));

            if(shmFD == -1 || sharedChar == NULL || file == NULL || (offset + no_of_bytes) > file_size){
                write(fd_resp, "READ_FROM_FILE_OFFSET!ERROR!", strlen("READ_FROM_FILE_OFFSET!ERROR!"));
                continue;
            }

            for(int i = 0; i < no_of_bytes; i++){
                sharedChar[i] = file[offset + i];
            }

            write(fd_resp, "READ_FROM_FILE_OFFSET!SUCCESS!", strlen("READ_FROM_FILE_OFFSET!SUCCESS!"));
        } else if (strcmp(dst, "READ_FROM_FILE_SECTION") == 0){
            unsigned int section_no = 0;
            unsigned int offset = 0;
            unsigned int no_of_bytes = 0;

            read(fd_req, &section_no, sizeof(unsigned int));
            read(fd_req, &offset, sizeof(unsigned int));
            read(fd_req, &no_of_bytes, sizeof(unsigned int));

            unsigned short header_size = *(unsigned short*)(file + file_size - 6);
            unsigned int header_start = file_size - header_size;

            unsigned int no_of_sections = *(unsigned int*)(file + header_start + 4);

            if(section_no < 1 || section_no > no_of_sections){
                write(fd_resp, "READ_FROM_FILE_SECTION!ERROR!", strlen("READ_FROM_FILE_SECTION!ERROR!"));
                continue;
            }

            unsigned int section_header_start = header_start + 5 + (section_no - 1) * 19;
            unsigned int section_offset = *(unsigned int*)(file + section_header_start + 11);
            unsigned int section_size = *(unsigned int*)(file + section_header_start + 15);

            if(offset + no_of_bytes > section_size){
                write(fd_resp, "READ_FROM_FILE_SECTION!ERROR!", strlen("READ_FROM_FILE_SECTION!ERROR!"));
                continue;        
            }

            for(int i = 0; i < no_of_bytes; i++){
                sharedChar[i] = file[section_offset + offset + i];
            }

            write(fd_resp, "READ_FROM_FILE_SECTION!SUCCESS!", strlen("READ_FROM_FILE_SECTION!SUCCESS!"));
        } else if(strcmp(dst, "READ_FROM_LOGICAL_SPACE_OFFSET") == 0){
            unsigned int logical_offset = 0;
            unsigned int no_of_bytes = 0;

            read(fd_req, &logical_offset, sizeof(unsigned int));
            read(fd_req, &no_of_bytes, sizeof(unsigned int));

            unsigned short header_size = *(unsigned short*)(file + file_size - 6);
            unsigned int header_start = file_size - header_size;
            unsigned int no_of_sections = *(unsigned int*)(file + header_start + 4);
            unsigned int current_offset = 0;
            int i  = 0;

            for(i = 0; i < no_of_sections; i++){ 
                unsigned int section_header_start = header_start + 5 + i * 19;
                unsigned int section_size = *(unsigned int*)(file + section_header_start + 15);  
                unsigned int next_offset = current_offset + (section_size / 3072 + 1) * 3072;
                if(current_offset <= logical_offset && logical_offset < next_offset){
                    break;
                }
                current_offset = next_offset;
            }
            if(i == no_of_sections){
                write(fd_resp, "READ_FROM_LOGICAL_SPACE_OFFSET!ERROR!", strlen("READ_FROM_LOGICAL_SPACE_OFFSET!ERROR!"));
                continue;
            }

            unsigned int offset_in_section = logical_offset - current_offset;
            unsigned int section_header_start = header_start + 5 + i * 19;
            unsigned int section_offset = *(unsigned int*)(file + section_header_start + 11);
            for(int j = 0; j < no_of_bytes; j++){
                sharedChar[j] = file[section_offset + offset_in_section + j];
            }
            write(fd_resp, "READ_FROM_LOGICAL_SPACE_OFFSET!SUCCESS!", strlen("READ_FROM_LOGICAL_SPACE_OFFSET!SUCCESS!"));
        } else if(strcmp(dst, "EXIT") == 0){
            munmap((void*)file, file_size);
            close(fd);
            shm_unlink("/yTJuDV");
            munmap((void*)sharedChar, shm_size);
            close(fd_resp);
            close(fd_req);
            unlink(RESP_PIPE);
            unlink(REQ_PIPE);
            return 0;
        }
    }
    return 0;
}