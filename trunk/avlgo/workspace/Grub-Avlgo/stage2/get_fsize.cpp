
// get_fsize.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h> 
#include <fcntl.h> 
#include <io.h> 
int main(int argc, char* argv[])
{
    char chBuf[]="ffffff";

    int handle;

    handle = open("pre_stage2", _O_BINARY); 
    
    for(int i=1; i< argc; i++)
        printf("%s ", argv[i]);

    printf(" %d\n", filelength(handle));
    
    printf(chBuf);

    close(handle);

	return 0;
}


