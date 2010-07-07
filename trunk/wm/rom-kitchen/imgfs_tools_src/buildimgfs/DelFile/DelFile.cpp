// DelFile.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <windows.h>
#include "..\imgfs.h"

char* Base=0;
FS_BOOT_SECTOR *Boot=0;
char *GetFileName(FS_SHORT_NAME &fsName)
{
	DWORD NameOffs=fsName.dwFullNameOffset;
	DWORD Flags=fsName.wFlags;
	DWORD NameLen=fsName.cchName;
	wchar_t *Name=(wchar_t*)(NameOffs+Base);
	if(fsName.cchName<=4)
		Name=fsName.szShortName;

	if(Flags==2)
		Name+=2;
	static char Buff[1024];
	memset(Buff,0,sizeof(Buff));

	WideCharToMultiByte(CP_ACP,0,Name,NameLen,Buff,1024,0,0);
	return Buff;
}

void DelNormalFile(FS_DATA_RUN *Dr)
{
	FS_ALLOC_TABLE_ENTRY *Tb=(FS_ALLOC_TABLE_ENTRY*)(Base+Dr->dwDiskOffset);
	int Num=Dr->cbOnDiskSize/sizeof(FS_ALLOC_TABLE_ENTRY);
	for(int i=0; i<Num; i++)
	{
		if(Tb[i].dwDiskOffset==0)
			break;
	   	memset(Tb[i].dwDiskOffset+Base,0xFF,Tb[i].wCompressedBlockSize);
	}
	memset(Tb,0xFF,Dr->cbOnDiskSize);
}

void DelXipFile(FS_DATA_RUN *Dr, DWORD FirstStream)
{
	DelNormalFile(Dr);

	FSHEADER *Hdr=(FSHEADER*)(Base+FirstStream);
	while((char*)Hdr>Base)
	{
		FS_STREAM_HEADER &St=Hdr->hdrStream;
		DelNormalFile(St.dataTable);
		FSHEADER *Tmp=(FSHEADER*)(Base+St.dwNextStreamHeaderOffset);	
		memset(Hdr,0xff,Boot->dwBytesPerHeader);
		Hdr=Tmp;
	};
}

int _tmain(int argc, _TCHAR* argv[])
{
	if(argc<2)
	{
		puts("Usage:\n  DelFile file_name");
		return 1;
	}

    HANDLE HF=CreateFile("imgfs_raw_data.bin",GENERIC_WRITE|GENERIC_READ,
        FILE_SHARE_WRITE|FILE_SHARE_READ,0,OPEN_EXISTING,0,0);

    HANDLE HM=CreateFileMapping(HF,0,PAGE_READWRITE,0,0,0);

    Base=(char*)MapViewOfFile(HM,FILE_MAP_WRITE,0,0,0);

    if(!HF || !HM || !Base)
    {
        printf("Error! Cannot map file.");
        return 1;
    }

    DWORD Size=GetFileSize(HF,0);
	Boot=(FS_BOOT_SECTOR*)Base;

	FS_BLOCK_HEADER *Block=(FS_BLOCK_HEADER*)(Boot->dwFirstHeaderBlockOffset+Base);
	bool Found=false;
	do
	{
		for(int i=0; i<Boot->dwFirstHeaderBlockOffset/Boot->dwBytesPerHeader; i++) 
		{
			FSHEADER *Hdr=(FSHEADER*)((char*)Block+sizeof(FS_BLOCK_HEADER)+Boot->dwBytesPerHeader*i);
			switch(Hdr->dwHeaderFlags)
			{
			case 0xFFFFF6FE:
			case 0xFFFFFEFE:
			  {
    			FS_FILE_HEADER &Fh=Hdr->hdrFile;
				if(_strcmpi(argv[1],GetFileName(Fh.fsName))!=0)
					break;

				if(0xFFFFFEFE==Hdr->dwHeaderFlags)
					DelXipFile(Fh.dataTable,Fh.dwNextStreamHeaderOffset);
				else
					DelNormalFile(Fh.dataTable);
				if(Fh.fsName.wFlags==0)
				{
					memset(Fh.fsName.dwFullNameOffset+Base,0xff,Fh.fsName.cchName*2);
				} else if(Fh.fsName.wFlags==2)
				{
					memset(Fh.fsName.dwFullNameOffset+Base,0xff,Boot->dwBytesPerHeader);
				} 
				memset(Hdr,0xff,Boot->dwBytesPerHeader);
				Found=TRUE;
				puts("Deleted");

				break;
			  }
			};
		}


		Block=(FS_BLOCK_HEADER*)(Block->dwNextHeaderBlock+Base);
	} while((char*)Block!=Base);
	if(!Found)
		printf("File '%s' not found in ROM\n",argv[1]);
}

