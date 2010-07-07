#include <stdio.h>
#include <windows.h>
#include "imgfs.h"

unsigned char IMGFS_GUID[]={0xF8, 0xAC, 0x2C, 0x9D, 0xE3, 0xD4, 0x2B, 0x4D, 
							0xBD, 0x30, 0x91, 0x6E, 0xD8, 0x4F, 0x31, 0xDC };

void main(int argc, char **argv)
{
	if (argc < 2)
	{
		puts("Usage: prepare_imgfs.exe nk.nba [-nosplit] [-mpx200] [-acer] [-sp] [-emu]");
		return;
	}

    HANDLE HF=CreateFile(argv[1], GENERIC_WRITE|GENERIC_READ, 
        FILE_SHARE_WRITE|FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    HANDLE HM=CreateFileMapping(HF, 0, PAGE_READWRITE, 0, 0, 0);
    char* Base=(char*)MapViewOfFile(HM, FILE_MAP_WRITE, 0, 0, 0);

    if (!HF || !HM || !Base)
    {
        printf("Unable to open source image [%s]\n", argv[1]);
        return;
    }

    DWORD Size=GetFileSize(HF, 0);
    char *ImgfsPtr=0;
    printf("Searching for IMGFS start... ");

    for(DWORD i=0; i<Size-16; i++)
    {
    	if (memcmp(IMGFS_GUID, Base+i, 16)==0 &&
    		(memcmp(Base+i+0x2c, "LZX", 3)==0 || memcmp(Base+i+0x2c, "XPR", 3)==0))
    	{
    		ImgfsPtr=Base+i;
    	}
    }

    if (ImgfsPtr==0)
    {
    	puts("Not found!");
    	return;
    }
    printf("Found at %08X\n", ImgfsPtr-Base);

	HANDLE H=CreateFile("imgfs_raw_data.bin", GENERIC_WRITE|GENERIC_READ, 
        FILE_SHARE_WRITE|FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
	HANDLE HT=CreateFile("imgfs_removed_data.bin", GENERIC_WRITE|GENERIC_READ, 
        FILE_SHARE_WRITE|FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

	DWORD rate=0x40000, step=0x3F000, skip=0x1000;
	BOOL acer=FALSE;

	puts("Dumping IMGFS ...");
    while(argc-->2)
    {
		if (argv[argc][1] == 'm')
			{ rate=0x20000; step=0x1F800; skip=0x0800; }
		if (argv[argc][1] == 'e')
			{ rate=0x10000; step=0xF000; skip=0x1000; }
		if (argv[argc][1] == 's')
			{ rate=0x208; step=0x200; skip=0x08; }
		if (argv[argc][1] == 'a')
			{ rate=0x40000; step=0x40000; skip=0x0000; acer=TRUE; }
    	if (argv[argc][1] == 'n')
    		{ CloseHandle(HT); HT=H; }
    }

    Size -= (DWORD) (ImgfsPtr - Base);
	DWORD Tmp;
    for(DWORD i=0; i<Size; i+=rate)
    {
		if (acer)
		{
			for (DWORD j=0; j<step; j+=0x200)
			{
    			WriteFile(H, ImgfsPtr+i+j, 0x200, &Tmp, 0);
    			i += 8;
			}
			for (DWORD j=0; j<skip; j+=0x200)
			{
    			WriteFile(HT, ImgfsPtr+i+j+step, 0x200, &Tmp, 0);
    			i += 8;
			}
		}
		else
		{
    		WriteFile(H, ImgfsPtr+i, step, &Tmp, 0);
    		WriteFile(HT, ImgfsPtr+i+step, skip, &Tmp, 0);
		}

    	FlushFileBuffers(H);
    	FlushFileBuffers(HT);
    }
	puts("Done!");
}
