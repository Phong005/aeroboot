// BuildImgfs.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <windows.h>
#include <assert.h>
#include "imgfs.h"

char* Base=0;
FS_BOOT_SECTOR *Boot=0;
DWORD AllocAddress=-1;
DWORD SectorSize=-1;
#define RoundUp(a,align) ((((a)+(align)-1)/(align))*(align))
FILETIME FileTime;
DWORD Hash=0;

unsigned char Signature[]={0x06, 0x21, 0x55, 0xC1, 0xD5, 0x29, 0x9C, 0xDF, 0xB8, 0xEA, 0xB8, 0xBA, 0xF3, 0xF9, 0xD3, 0x82,
	0x47, 0x8A, 0x43, 0x3B, 0xE6, 0x2A, 0xD9, 0xA9, 0x31, 0x67, 0x39, 0x09, 0x53, 0x67, 0xBF, 0x12};

#define MAX_ROM_SIZE (128*1024*1024)
bool MemoryMap[MAX_ROM_SIZE/0x40];	// false == free, true == used

/*int mywcslen(wchar_t *s)	// for some hell VS2005 does not understand wcslen func in ASM in release builds
{
	return wcslen(s);
}
int mytowlower(wchar_t s)	
{
	return towlower(s);
}

int __declspec(naked) HashName(wchar_t *arg_0)
{
	__asm {
//arg_0		= dword	ptr  10h

		push	ebx
		push	ebp
		push	esi
		mov	esi, [esp+0x10]
		push	edi
		push	esi		
		call	mywcslen
		mov	ebx, eax
		xor	eax, eax
		mov	ax, [esi]
		push	eax		
		call	mytowlower
		xor	edi, edi
		mov	di, ax
		xor	eax, eax
		mov	ax, [esi+2]
		mov	ebp, 0FFh
		and	edi, ebp
		push	eax		
		call	mytowlower
		xor	ecx, ecx
		mov	ch, al
		add	esp, 0Ch
		or	edi, ecx
		cmp	ebx, 8
		jb	short loc_100029A7
		cmp	word ptr [esi+ebx*2-8],	2Eh
		lea	eax, [ebx-6]
		jz	short loc_100029AA

loc_100029A7:			
		lea	eax, [ebx-2]

loc_100029AA:			
		lea	esi, [esi+eax*2]
		xor	eax, eax
		mov	ax, [esi]
		push	eax		
		call	mytowlower
		and	eax, ebp
		shl	eax, 10h
		or	edi, eax
		xor	eax, eax
		mov	ax, [esi+2]
		push	eax		
		call	mytowlower
		pop	ecx
		movzx	eax, ax
		pop	ecx
		shl	eax, 18h
		or	eax, edi
		pop	edi
		pop	esi
		pop	ebp
		pop	ebx
		retn
	};
}*/

int HashName(wchar_t *arg_0,int namelen)
{
	char Buff[1024];
	memset(Buff,0,sizeof(Buff));
	for(int i=0; i<namelen; i++)
		Buff[i]=arg_0[i]&255;
	if(strlen(Buff)>=8 && Buff[strlen(Buff)-4]=='.')
		Buff[strlen(Buff)-4]=0;
	strlwr(Buff);
	return Buff[0]|(Buff[1]<<8L)|(Buff[strlen(Buff)-2]<<16L)|(Buff[strlen(Buff)-1]<<24L);
}

// TODO: Optimize that
DWORD Alloc(DWORD Bytes, DWORD Align=0x40)
{
	int NBlocks=RoundUp(Bytes,Align)/0x40;
	int Pos=0;
	static int FirstFreePos=0;
	while(MemoryMap[FirstFreePos+1]==true)
		FirstFreePos++;
	for(int i=((FirstFreePos*0x40/Align)*Align)/0x40; i<MAX_ROM_SIZE/0x40; i+=Align/0x40)
	{
		for(int j=i; j<i+NBlocks; j++)
			if(MemoryMap[j])
				goto Next;
		Pos=i;
		break;
Next:;
	}
	for(int j=Pos; j<Pos+NBlocks; j++)
		MemoryMap[j]=true;
	return Pos*0x40;
}

DWORD AllocHeader()
{
	FS_BLOCK_HEADER *Block=(FS_BLOCK_HEADER*)(Boot->dwFirstHeaderBlockOffset+Base);
	FS_BLOCK_HEADER *LastBlock;
	FSHEADER *LastHdr=0;
	do
	{
		LastBlock=Block;
		for(int i=0; i<Boot->dwFirstHeaderBlockOffset/Boot->dwBytesPerHeader; i++) 
		{
			FSHEADER *Hdr=(FSHEADER*)((char*)LastBlock+sizeof(FS_BLOCK_HEADER)+Boot->dwBytesPerHeader*i);
			if(Hdr->dwHeaderFlags==0xFFFFFFFF)
			{
				LastHdr=Hdr;
				return ((char*)LastHdr)-Base;
			}
		}
		Block=(FS_BLOCK_HEADER*)(Block->dwNextHeaderBlock+Base);
	} while((char*)Block!=Base);

	DWORD NewHeaderPos=Alloc(Boot->dwSectorsPerHeaderBlock*SectorSize,SectorSize);
	LastBlock->dwNextHeaderBlock=NewHeaderPos;
	memset(NewHeaderPos+Base,0xff,Boot->dwSectorsPerHeaderBlock*SectorSize);
	Block=(FS_BLOCK_HEADER*)(Base+NewHeaderPos);
	Block->dwBlockSignature=0x2F5314CE;
	Block->dwNextHeaderBlock=0;
	return NewHeaderPos+sizeof(FS_BLOCK_HEADER);
}

void InitAlloc()
{
	memset(MemoryMap,0,sizeof(MemoryMap));
	for(int i=0; i<Boot->dwFirstHeaderBlockOffset/0x40; i++)	// mark 1st sector as used
	{
		MemoryMap[i]=true;
	}
	DWORD NewHeaderPos=Alloc(Boot->dwSectorsPerHeaderBlock*SectorSize,SectorSize); // alloc first header sector
	memset(NewHeaderPos+Base,0xff,Boot->dwSectorsPerHeaderBlock*SectorSize);
	FS_BLOCK_HEADER *Block=(FS_BLOCK_HEADER*)(Base+NewHeaderPos);
	Block->dwBlockSignature=0x2F5314CE;
	Block->dwNextHeaderBlock=0;
}

typedef LPVOID (*FNCompressAlloc)(DWORD AllocSize);
typedef VOID (*FNCompressFree)(LPVOID Address);
typedef DWORD (*FNCompressOpen)( DWORD dwParam1, DWORD MaxOrigSize, FNCompressAlloc AllocFn, FNCompressFree FreeFn, DWORD dwUnknown);
typedef DWORD (*FNCompressConvert)( DWORD ConvertStream, LPVOID CompAdr, DWORD CompSize, LPCVOID OrigAdr, DWORD OrigSize); 
typedef VOID (*FNCompressClose)( DWORD ConvertStream);

LPVOID Compress_AllocFunc(DWORD AllocSize)
{
    return LocalAlloc(LPTR, AllocSize);
}
VOID Compress_FreeFunc(LPVOID Address)
{
    LocalFree(Address);
}

FNCompressOpen CompressOpen= NULL;
FNCompressConvert CompressConvert= NULL;
FNCompressClose CompressClose= NULL;

void AddFile(char* Path)
{
	DWORD Hdr=AllocHeader();
	FSHEADER *LastHdr=(FSHEADER*)(Hdr+Base);
	LastHdr->dwHeaderFlags=0xFFFFF6FE;
	FS_FILE_HEADER *Fh=&LastHdr->hdrFile;
	memset(Fh,0,sizeof(FS_FILE_HEADER));

	char *FileName=strrchr(Path,'\\')+1;

    HANDLE HFa=CreateFile(Path,GENERIC_READ,
        FILE_SHARE_READ,0,OPEN_EXISTING,0,0);

	if(HFa==0)
		ExitProcess(1);

    HANDLE HMa=CreateFileMapping(HFa,0,PAGE_READONLY,0,0,0);

    char *BaseA=(char*)MapViewOfFile(HMa,FILE_MAP_READ,0,0,0);

    DWORD SizeA=GetFileSize(HFa,0);

	Fh->dwFileAttributes=(GetFileAttributes(Path)&0xF)|FILE_ATTRIBUTE_INROM|FILE_ATTRIBUTE_READONLY;
	Fh->fsName.cchName=strlen(FileName);
	Fh->fsName.wFlags=0;
	Fh->fileTime=FileTime;
	if(strlen(FileName)<=4)
	{
		MultiByteToWideChar(CP_OEMCP,0,FileName,Fh->fsName.cchName,(LPWSTR)(Fh->fsName.szShortName),8);
	} else if(strlen(FileName)<=0x30/2)
	{
		char Buff[5];
		Fh->fsName.wFlags=2;
		Fh->fsName.dwFullNameOffset=AllocHeader();
		FSHEADER *NameHdr=(FSHEADER*)(Base+Fh->fsName.dwFullNameOffset);
		NameHdr->dwHeaderFlags=0xFFFFFEFB;
		memset(NameHdr->hdrName.szName,0,0x30);
		MultiByteToWideChar(CP_OEMCP,0,FileName,Fh->fsName.cchName,NameHdr->hdrName.szName,Fh->fsName.cchName*2);
		Fh->fsName.dwShortName=HashName(NameHdr->hdrName.szName,Fh->fsName.cchName);
	} else
	{
		char Buff[5];
		Fh->fsName.dwFullNameOffset=Alloc(Fh->fsName.cchName*2);
		MultiByteToWideChar(CP_OEMCP,0,FileName,Fh->fsName.cchName,(LPWSTR)(Fh->fsName.dwFullNameOffset+Base),Fh->fsName.cchName*2);
		Fh->fsName.dwShortName=HashName((LPWSTR)(Fh->fsName.dwFullNameOffset+Base),Fh->fsName.cchName);
	}

	Fh->dwStreamSize=SizeA;
	if(SizeA==0)
	{
		Fh->dataTable[0].cbOnDiskSize=0;
		Fh->dataTable[0].dwDiskOffset=0;
	} else
	{
		Fh->dataTable[0].cbOnDiskSize=RoundUp(RoundUp(SizeA,Boot->dwDataBlockSize)/Boot->dwDataBlockSize*sizeof(FS_ALLOC_TABLE_ENTRY),0x40);
		Fh->dataTable[0].dwDiskOffset=Alloc(Fh->dataTable[0].cbOnDiskSize);
		memset(Fh->dataTable[0].dwDiskOffset+Base,0,Fh->dataTable[0].cbOnDiskSize);
		FS_ALLOC_TABLE_ENTRY *Te=(FS_ALLOC_TABLE_ENTRY*)(Base+Fh->dataTable[0].dwDiskOffset);
		for(int i=0; i<SizeA; i+=Boot->dwDataBlockSize)
		{
			int CurrSize=Boot->dwDataBlockSize;
			if(SizeA-i<Boot->dwDataBlockSize)
				CurrSize=SizeA-i;
		
			Te[i/Boot->dwDataBlockSize].wDecompressedBlockSize=CurrSize;

			DWORD stream= CompressOpen(0x10000, 0x10000, Compress_AllocFunc, Compress_FreeFunc, 0);
			BYTE *buf= (BYTE*)LocalAlloc(LPTR, 0x10000);
			DWORD ret=CompressConvert(stream, buf, CurrSize, BaseA+i, CurrSize);

			if(ret<CurrSize)
			{
				Te[i/Boot->dwDataBlockSize].wCompressedBlockSize=ret;
				Te[i/Boot->dwDataBlockSize].dwDiskOffset=Alloc(ret);
				memcpy(Te[i/Boot->dwDataBlockSize].dwDiskOffset+Base,buf,ret);
			} else
			{
				Te[i/Boot->dwDataBlockSize].wCompressedBlockSize=CurrSize;
				Te[i/Boot->dwDataBlockSize].dwDiskOffset=Alloc(CurrSize);
				memcpy(Te[i/Boot->dwDataBlockSize].dwDiskOffset+Base,BaseA+i,CurrSize);
			}

			LocalFree(buf);
			CompressClose(stream);
		}
	}
	UnmapViewOfFile(BaseA);
	CloseHandle(HMa);
	CloseHandle(HFa);
}

void AddModule(char* Path)
{
	DWORD Hdr=AllocHeader();
	FSHEADER *LastHdr=(FSHEADER*)(Hdr+Base);
	LastHdr->dwHeaderFlags=0xFFFFFEFE;
	FS_FILE_HEADER *Fh=&LastHdr->hdrFile;
	memset(Fh,0,sizeof(FS_FILE_HEADER));

	char *FileName=strrchr(Path,'\\')+1;

	char TmpFileName[1024];
	strcpy(TmpFileName,Path);
	strcat(TmpFileName,"\\imageinfo.bin");

    HANDLE HFa=CreateFile(TmpFileName,GENERIC_READ,
        FILE_SHARE_READ,0,OPEN_EXISTING,0,0);

	if(HFa==0)
		ExitProcess(1);

	HANDLE HMa=CreateFileMapping(HFa,0,PAGE_READONLY,0,0,0);

	char *BaseA=(char*)MapViewOfFile(HMa,FILE_MAP_READ,0,0,0);

    DWORD SizeA=GetFileSize(HFa,0);

	Fh->dwFileAttributes=(GetFileAttributes(TmpFileName)&0xF)|FILE_ATTRIBUTE_INROM;
	Fh->fsName.cchName=strlen(FileName);
	Fh->fsName.wFlags=0;
	Fh->fileTime=FileTime;
	if(strlen(FileName)<=4)
	{
		MultiByteToWideChar(CP_OEMCP,0,FileName,Fh->fsName.cchName,(LPWSTR)(Fh->fsName.szShortName),8);
	} else if(strlen(FileName)<=0x30/2)
	{
		char Buff[5];
		Fh->fsName.wFlags=2;
		Fh->fsName.dwFullNameOffset=AllocHeader();
		FSHEADER *NameHdr=(FSHEADER*)(Base+Fh->fsName.dwFullNameOffset);
		NameHdr->dwHeaderFlags=0xFFFFFEFB;
		memset(NameHdr->hdrName.szName,0,0x30);
		MultiByteToWideChar(CP_OEMCP,0,FileName,Fh->fsName.cchName,NameHdr->hdrName.szName,Fh->fsName.cchName*2);
		Fh->fsName.dwShortName=HashName(NameHdr->hdrName.szName,Fh->fsName.cchName);
	} else
	{
		char Buff[5];
		Fh->fsName.dwFullNameOffset=Alloc(Fh->fsName.cchName*2);
		MultiByteToWideChar(CP_OEMCP,0,FileName,Fh->fsName.cchName,(LPWSTR)(Fh->fsName.dwFullNameOffset+Base),Fh->fsName.cchName*2);
		Fh->fsName.dwShortName=HashName((LPWSTR)(Fh->fsName.dwFullNameOffset+Base),Fh->fsName.cchName);
	}

	Fh->dwStreamSize=SizeA;
	if(SizeA==0)
	{
		Fh->dataTable[0].cbOnDiskSize=0;
		Fh->dataTable[0].dwDiskOffset=0;
	} else
	{
		Fh->dataTable[0].cbOnDiskSize=RoundUp(RoundUp(SizeA,Boot->dwDataBlockSize)/Boot->dwDataBlockSize*sizeof(FS_ALLOC_TABLE_ENTRY),0x40);
		Fh->dataTable[0].dwDiskOffset=Alloc(Fh->dataTable[0].cbOnDiskSize);
		memset(Fh->dataTable[0].dwDiskOffset+Base,0,Fh->dataTable[0].cbOnDiskSize);
		FS_ALLOC_TABLE_ENTRY *Te=(FS_ALLOC_TABLE_ENTRY*)(Base+Fh->dataTable[0].dwDiskOffset);
		for(int i=0; i<SizeA; i+=Boot->dwDataBlockSize)
		{
			int CurrSize=Boot->dwDataBlockSize;
			if(SizeA-i<Boot->dwDataBlockSize)
				CurrSize=SizeA-i;
		
			Te[i/Boot->dwDataBlockSize].wDecompressedBlockSize=CurrSize;

			DWORD stream= CompressOpen(0x10000, 0x10000, Compress_AllocFunc, Compress_FreeFunc, 0);
			BYTE *buf= (BYTE*)LocalAlloc(LPTR, 0x10000);
			DWORD ret=CompressConvert(stream, buf, CurrSize, BaseA+i, CurrSize);

			if(ret<CurrSize)
			{
				Te[i/Boot->dwDataBlockSize].wCompressedBlockSize=ret;
				Te[i/Boot->dwDataBlockSize].dwDiskOffset=Alloc(ret);
				memcpy(Te[i/Boot->dwDataBlockSize].dwDiskOffset+Base,buf,ret);
			} else
			{
				Te[i/Boot->dwDataBlockSize].wCompressedBlockSize=CurrSize;
				Te[i/Boot->dwDataBlockSize].dwDiskOffset=Alloc(CurrSize);
				memcpy(Te[i/Boot->dwDataBlockSize].dwDiskOffset+Base,BaseA+i,CurrSize);
			}

			LocalFree(buf);
			CompressClose(stream);
		}
	}
	UnmapViewOfFile(BaseA);
	CloseHandle(HMa);
	CloseHandle(HFa);
	FS_STREAM_HEADER *PrevStream=(FS_STREAM_HEADER*)Fh;
	for(int i=0; i<999; i++) // FIXME: Use FindFirst/FindNext to search for "S000" files in subdir
	{
		char FileName[1024];
		sprintf(FileName,"%s\\S%03d",Path,i);
		HANDLE HFa=CreateFile(FileName,GENERIC_READ,
			FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
		if(HFa==INVALID_HANDLE_VALUE)
			break;
		Fh->dwFileAttributes|=FILE_ATTRIBUTE_ROMMODULE;

		HANDLE HMa=CreateFileMapping(HFa,0,PAGE_READONLY,0,0,0);

		char *BaseA=(char*)MapViewOfFile(HMa,FILE_MAP_READ,0,0,0);

		DWORD SizeA=GetFileSize(HFa,0);
		DWORD Hdr=AllocHeader();
		FSHEADER *LastHdr=(FSHEADER*)(Hdr+Base);
		LastHdr->dwHeaderFlags=0xFFFFF6FD;
		FS_STREAM_HEADER *Fs=&LastHdr->hdrStream;
		memset(Fs,0,sizeof(FS_STREAM_HEADER));
		PrevStream->dwNextStreamHeaderOffset=Hdr;
		PrevStream=Fs;
		
		Fs->dwSecNameLen=4;
		MultiByteToWideChar(CP_OEMCP,0,FileName+strlen(FileName)-4,4,(LPWSTR)(Fs->szSecName),8);

		Fs->dwStreamSize=SizeA;
		if(SizeA==0)
		{
			Fs->dataTable[0].cbOnDiskSize=0;
			Fs->dataTable[0].dwDiskOffset=0;
		} else
		{
			Fs->dataTable[0].cbOnDiskSize=RoundUp(RoundUp(SizeA,Boot->dwDataBlockSize)/Boot->dwDataBlockSize*sizeof(FS_ALLOC_TABLE_ENTRY),0x40);
			Fs->dataTable[0].dwDiskOffset=Alloc(Fs->dataTable[0].cbOnDiskSize);
			memset(Fs->dataTable[0].dwDiskOffset+Base,0,Fs->dataTable[0].cbOnDiskSize);
			FS_ALLOC_TABLE_ENTRY *Te=(FS_ALLOC_TABLE_ENTRY*)(Base+Fs->dataTable[0].dwDiskOffset);
			for(int i=0; i<SizeA; i+=Boot->dwDataBlockSize)
			{
				int CurrSize=Boot->dwDataBlockSize;
				if(SizeA-i<Boot->dwDataBlockSize)
					CurrSize=SizeA-i;
			
				Te[i/Boot->dwDataBlockSize].wDecompressedBlockSize=CurrSize;

				DWORD stream= CompressOpen(0x10000, 0x10000, Compress_AllocFunc, Compress_FreeFunc, 0);
				BYTE *buf= (BYTE*)LocalAlloc(LPTR, 0x10000);
				DWORD ret=CompressConvert(stream, buf, Boot->dwDataBlockSize, BaseA+i, CurrSize);
		//		printf("%d\n",ret);

				if(ret<CurrSize)
				{
					Te[i/Boot->dwDataBlockSize].wCompressedBlockSize=ret;
					Te[i/Boot->dwDataBlockSize].dwDiskOffset=Alloc(ret);
					memcpy(Te[i/Boot->dwDataBlockSize].dwDiskOffset+Base,buf,ret);
				} else
				{
					Te[i/Boot->dwDataBlockSize].wCompressedBlockSize=CurrSize;
					Te[i/Boot->dwDataBlockSize].dwDiskOffset=Alloc(CurrSize);
					memcpy(Te[i/Boot->dwDataBlockSize].dwDiskOffset+Base,BaseA+i,CurrSize);
				}

				LocalFree(buf);
				CompressClose(stream);
			}
		}
		UnmapViewOfFile(BaseA);
		CloseHandle(HMa);
		CloseHandle(HFa);
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
   	HMODULE hDll= LoadLibrary("cecompr_nt.dll");
    if (hDll==NULL || hDll==INVALID_HANDLE_VALUE) 
	{
		puts("Unable to load compression DLL!");
    	return 1;
	}
    
    HANDLE HF=CreateFile("imgfs_raw_data.bin",GENERIC_WRITE|GENERIC_READ,
        FILE_SHARE_WRITE|FILE_SHARE_READ,0,OPEN_EXISTING,0,0);

	GetFileTime(HF,&FileTime,0,0);

	if(HF==0)
	{
		puts("Unable to map file!");
		return 1;
	}

    HANDLE HM=CreateFileMapping(HF,0,PAGE_READWRITE,0,0,0);

    Base=(char*)MapViewOfFile(HM,FILE_MAP_WRITE,0,0,0);
	Boot=(FS_BOOT_SECTOR*)Base;

    HANDLE HFr=CreateFile("imgfs_removed_data.bin",GENERIC_WRITE|GENERIC_READ,
        FILE_SHARE_WRITE|FILE_SHARE_READ,0,OPEN_EXISTING,0,0);

    HANDLE HMr=CreateFileMapping(HFr,0,PAGE_READWRITE,0,0,0);

    char *BaseR=(char*)MapViewOfFile(HMr,FILE_MAP_WRITE,0,0,0);

    DWORD SizeR=GetFileSize(HFr,0);

	char CO[]="LZX_CompressOpen";
	char CE[]="LZX_CompressEncode";
	char CC[]="LZX_CompressClose";

	memcpy(CO,&Boot->szCompressionType,3);
	memcpy(CE,&Boot->szCompressionType,3);
	memcpy(CC,&Boot->szCompressionType,3);

	CompressOpen= (FNCompressOpen)GetProcAddress(hDll, CO);
    CompressConvert= (FNCompressConvert)GetProcAddress(hDll, CE);
    CompressClose= (FNCompressClose)GetProcAddress(hDll, CC);
	if(CompressOpen==0)
	{
		puts("Unable to load compression DLL!");
		return 1;
	}
	if(Base==0)
	{
		puts("Unable to map file!");
		return 1;
	}

    DWORD Size=GetFileSize(HF,0);
	AllocAddress = Boot->dwFirstHeaderBlockOffset;
	SectorSize=Boot->dwFirstHeaderBlockOffset;

	InitAlloc();
	
	WIN32_FIND_DATA fd;
	HANDLE Hf=FindFirstFile("dump\\*",&fd);
	BOOL F;
	do
	{
		if(fd.cFileName[0]=='.')
			if((fd.cFileName[1]=='.' && fd.cFileName[2]==0) || fd.cFileName[1]==0)
			{
				F=FindNextFile(Hf,&fd);
				continue;
			}
		printf("Processing \"%s\"",fd.cFileName);
		char FileName[1024]="dump\\";
		strcat(FileName,fd.cFileName);
		if(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
		{
			puts(" as module");
			AddModule(FileName);
		} else
		{
			puts(" as file");
			AddFile(FileName);
		}

		F=FindNextFile(Hf,&fd);
	} while(F);


// and now fill the imgfs_removed_data.bin
// Boot->dwFirstHeaderBlockOffset == sector size
	DWORD FirstDW=0;
	if(BaseR)
		FirstDW=*(DWORD*)BaseR;
	DWORD SectPerBlock=0x3F000/Boot->dwFirstHeaderBlockOffset;
	int NumBlocks=0;
	for(int i=0; i<SizeR-0xFE0 && BaseR; i+=0x1000)
	{
		if(memcmp(BaseR+i+0xFE0,Signature,sizeof(Signature))==0)
		{
			memset(BaseR+i,0xFF,0xFE0);
			NumBlocks++;
		}
	}
// and now fill the imgfs_removed_data.bin
// Boot->dwFirstHeaderBlockOffset == sector size
	DWORD LastAddress=MAX_ROM_SIZE/0x40;
	while(MemoryMap[LastAddress-1]==false)
		LastAddress--;
	LastAddress*=0x40;
	printf("Total Sectors: %04x\n",NumBlocks*SectPerBlock);
	printf("Used Sectors : %04x\n",LastAddress/Boot->dwFirstHeaderBlockOffset);
	printf("Free Sectors : %04x\n",NumBlocks*SectPerBlock-LastAddress/Boot->dwFirstHeaderBlockOffset);
	LastAddress=RoundUp(LastAddress,SectorSize);
	for(int i=0; i<1+(LastAddress/SectorSize) && BaseR; i++)
	{	
		DWORD *Ptr=(DWORD*)((i/SectPerBlock)*0x1000+(i%SectPerBlock)*8+BaseR);
		Ptr[0]=i+FirstDW;
		Ptr[1]=0xFFFBFFFF;
	}
	return 0;
}

