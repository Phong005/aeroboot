// ObjFolder.cpp: implementation of the CObjFolder class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ObjFolder.h"
#include <string.h>
#include <io.h>


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CObjFolder::CObjFolder()
{
	m_nIndent = 0;

	m_nNameOffset = 0;

	m_hOutputFile = INVALID_HANDLE_VALUE;
}

CObjFolder::~CObjFolder()
{
#ifdef USE_OUTPUT_FILE
	if(INVALID_HANDLE_VALUE != m_hOutputFile)
	{
		::CloseHandle(m_hOutputFile);
		m_hOutputFile = INVALID_HANDLE_VALUE;
	}
#endif
}

#ifdef USE_OUTPUT_FILE

void CObjFolder::OutputToFile(const char* szInfo)
{
	if(NULL == szInfo || 0 == lstrlenA(szInfo) || lstrlenA(m_strVCPrjPath) == 0)
		return;	// 没有必要记录
	
	if(NULL == m_hOutputFile || INVALID_HANDLE_VALUE == m_hOutputFile)
	{
		m_hOutputFile = ::CreateFileA(m_strVCPrjPath,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		
		if(INVALID_HANDLE_VALUE == m_hOutputFile)
		{
			m_hOutputFile = ::CreateFileA(m_strVCPrjPath,
				GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL, CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
		}
		
		//if(INVALID_HANDLE_VALUE != m_hOutputFile)
		//{
		//	return;
		//}
	}
	
	::SetFilePointer(m_hOutputFile, 0, NULL, FILE_END);
	
	DWORD dwWrite = lstrlenA(szInfo);
	DWORD dwWritten = 0;
	::WriteFile(m_hOutputFile, szInfo, dwWrite, &dwWritten, NULL);
}
#endif

#define HNULL INVALID_HANDLE_VALUE
//#define _A_SUBDIR FILE_ATTRIBUTE_DIRECTORY
//#define _A_HIDDEN FILE_ATTRIBUTE_HIDDEN

class FileEntry {
	// This encapsulates stuff about searching directories.  Creating a
	// FileEntry starts searching for files that fit a specified mask.  
	// You  can then check whether a file was actually found, and if so,
	// its name, size, attributes, etc.  After you're done with the
	// current file, you find the next one by calling next().
	//
	// This doesn't expose the full generality of the WIN32_FIND_DATA,
	// such as the short name of the file, or whether its archive
	// attribute is set.  I've simply never needed them...
	//
    WIN32_FIND_DATA file_;
    int    found_;
    HANDLE handle_;
	
    BOOL isDir() const {
        return 0 != (file_.dwFileAttributes & _A_SUBDIR);
    }
	
public:
	
    FileEntry(char *mask) {
        found_ = (HNULL != (handle_ = FindFirstFile(mask, &file_)));
    }
	
    BOOL next() { return found_ = FindNextFile(handle_, &file_); }
	
    BOOL found() const {
        return 0!=found_;
    }
	
    char const *name() const { return file_.cFileName; }
	
    long size()  const          { return file_.nFileSizeLow; }
    BOOL isSubdir() const       { return isDir() && ('.' != name()[0]); }
    BOOL isHidden() const {
        return 0 != (file_.dwFileAttributes & _A_HIDDEN);
    }
	
    ~FileEntry() { FindClose(handle_); }
	
};

bool CObjFolder::Recusive(char *lpstrRoot)
{
	char	szBuf[MAX_PATH * 3] = {0};

	// get the folder name
	char	*p = GetCurrentDirName(lpstrRoot);

	IndentLine(); 

	// output
	sprintf(szBuf, "<Filter	Name=\"%s\">\n", p);
	Output (szBuf);
	
	//////////////////////////////////////////////////////////////////////////
	// enumerates files and folders.
	
	// The real guts of Search - carries out the actual search.
	FileEntry *entry_;
	

	// 1st, looks for files.
	for (
		entry_ = new FileEntry("*");
	entry_->found();
	entry_->next() )
	{
		if (entry_ ->isSubdir())
		{
			continue;
		}
		else
			process(entry_);
	}
	delete entry_;

	// 2nd, looks for sub - folders.
	for (
		entry_ = new FileEntry("*");
	entry_->found();
	entry_->next() )
	{
		if (entry_ ->isSubdir())
		{
			if (entry_ ->isHidden ())
				continue;

			m_nIndent ++;

			SetCurrentDirectory(entry_->name());
			Recusive((char*)entry_->name());
			SetCurrentDirectory("..");

			m_nIndent --;
		}
// 		else
// 			process(entry_);
	}
	delete entry_;
	
	// output.
	IndentLine();
	Output ("</Filter>\n");

	return true;
}


char* CObjFolder::GetCurrentDirName(char *lpPath)
{
	char	*p = NULL;

	if (strchr(lpPath, ':') <= 0)
		return lpPath;

ReScan:
	p = strrchr(lpPath, '\\');
	if (p == NULL)
		return NULL;

	p++;

	if (*p == '\0')
	{
		p--;
		*p = 0x0;

		goto ReScan;
	}
	return p;
}

void CObjFolder::process(FileEntry *entry)
{
	char		szPath[MAX_PATH] = {0};
	char		szRelPath[MAX_PATH] = {0};
	char		*pRelPath = szPath;

	if (strnicmp(entry->name(), ".", 1) == 0 ||
		strnicmp(entry->name(), "..", 2) == 0 )
		return;


	GetCurrentDirectory(MAX_PATH, szPath);

	strcat(szPath, "\\");
	strcat (szPath, entry ->name());

	m_nIndent ++;
	IndentLine();
	m_nIndent --;

	pRelPath += m_nNameOffset;

	//
	if (pRelPath[0] == '\\' || pRelPath[0] == '/')
	{
		pRelPath ++;
	}

	sprintf (szRelPath, ".\\%s", pRelPath);
	Output ("<File RelativePath=\"%s\"></File>\n", szRelPath/*szPath*/);
}



void CObjFolder::IndentLine()
{
	for (int i=0; i < m_nIndent; i ++)
		Output ("\t");
}

void CObjFolder::Output(char *format, ...)
{
	char	szBuf[1024 * 8] = {0};

	va_list		ap;

	va_start(ap, format);
	_vsnprintf (szBuf, 1024 * 8, format, ap);

#ifdef USE_OUTPUT_FILE
	OutputToFile(szBuf);
	#ifdef _DEBUG
	OutputDebugString(szBuf);
	#endif // _DEBUG
#else
	printf (szBuf);
#endif
	
	// OutputDebugString (szBuf);
	va_end(ap);
}

BOOL CObjFolder::CreateVCPrjInfo(char *lpstrRoot)
{
	int lastch = 0;

	if (lpstrRoot == NULL || strlen (lpstrRoot) <=0)
	{
		m_strVCPrjPath[0] = 0x0;
		m_strVCPrjName[0] = 0x0;

		return FALSE;
	}

	// c:\\myprogram
	strcpy (m_strVCPrjPath, lpstrRoot);
	lastch = lstrlenA(m_strVCPrjPath)-1;
	m_nNameOffset = lstrlenA(m_strVCPrjPath);

	if (m_strVCPrjPath[lastch] == '\\')
		m_strVCPrjPath[lastch] = 0x0;

	// myprogram
	char *p = strrchr (m_strVCPrjPath, '\\');
	if (p == NULL)
		return FALSE;

	strcpy (m_strVCPrjName, p+1);

	m_nNameOffset -= strlen (m_strVCPrjName) + 1;

	// c:\\myprogram.vcproj
	strcat (m_strVCPrjPath, ".vcproj");

	if (_access (m_strVCPrjPath, 2) == 0)
		DeleteFile(m_strVCPrjPath);
	return TRUE;
}

DWORD CObjFolder::LoadProjFile(char *szResName)
{
	HRSRC hRes;
	ULONG len = 0;
	HGLOBAL hG;
	
	if( szResName == NULL )
		return 0 ;
	
	
	/// Load engine.exe or engine.dll
	hRes = FindResource( NULL, szResName , "PROJ_STUB" );
	if( hRes == NULL )
		return 0;
	
				
	len= SizeofResource(NULL, hRes);
	hG = LoadResource(NULL, hRes);
	
	void* pBuf = (void*)LockResource( hG );
	if( pBuf == NULL )
		return 0;
	
	Output((char*)pBuf);
	
	return len;
}
