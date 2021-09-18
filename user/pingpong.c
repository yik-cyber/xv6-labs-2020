#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char* argv[]){
      int p1[2], p2[2];
      if(pipe(p1) < 0){
            printf("pipe: error.");
            exit(1);
      }
      if(pipe(p2) < 0){
            printf("pipe: error.");
            exit(1);
      }

      char byte = 'x';

      if(fork() == 0){
            // child proc
            if(read(p1[0], &byte, 1) > 0){
                  printf("%d: received ping\n", getpid());
                  write(p2[1], &byte, 1);
            }
      }
      else{
            write(p1[1], &byte, 1);
            if(read(p2[0], &byte, 1) > 0){
                  printf("%d: received pong\n", getpid());
            }
      }

      close(p1[0]);
      close(p1[1]);
      close(p2[0]);
      close(p2[1]);

      exit(0);
}