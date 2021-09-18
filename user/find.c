#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char target[512];

void find_target(char* path){
      // printf("path: %s\n", path);
      char buf[512], *p;
      int fd;
      struct dirent de;
      struct stat st;

      if((fd = open(path, 0)) < 0){
            fprintf(2, "find: cannot open %s\n", path);
            return;
      }

      if(fstat(fd, &st) < 0){
            fprintf(2, "find: cannot stat %s\n", path);
            close(fd);
            return;
      }
      
      // printf("here.\n");
      if(st.type == T_DIR){
            if(strlen(path) + 1 + DIRSIZ + 1 > 512){
                  printf("find: path too long\n");
                  goto end;
            }
            strcpy(buf, path);
            p = buf + strlen(path);
            *p++ = '/';
            while(read(fd, &de, sizeof(de)) > 0){
                  if(de.inum == 0){
                        continue;
                  }
                  // ignore . and ..
                  if(de.name[0] == '.'){
                        if(de.name[1] == 0 || (de.name[1] == '.' && de.name[2] == 0)){
                              continue;
                        }
                  }
                  // printf("de.name: %s\n", de.name);
                  memmove(p, de.name, DIRSIZ);
                  p[DIRSIZ] = 0;
                  if(stat(buf, &st) < 0){ // it is buf not path
                        fprintf(2, "find: cannot stat");
                  }
                  if(st.type == T_FILE && strcmp(de.name, target) == 0){
                        printf("%s\n", buf); // buf plz
                  }
                  else if(st.type == T_DIR){
                        find_target(buf);  // buf plz
                  }
            }
      }

      end:

      close(fd);
      return;
}

int main(int argc, char* argv[]){
      if(argc != 3){
            fprintf(2, "arguments error");
            exit(1);
      }
      strcpy(target, argv[2]);
      find_target(argv[1]);
      exit(0);
}