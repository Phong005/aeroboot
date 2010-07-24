// ctags.cpp : Defines the entry point for the console application.
//
#include "StdAfx.h"
#include <windows.h>
#include <direct.h>
#include <iostream.h>
#include <stdio.h>        // used only for FILENAME_MAX

#include "ObjFolder.h"

int main(int argc, char **argv) 
{

	CObjFolder		objFolder ;

	
	char			szRoot[MAX_PATH] = {0}; //"E:\\GCodes\\Grub4\\grub2";

	// Logo.
	printf ("\n\nUnix Project to Visual Studio Project\n\n (r) Powered by y.volta (y.volta@gmail.com).\n 2008-09-11 12:30 rev 2.\n\n");

	if (argc != 2)
	{
		printf ("\n Error! you should provide a target directory.\n");
		return -1;
	}

	strcpy(szRoot, argv[1]);
	
	SetCurrentDirectory( szRoot );

	// init the output file.
	objFolder.CreateVCPrjInfo(szRoot);

	// output the head of .proj file.
	printf(" Construct the .proj _");
	objFolder.LoadProjFile("IDR_PROJ_HEAD");

	//
	printf("...");
	objFolder.Recusive ( szRoot );

	// append the nail of .proj file.
	printf("-");
	objFolder.LoadProjFile("IDR_PROJ_NAIL");
	
	printf(" Done!\n\n");
    return 0;

}
