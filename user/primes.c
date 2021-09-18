#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


void primes_pipe(int rd){
      int n, x;

      if(read(rd, &n, 4) == 0){
            close(rd);
            return;
      }

      printf("prime %d\n", n);
      int p[2];
      if(pipe(p) < 0){
            printf("pipe: error");
            exit(1);
      }
      
      if(fork() == 0){
            close(rd);
            close(p[1]);
            primes_pipe(p[0]);
            exit(0);

      }
      else{
            close(p[0]);
            while(read(rd, &x, 4) > 0){
                  if(x%n){
                        write(p[1], &x, 4);
                  }
            }
            close(rd);
            close(p[1]);
            wait(0);
      }
      exit(0);
}



int main(int argc, char* argv[]){
      int p[2];
      if(pipe(p) < 0){
            printf("pipe: error");
            exit(1);
      }

      if(fork() == 0){
            close(p[1]);
            primes_pipe(p[0]);
      }
      else{
            printf("prime 2\n");
            close(p[0]);
            for(int i = 3; i <= 35; ++i){
                  if(i%2){
                        // 4 bytes
                        write(p[1], &i, 4);
                  }
            }
            close(p[1]);
            // it will end when child proc returns
            wait(0);
      }
      exit(0);
}