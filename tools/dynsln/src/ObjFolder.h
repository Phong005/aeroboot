// ObjFolder.h: interface for the CObjFolder class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_OBJFOLDER_H__5BD11AE7_DC04_4B6E_A4F8_8F8F2C79C116__INCLUDED_)
#define AFX_OBJFOLDER_H__5BD11AE7_DC04_4B6E_A4F8_8F8F2C79C116__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ObjFileEntry.h"

#define USE_OUTPUT_FILE	1

class FileEntry;

class CObjFolder : public CObjFileEntry  
{
public:
	void Output(char *format, ...);
	void IndentLine();
	int m_nIndent;
	void process(FileEntry *entry );
	char * GetCurrentDirName (char *lpPath);
	bool Recusive(char *lpstrRoot);
	CObjFolder();
	virtual ~CObjFolder();

	BOOL CreateVCPrjInfo(char *lpstrRoot);
	DWORD LoadProjFile(char* szResName);

protected:
	int m_nNameOffset;

	char m_strVCPrjPath[MAX_PATH];		// c:\\myprj.vcproj
	char m_strVCPrjName[MAX_PATH];		// myprj.vcproj

#ifdef USE_OUTPUT_FILE
	HANDLE m_hOutputFile;
	void OutputToFile(const char* szInfo);
#endif // USE_OUTPUT_FILE
	

};

#endif // !defined(AFX_OBJFOLDER_H__5BD11AE7_DC04_4B6E_A4F8_8F8F2C79C116__INCLUDED_)
