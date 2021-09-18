#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"


int main(int argc, char* argv[]){
      if(argc <= 1){
            printf("xargs: arguments too few.");
            exit(1);
      }

      char** argv_ = malloc(MAXARG); // defined in kernel/param.h
      char* buf = malloc(MAXARGLEN);
      int i = 1;

      for(; i < argc; ++i){
            argv_[i-1] = argv[i];
      }
      argv_[i] = 0;
      --i;
      
      int n, j;
      while(1){
            j = 0;

            while((n = read(0, &buf[j], 1)) > 0){
                  if(buf[j] == '\n'){
                        // one argument's end
                        break;
                  }
                  ++j;
            }

            if(!n){
                  break;
            }

            buf[j] = 0;
            argv_[i] = buf;

            if(fork() == 0){
                  exec(argv[1], argv_);
            }
            else{
                  wait(0);
            }
      }
      exit(0);
}