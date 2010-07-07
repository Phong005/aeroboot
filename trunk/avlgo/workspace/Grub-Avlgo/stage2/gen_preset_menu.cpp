// gen_preset_menu.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h> 

int main(int argc, char* argv[])
{
    char chBuf[]="ffffff";
    int c;

    FILE *fp, *fp2;

    fp = fopen("menu.lst","r"); 
    if(fp == NULL)
    {
        printf("\nmenu.lst is not found");
        return 1;
    }
    fp2 = fopen("preset_menu.h", "w+");
    
    fputs("#define PRESET_MENU_STRING \"", fp2);

    while ((c = fgetc (fp)) != EOF)
    {
    switch (c)
        {
	    case '\n':
	      fputs ("\\n", fp2);
	      break;
	    case '\r':
	      fputs ("\\r", fp2);
	      break;
	    case '\\':
	      fputs ("\\\\", fp2);
	      break;
	    case '"':
	      fputs ("\\\"", fp2);
	      break;
	    default:
	      fputc (c, fp2);
    	}
    }
    
    fputs("\"", fp2);

    fclose(fp);
    fclose(fp2);
  
	return 0;
}
