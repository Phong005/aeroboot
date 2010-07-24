// File.h: interface for the CFile class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FILE_H__5F4E8256_9B58_41B9_A35C_125D67103A7C__INCLUDED_)
#define AFX_FILE_H__5F4E8256_9B58_41B9_A35C_125D67103A7C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CFile  
{
public:
	BOOL OpenFile ();
	CFile();
	CFile(LPCTSTR lpFileName);
	virtual ~CFile();

protected:
	CHAR m_strFilePath[MAX_PATH];
};

#endif // !defined(AFX_FILE_H__5F4E8256_9B58_41B9_A35C_125D67103A7C__INCLUDED_)
