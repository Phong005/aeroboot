#include <stdio.h>
#include <windows.h>
#include "imgfs.h"

unsigned char IMGFS_GUID[]={0xF8, 0xAC, 0x2C, 0x9D, 0xE3, 0xD4, 0x2B, 0x4D, 
							0xBD, 0x30, 0x91, 0x6E, 0xD8, 0x4F, 0x31, 0xDC };


void main(int argc, char **argv)
{
	if(argc<2)
	{
		puts("Usage: make_imgfs.exe nk.nba [-nosplit]");
		return;
	}

    HANDLE HF=CreateFile(argv[1],GENERIC_WRITE|GENERIC_READ,
        FILE_SHARE_WRITE|FILE_SHARE_READ,0,OPEN_EXISTING,0,0);

    HANDLE HM=CreateFileMapping(HF,0,PAGE_READWRITE,0,0,0);

    char* Base=(char*)MapViewOfFile(HM,FILE_MAP_WRITE,0,0,0);

    if(!HF || !HM || !Base)
    {
        printf("Ошибка! Не могу отобразить файл %s в память\n",argv[1]);
        return;
    }

    DWORD Size=GetFileSize(HF,0);
    char *ImgfsPtr=0;
    printf("Searching for IMGFS start... ");
    for(int i=0; i<Size-16; i++)
    {
    	if(memcmp(IMGFS_GUID,Base+i,16)==0 && 
    		(memcmp(Base+i+0x2c,"LZX",3)==0 || memcmp(Base+i+0x2c,"XPR",3)==0))
    	{
    		ImgfsPtr=Base+i;
    	}
    }
    if(ImgfsPtr==0)
    {
    	puts("Not found!");
    	return;
    }
    printf("Found at %08X\n",ImgfsPtr-Base);
	HANDLE H=CreateFile("imgfs_raw_data.bin",GENERIC_WRITE|GENERIC_READ,
        FILE_SHARE_WRITE|FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
	HANDLE HT=CreateFile("imgfs_removed_data.bin",GENERIC_WRITE|GENERIC_READ,
        FILE_SHARE_WRITE|FILE_SHARE_READ,0,OPEN_EXISTING,0,0);

    if(argc>2)
    {
    	CloseHandle(HT); HT=H;
    }

    Size-=ImgfsPtr-Base;
	DWORD Tmp;
	printf("Fixing... ");
    for(int i=0; i<Size; i+=0x40000)
    {
    	ReadFile(H,ImgfsPtr+i,0x3F000,&Tmp,0);
    	ReadFile(HT,ImgfsPtr+i+0x3F000,0x1000,&Tmp,0);
    }
	puts("Done!");
}
