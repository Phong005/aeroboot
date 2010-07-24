// File.cpp: implementation of the CFile class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "File.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFile::CFile()
{
	m_strFilePath[0] = 0x0;
}

CFile::CFile(LPCTSTR lpFileName)
{
	strcpy (m_strFilePath, lpFileName);
}

CFile::~CFile()
{

}

//////////////////////////////////////////////////////////////////////////
BOOL CFile::OpenFile()
{
	HANDLE hFile = NULL;
	HANDLE hMapFile = NULL;
	CHAR   *pBuff = NULL;

/*
	TCHAR szName[] = TEXT("Unix2VsMappingObject");

	if (strlen (m_strFilePath) <= 0)
		return FALSE;

	hFile = CreateFile( m_strFilePath, 
					GENERIC_READ | GENERIC_WRITE,
					0, 
					NULL,
					CREATE_ALWAYS, 
					FILE_ATTRIBUTE_NORMAL, 
					NULL
					);

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	hMapFile = CreateFileMapping(hFile, 
			NULL, 
			PAGE_READWRITE, 
			2, // 8G = 4G*2
			0, 
			szName );

	if (INVALID_HANDLE_VALUE == hMapFile || NULL == hMapFile)
	{
		// "Failed: on Create File Mapping\r\n"
		goto ExitClean;
	}

	pBuff = MapViewOfFile(hMapFile,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		1*1024*1024*1024);
	
	if (pBuff != NULL)
	{
		memcpy(pBuff, "TEST", 4);
		UnmapViewOfFile(pBuff);
	}	
	*/

	return FALSE;
}
