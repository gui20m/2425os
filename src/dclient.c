#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char* argv[]){

    /*if (argc<2){
        char* buf = "Usage:\n  index file: ./dclient -a \"title\" \"authors\" \"year\" \"path\"\n  consult meta-data: ./dclient -c \"key\"\n  delete meta-data: ./dclient -d \"key\"\n  how many words exists: ./dclient -l \"key\" \"keyword\"\n";
        write(1, buf, strlen(buf));
        return 1;
    }*/

    if (strcmp(argv[1], "-a")==0){
        //TO DO
    }
    if (strcmp(argv[1], "-c")==0){
        //TO DO
    }
    if (strcmp(argv[1], "-d")==0){
        //TO DO
    }
    if (strcmp(argv[1], "-l")==0){
        //TO DO
    }
    if (strcmp(argv[1], "-s")==0){
        //TO DO
    }

    return 0;
}