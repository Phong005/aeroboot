#include <stdio.h>
#include <windows.h>
#include "imgfs.h"

char* Base=0;
#define FIRST_FREE_ADDRESS 0x1527400
#define FILENAME "wince.new"
static DWORD AllocAddress=FIRST_FREE_ADDRESS;

unsigned char Signature[]={0x06, 0x21, 0x55, 0xC1, 0xD5, 0x29, 0x9C, 0xDF, 0xB8, 0xEA, 0xB8, 0xBA, 0xF3, 0xF9, 0xD3, 0x82,
	0x47, 0x8A, 0x43, 0x3B, 0xE6, 0x2A, 0xD9, 0xA9, 0x31, 0x67, 0x39, 0x09, 0x53, 0x67, 0xBF, 0x12};

#define RoundUp(a,align) ((((a)+(align)-1)/(align))*(align))
DWORD Alloc(DWORD Bytes, DWORD Align=0x40)
{
	DWORD Ret=RoundUp(AllocAddress,Align);
	AllocAddress=Ret+Bytes;
	return Ret;
}


char *GetFileName(DWORD NameOffs,DWORD Flags, DWORD NameLen)
{
	wchar_t *Name=(wchar_t*)(NameOffs+Base);
	if(Flags==2)
		Name+=2;
	static char Buff[1024];
	memset(Buff,0,sizeof(Buff));
	WideCharToMultiByte(CP_ACP,0,Name,NameLen,Buff,1024,0,0);
	return Buff;
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

#define min(a,b) ((a)<(b)?(a):(b))

void main(int argc, char **argv)
{
   	HMODULE hDll= LoadLibrary("cecompr_nt.dll");
    if (hDll==NULL || hDll==INVALID_HANDLE_VALUE) 
    	return;
    
	CompressOpen= (FNCompressOpen)GetProcAddress(hDll, "LZX_CompressOpen");
    CompressConvert= (FNCompressConvert)GetProcAddress(hDll, "LZX_CompressEncode");
    CompressClose= (FNCompressClose)GetProcAddress(hDll, "LZX_CompressClose");

    HANDLE HF=CreateFile("imgfs_raw_data.bin",GENERIC_WRITE|GENERIC_READ,
        FILE_SHARE_WRITE|FILE_SHARE_READ,0,OPEN_EXISTING,0,0);

    HANDLE HM=CreateFileMapping(HF,0,PAGE_READWRITE,0,0,0);

    Base=(char*)MapViewOfFile(HM,FILE_MAP_WRITE,0,0,0);

    HANDLE HFr=CreateFile("imgfs_removed_data.bin",GENERIC_WRITE|GENERIC_READ,
        FILE_SHARE_WRITE|FILE_SHARE_READ,0,OPEN_EXISTING,0,0);

    HANDLE HMr=CreateFileMapping(HFr,0,PAGE_READWRITE,0,0,0);

    char *BaseR=(char*)MapViewOfFile(HMr,FILE_MAP_WRITE,0,0,0);

    DWORD SizeR=GetFileSize(HFr,0);

    HANDLE HFa=CreateFile("wince.nls",GENERIC_WRITE|GENERIC_READ,
        FILE_SHARE_WRITE|FILE_SHARE_READ,0,OPEN_EXISTING,0,0);

    HANDLE HMa=CreateFileMapping(HFa,0,PAGE_READWRITE,0,0,0);

    char *BaseA=(char*)MapViewOfFile(HMa,FILE_MAP_WRITE,0,0,0);

    DWORD SizeA=GetFileSize(HFa,0);

    if(!HF || !HM || !Base)
    {
        printf("Ошибка! Не могу отобразить файл %s в память\n",argv[1]);
        return;
    }

    DWORD Size=GetFileSize(HF,0);
	FS_BOOT_SECTOR *Boot=(FS_BOOT_SECTOR*)Base;

	FS_BLOCK_HEADER *Block=(FS_BLOCK_HEADER*)(Boot->dwFirstHeaderBlockOffset+Base);
	FS_BLOCK_HEADER *LastBlock;
	do
	{
		printf("Address: %08X, dwBlockSignature: %08X\n",(char*)Block-Base,Block->dwBlockSignature);
		LastBlock=Block;
		Block=(FS_BLOCK_HEADER*)(Block->dwNextHeaderBlock+Base);
	} while((char*)Block!=Base);

	FSHEADER *LastHdr=0;
	for(int i=0; i<Boot->dwFirstHeaderBlockOffset/Boot->dwBytesPerHeader; i++) 
	{
		FSHEADER *Hdr=(FSHEADER*)((char*)LastBlock+sizeof(FS_BLOCK_HEADER)+Boot->dwBytesPerHeader*i);
		printf("\n Header type: %08X, Addr: %08X\n",Hdr->dwHeaderFlags,(char*)Hdr-Base);
		if(Hdr->dwHeaderFlags==0xFFFFFFFF)
		{
			LastHdr=Hdr;
			break;
		}
	}
	if(LastHdr==0)
	{
		puts("No empty headers!");
		return;
	}

	LastHdr->dwHeaderFlags=0xFFFFFEFE;
	FS_FILE_HEADER *Fh=&LastHdr->hdrFile;
	memset(Fh,0,sizeof(FS_FILE_HEADER));
	Fh->dwFileAttributes=0x47;
	Fh->fsName.cchName=strlen(FILENAME);
	Fh->fsName.wFlags=0;
	Fh->fsName.dwShortName=0x65636977;
	Fh->fsName.dwFullNameOffset=Alloc(Fh->fsName.cchName*2);
	memcpy(Fh->fsName.dwFullNameOffset+Base,L"" FILENAME, 2*strlen(FILENAME));
	Fh->dwStreamSize=SizeA;
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
		DWORD ret=CompressConvert(stream, buf, CurrSize/*Boot->dwDataBlockSize*/, BaseA+i, CurrSize);//Boot->dwDataBlockSize);
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

// and now fill the imgfs_removed_data.bin
// Boot->dwFirstHeaderBlockOffset == sector size
	DWORD FirstDW=*(DWORD*)BaseR;
	DWORD SectPerBlock=0x3F000/Boot->dwFirstHeaderBlockOffset;
	int NumBlocks=0;
	for(int i=0; i<SizeR-0xFE0; i+=0x1000)
	{
		if(memcmp(BaseR+i+0xFE0,Signature,sizeof(Signature))==0)
		{
			memset(BaseR+i,0xFF,0xFE0);
			NumBlocks++;
		}
	}
	printf("Total Sectors: %04x\n",NumBlocks*SectPerBlock);
	printf("Used Sectors : %04x\n",AllocAddress/Boot->dwFirstHeaderBlockOffset);
	printf("Free Sectors : %04x\n",NumBlocks*SectPerBlock-AllocAddress/Boot->dwFirstHeaderBlockOffset);
	for(int i=0; i<1+((AllocAddress+Boot->dwFirstHeaderBlockOffset)/Boot->dwFirstHeaderBlockOffset); i++)
	{	
		DWORD *Ptr=(DWORD*)((i/SectPerBlock)*0x1000+(i%SectPerBlock)*8+BaseR);
		Ptr[0]=i+FirstDW;
		Ptr[1]=0xFFFBFFFF;
	}
}
