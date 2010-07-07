//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include "aygshell.h"
#include <ras.h>
#include <resource.h>

#include "SERDMAPlugIn.h"
#include "SERDMAASGUID.h"

#define ARRAYSIZE(a)    (sizeof(a)/sizeof((a)[0]))

static const GUID g_guidCSERDMAPlugin  = __uuidof(CLS_CSERDMAPlugin);
static LONG s_clockDLL = 0;
static LONG s_crefDLL = 0;

//Context Menu registry entries
const WCHAR c_wszRegKey[]          = L"SOFTWARE\\Microsoft\\Shell\\Extensions\\ContextMenus\\ActiveSync\\ActiveSyncConnect\\{f672ddb2-bb6f-4b8c-b580-e8b71758e394}";
const WCHAR c_wszDelay[]           = L"DelayLoad";

//COM registration
const WCHAR c_wszCLSID[]           = L"CLSID\\{f672ddb2-bb6f-4b8c-b580-e8b71758e394}\\InProcServer32";
WCHAR c_wszDll[]                   = L"SERDMAASPlugIn.dll";

const CHAR  c_szCommand[]          = "CONNECT";         //Invoke command
const WCHAR c_wszRnaCmdParam[]     = L"-n -m -e\"%s\""; //RNAAPP command line options
WCHAR c_wszSERDMAReplEntryName[]      = L"`DMA Default"; //move to resource file for localization
const WCHAR c_wszRnaApp[]          = L"rnaapp.exe"; 
    
const WCHAR c_wszDialog[]          = L"Dialog";         //RNAAPP dialog window
const WCHAR c_wszReplWnd[]         = L"ReplLog";        //Sync Application

WCHAR c_wszSERDMALineDeviceFriendlyName[] = L"Serial over DMA"; // This is the FriendlyName string from HKLM\Drivers\SERDMA

HINSTANCE   g_hInstance;


// ******************************************************************************************
//
// DLL entry point
//
// ******************************************************************************************
BOOL WINAPI DllMain(HANDLE hinstDll, DWORD dwReason, LPVOID /*lpReserved*/)
{
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
            g_hInstance =  (HINSTANCE)hinstDll;
            break;

       case DLL_PROCESS_DETACH:
            break;
     }
    return TRUE;
}

//********************************************************************************************
//  CClassFactory::QueryInterface
//
//  Purpose:
//      Standard QI.  We support IClassFactory and IUnknown.
//
//  Parameters:
//      riid   = INPUT  - IID of interface to QI for
//      ppvObj = OUTPUT - where interface point is placed
//
//  Returns:
//      HRESULT indicating success
//********************************************************************************************
HRESULT CClassFactory::QueryInterface(REFIID riid, void **ppvObj)
{
    if ((riid == IID_IUnknown) || (riid == IID_IClassFactory))
    {
        *ppvObj = this;
        AddRef();
        return S_OK;
    }
    else
    {
        *ppvObj = NULL;
        return E_NOINTERFACE;
    }
}


//********************************************************************************************
//  CClassFactory::AddRef
//
//  Purpose: Increments the reference count by one
//  
//  Returns:
//      ULONG indicating current ref count
//********************************************************************************************
ULONG CClassFactory::AddRef(void)
{
    return InterlockedIncrement(&m_cRef);
}


//********************************************************************************************
//  CClassFactory::Release
//
//  Purpose:
//      Decrements the reference count by one, and destroys the object if there are no more
//      references.
//
//  Returns:
//      ULONG indicating current ref count
//********************************************************************************************
ULONG CClassFactory::Release(void)
{
    ULONG RefCount;
    RefCount = InterlockedDecrement(&m_cRef);
    if (RefCount==0)
    {
        delete this;
    }

    return RefCount;
}


//********************************************************************************************
//  CClassFactory::CreateInstance
//
//  Purpose:
//      Created a instance of a class
//
//  Parameters:
//      pUnkOuter = INPUT  - Outer object
//      riid      = INPUT  - Class id on the object to create
//      ppvObj    = OUTPUT - new object created
//
//  Returns:
//      HRESULT indicating success
//********************************************************************************************
HRESULT CClassFactory::CreateInstance(LPUNKNOWN pUnkOuter,
                                      REFIID riid,
                                      LPVOID FAR* ppvObj)
{
    HRESULT Result;

    if (pUnkOuter)
    {
        Result=E_INVALIDARG; // No aggregations
    }
    else
    {
        IUnknown *punk = new CSERDMAPlugIn();
        if (punk)
        {
            Result = punk->QueryInterface(riid, ppvObj);
            punk->Release();
        }
        else
        {
            Result = E_OUTOFMEMORY;
        }
    }
    return Result;
}


//********************************************************************************************
//  CClassFactory::LockServer
//
//  Purpose:
//      Called by the client of a class object to keep a server open in memory, 
//      allowing instances to be created more quickly.
//
//  Parameters:
//      fLock = INPUT  - If TRUE, increments the lock count; if FALSE, decrements the lock count. 
//
//  Returns:
//      HRESULT indicating success
//********************************************************************************************
STDMETHODIMP CClassFactory::LockServer(BOOL fLock)
{
    if (fLock)
    {
        InterlockedIncrement(&s_clockDLL);
    }
    else
    {
        InterlockedDecrement(&s_clockDLL);
    }

    return NOERROR;
}


//********************************************************************************************
//  DllAddRef
//
//  Purpose: Increments the reference count by one
//  
//********************************************************************************************
STDAPI_(ULONG) DllAddRef(void)
{
    ULONG ulRet = InterlockedIncrement(&s_crefDLL);
    ASSERT(ulRet < 1000);   // reasonable upper limit
    return ulRet;
}

//********************************************************************************************
//  DllRelease
//
//  Purpose:
//      Decrements the reference count by one
//
//********************************************************************************************
STDAPI_(ULONG) DllRelease(void)
{
    ULONG ulRet = InterlockedDecrement(&s_crefDLL);
    ASSERT(ulRet >= 0);      // don't underflow
    return ulRet;
}


//********************************************************************************************
//  DllRelease
//
//  Purpose:
//      This function determines whether the dynamic-link library (DLL) that implements this 
//      function is in use. If it is not, the caller can safely unload the DLL from memory.
//
//********************************************************************************************
STDAPI DllCanUnloadNow()
{
    HRESULT Result = ResultFromScode((s_crefDLL == 0) && (s_clockDLL == 0) ? S_OK : S_FALSE);
    return Result;
}


//********************************************************************************************
//  DllRelease
//
//  Purpose:
//      This function retrieves the class object from a DLL object handler or object 
//      application. DllGetClassObject is called from within the CoGetClassObject function 
//      when the class context is a DLL.
//
//  Parameters:
//      rclsid = INPUT  - CLSID that will associate the correct data and code. 
//      riid   = INPUT  - Reference to the identifier of the interface 
//      ppvObj = OUTPUT - Address of pointer variable that receives the interface pointer requested in riid
//
//  Returns:
//      HRESULT indicating success
//
//********************************************************************************************
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID FAR* ppvObj)
{
    HRESULT Result;

    *ppvObj = NULL;
    if(g_guidCSERDMAPlugin != rclsid)
    {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    CClassFactory *pcf = new CClassFactory();
    if (!pcf)
    {
        return E_OUTOFMEMORY;
    }

    // Note if the QueryInterface fails, the Release will delete the object
    Result = pcf->QueryInterface(riid, ppvObj);
    pcf->Release();

    return Result;
}

extern "C" HRESULT WINAPI OneStopFactory(LPCWSTR /*pszType*/, IContextMenu ** ppObj)
{
    *ppObj = new CSERDMAPlugIn;

    if (*ppObj == NULL)
    {
        return E_OUTOFMEMORY;
    }
    return S_OK;
}


//********************************************************************************************
//  CSERDMAPlugIn::constructor
//********************************************************************************************
CSERDMAPlugIn::CSERDMAPlugIn() :
    m_lRefCount(1),
    pObjectWithSite(NULL)
{
    DllAddRef();
}

//********************************************************************************************
//  CSERDMAPlugIn::destructors
//********************************************************************************************
CSERDMAPlugIn::~CSERDMAPlugIn()
{
    if(pObjectWithSite)
    {
        pObjectWithSite->Release();
    }
    DllRelease();
}
    


//********************************************************************************************
//  CSERDMAPlugIn::QueryInterface
//
//  Purpose:
//      Standard QI.  We support IMassNotifyLayer and IUnknown.
//
//  Parameters:
//      riid = INPUT - IID of interface to QI for
//      ppv = OUTPUT - where interface point is placed
//
//  Returns:
//      HRESULT indicating success
//********************************************************************************************
STDMETHODIMP CSERDMAPlugIn::QueryInterface(REFIID riid, LPVOID *ppv)
{
    HRESULT hr = S_OK;

    //
    //  Check parameters
    //
    if (ppv == NULL)
    {
        hr = E_INVALIDARG;
        goto Exit;
    }
    
    //
    //  Initialize [out] parameters
    //
    *ppv = NULL;
    
    //
    //  Find IID
    //
    if (riid == IID_IUnknown)
    {
        *ppv = static_cast<IUnknown*>(this);
    }
    else if (riid == IID_IContextMenu)  //IID_IContextMenu
    {
        *ppv = static_cast<IContextMenu*>(this);
    }
    else if (riid == IID_IObjectWithSite) //IID_IObjectWithSite
    {
        if(NULL == pObjectWithSite)
        {
            pObjectWithSite = new CSERDMAPlugSite;
            if(NULL == pObjectWithSite)
            {
                hr = E_OUTOFMEMORY;
                goto Exit;
            }
        }
        *ppv = static_cast<IObjectWithSite*>(pObjectWithSite);
    }
    else
    {
        hr = E_NOINTERFACE;
        goto Exit;
    }
    
    //
    //  AddRef the outgoing interface
    //
    static_cast<IUnknown *>(*ppv)->AddRef();
    hr = S_OK;

Exit:
    return hr;
}


//********************************************************************************************
//  CSERDMAPlugIn::AddRef
//
//  Purpose:
//      Standard IUnknown::AddRef() implementation.
//
//  Returns:
//      ULONG indicating ref count
//********************************************************************************************
STDMETHODIMP_(ULONG) CSERDMAPlugIn::AddRef(void)
{
    return ::InterlockedIncrement(&m_lRefCount);
}


//********************************************************************************************
//  CSERDMAPlugIn::Release
//
//  Purpose:
//      Standard IUnknown::Release implementation.
//
//  Returns:
//      ULONG indicating current ref count
//********************************************************************************************
STDMETHODIMP_(ULONG) CSERDMAPlugIn::Release(void)
{
    if (::InterlockedDecrement(&m_lRefCount) == 0)
    {
        delete this;
        return 0;
    }

    return m_lRefCount;
}




//********************************************************************************************
//  CSERDMAPlugIn::QueryContextMenu
//
//  Purpose:
//      Adds commands to a shortcut menu.
//
//  Parameters:
//      hmenu      = INPUT - Handle to the menu.
//      indexMenu  = INPUT - Zero-based position at which to insert the first menu item. 
//      idCmdFirst = INPUT - Minimum value that the handler can specify for a menu item identifier.
//      idCmdLast  = INPUT - Maximum value that the handler can specify for a menu item identifier.
//      uFlags     = INPUT - Optional flags specifying how the shortcut menu can be changed.
//
//  Returns:
//      HRESULT indicating success
//********************************************************************************************
STDMETHODIMP CSERDMAPlugIn::QueryContextMenu(HMENU hmenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags)
{
    WCHAR *szToolBar = (WCHAR*)LoadString (g_hInstance, IDS_SERDMASYNC_TOOLBAR, NULL, 0);

	//Add the menu item
    BOOL fSuccess = InsertMenu(hmenu, indexMenu, MF_BYPOSITION | MF_STRING, idCmdFirst, szToolBar);

    HRESULT hr = E_FAIL;
    if (fSuccess) {
        hr = S_OK | 1; // Added 1 menu item.
	}

    return hr;

}

//********************************************************************************************
//  CSERDMAPlugIn::InvokeCommand
//
//  Purpose:
//      Carries out the command associated with a shortcut menu item.
//
//  Parameters:
//      lpici = INPUT - Pointer to a CMINVOKECOMMANDINFO structure containing information 
//                      about the command.
//
//  Returns:
//      HRESULT indicating success
//********************************************************************************************
STDMETHODIMP CSERDMAPlugIn::InvokeCommand(LPCMINVOKECOMMANDINFO lpici)
{
    HRESULT hr = S_OK;
    WCHAR  wszCmdLine[64];
	HWND hwndRna = NULL;

    if(strcmp(c_szCommand, lpici->lpVerb))
    {
        hr = E_NOTIMPL;
        goto Exit;
    }
    
	//Look for a running instance of RNAApp
    EnumWindows(FindRNAAppWindow, (LPARAM)&hwndRna);
    
    if ( !hwndRna ) //None found, so start it now
    {
        wsprintf( wszCmdLine, c_wszRnaCmdParam, c_wszSERDMAReplEntryName);
       
        if ( !CreateProcess( c_wszRnaApp, wszCmdLine, NULL, NULL, FALSE, 0, NULL, NULL, NULL, NULL ) )
        {
            hr = E_FAIL;
            goto Exit;
        }
        
    }
    else //Found a previous instance
    {
        RASCONN RasConn[4];
        DWORD   dwSize = sizeof(RasConn);
        DWORD   dwConn = 0;
        BOOL    fSimConn = FALSE;


		//Check to see if we are already connected
        RasConn[0].dwSize = sizeof(RASCONN);
        if ( !RasEnumConnections( RasConn, &dwSize, &dwConn ) )
        {
            DWORD i=0;
            for (i=0;i<dwConn;i++)
            {
                RASCONNSTATUS RasConnStatus;
                if ( !lstrcmp( RasConn[i].szEntryName, c_wszSERDMAReplEntryName ) )
                {
                    if ( !RasGetConnectStatus( RasConn[i].hrasconn, &RasConnStatus ) )
                    {
                        if ( RasConnStatus.rasconnstate == RASCS_Connected )
                        {
                            fSimConn = TRUE;
                            HWND hwndRepllog = FindWindow(c_wszReplWnd, NULL); //Look for the active sync window
                            PostMessage( hwndRna, RNA_RASCMD, (WPARAM)RNA_GETINFO, (LPARAM)hwndRepllog );
                        }
                    }
                    break;
                }
            }
        }

        if ( !fSimConn )
            hr = E_FAIL;
    }


Exit:
    return hr;
}

//********************************************************************************************
//  CSERDMAPlugIn::GetCommandString
//
//  Purpose:
//      Retrieves information about a shortcut menu command, including the Help string and 
//      the language-independent, or canonical, name for the command.
//
//  Parameters:
//      idCmd      = INPUT - Menu command identifier offset.
//      uType      = INPUT - Flags specifying the information to return
//      pwReserved = INPUT - Reserved.
//      pszName    = INPUT - Address of the buffer to receive the null-terminated string 
//                           being retrieved.
//      cchMax     = INPUT - Size of the buffer to receive the null-terminated string. 
//
//  Returns:
//      HRESULT indicating success
//********************************************************************************************
STDMETHODIMP CSERDMAPlugIn::GetCommandString(UINT idCmd, UINT uType, UINT * pwReserved, LPSTR pszName, UINT cchMax)
{

    if(cchMax < sizeof(c_szCommand))
    {
        return E_FAIL;
    }

    strcpy(pszName, c_szCommand);

    return S_OK;;
}




//********************************************************************************************
//  CSERDMAPlugSite::constructor
//********************************************************************************************
CSERDMAPlugSite::CSERDMAPlugSite() :
    m_lRefCount(1)
{
    DllAddRef();
}

//********************************************************************************************
//  CSERDMAPlugSite::destructors
//********************************************************************************************
CSERDMAPlugSite::~CSERDMAPlugSite()
{
    DllRelease();
};

//********************************************************************************************
//  CSERDMAPlugSite::QueryInterface
//
//  Purpose:
//      Standard QI.  We support IMassNotifyLayer and IUnknown.
//
//  Parameters:
//      riid = INPUT - IID of interface to QI for
//      ppv = OUTPUT - where interface point is placed
//
//  Returns:
//      HRESULT indicating success
//********************************************************************************************
STDMETHODIMP CSERDMAPlugSite::QueryInterface(REFIID riid, LPVOID *ppv)
{
    HRESULT hr = S_OK;

    //
    //  Check parameters
    //
    if (ppv == NULL)
    {
        hr = E_INVALIDARG;
        goto Exit;
    }
    
    //
    //  Initialize [out] parameters
    //
    *ppv = NULL;
    
    //
    //  Find IID
    //
    if (riid == IID_IUnknown)
    {
        *ppv = static_cast<IUnknown*>(this);
    }
    else if (riid == IID_IObjectWithSite)
    {
        *ppv = static_cast<IObjectWithSite*>(this);
    }
    else
    {
        hr = E_NOINTERFACE;
        goto Exit;
    }
    
    //
    //  AddRef the outgoing interface
    //
    static_cast<IUnknown *>(*ppv)->AddRef();
    hr = S_OK;

Exit:
    return hr;
}


//********************************************************************************************
//  CSERDMAPlugSite::AddRef
//
//  Purpose:
//      Standard IUnknown::AddRef() implementation.
//
//  Returns:
//      ULONG indicating ref count
//********************************************************************************************
STDMETHODIMP_(ULONG) CSERDMAPlugSite::AddRef(void)
{
    return ::InterlockedIncrement(&m_lRefCount);
}


//********************************************************************************************
//  CSERDMAPlugSite::Release
//
//  Purpose:
//      Standard IUnknown::Release implementation.
//
//  Returns:
//      ULONG indicating current ref count
//********************************************************************************************
STDMETHODIMP_(ULONG) CSERDMAPlugSite::Release(void)
{
    if (::InterlockedDecrement(&m_lRefCount) == 0)
    {
        delete this;
        return 0;
    }

    return m_lRefCount;
}

//********************************************************************************************
//  CSERDMAPlugIn::SetSite
//
//  Purpose:
//      Sets the site for the internal pointer to punkParent.
//
//  Parameters:
//      pUnkSite = Pointer to the IUnknown interface pointer of the site managing this object.
//
//  Returns:
//      HRESULT indicating success
//********************************************************************************************
STDMETHODIMP CSERDMAPlugSite::SetSite(IUnknown* pUnkSite)
{

    return S_OK;
}

//********************************************************************************************
//  CSERDMAPlugIn::GetSite
//
//  Purpose:
//      This method queries the site for a pointer to the interface identified by riid
//
//  Parameters:
//      riid    = INPUT -  The IID of the interface pointer that should be returned in ppvSite. 
//      ppvSite = OUTPUT - Address of pointer variable that receives the interface pointer 
//                         requested in riid.
//
//  Returns:
//      HRESULT indicating success
//********************************************************************************************
STDMETHODIMP CSERDMAPlugSite::GetSite(REFIID riid, void** ppvSite)
{
    return E_FAIL;
}



//********************************************************************************************
//  DllRegisterServer
//
//  Purpose:
//      This function instructs an in-process server to create its registry entries for 
//      all classes supported in this server module. If this function fails, the state of 
//      the registry for all its classes is indeterminate.
//
//  Returns:
//      HRESULT indicating success
//********************************************************************************************
STDAPI DllRegisterServer()
{

    LONG lRet;
    HKEY hKey = NULL;
    DWORD dwTrue = TRUE;
    DWORD dwDisposition;
    HRESULT hr = E_FAIL;
    WCHAR *szToolBar = (WCHAR*)LoadString (g_hInstance, IDS_SERDMASYNC_TOOLBAR, NULL, 0);
   
    //IContexMenu
    lRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, c_wszRegKey, 0, NULL, 0, 0, NULL, &hKey, &dwDisposition);
    if(ERROR_SUCCESS != lRet)
    {
        goto Exit;
    }

    lRet = RegSetValueEx(hKey, NULL, 0, REG_SZ, (BYTE*)szToolBar, (wcslen(szToolBar)+1)*sizeof(WCHAR));
    if(ERROR_SUCCESS != lRet)
    {
        goto Exit;
    }

    lRet = RegSetValueEx(hKey, c_wszDelay, 0, REG_DWORD, (BYTE*)&dwTrue, sizeof(dwTrue));
    if(ERROR_SUCCESS != lRet)
    {
        goto Exit;
    }

    RegCloseKey(hKey);
    hKey = NULL;

    //Com Dll
    lRet = RegCreateKeyEx(HKEY_CLASSES_ROOT, c_wszCLSID, 0, NULL, 0, 0, NULL, &hKey, &dwDisposition);
    if(ERROR_SUCCESS != lRet)
    {
        goto Exit;
    }

    lRet = RegSetValueEx(hKey, NULL, 0, REG_SZ, (BYTE*)c_wszDll, sizeof(c_wszDll));
    if(ERROR_SUCCESS != lRet)
    {
        goto Exit;
    }

    RegCloseKey(hKey);
    hKey = NULL;
    hr = S_OK;


Exit:

    if(hKey)
    {
        RegCloseKey(hKey);
    }

   return hr;

}

//********************************************************************************************
//  DllUnregisterServer
//
//  Purpose:
//      This function instructs an in-process server to create its registry entries for 
//      all classes supported in this server module. If this function fails, the state of 
//      the registry for all its classes is indeterminate.
//
//  Returns:
//      HRESULT indicating success
//********************************************************************************************
STDAPI DllUnregisterServer()
{
    WCHAR wszSubKey[MAX_PATH];
    LONG lRet;
   
    //IContexMenu
    lRet = RegDeleteKey(HKEY_LOCAL_MACHINE, c_wszRegKey);
    ASSERT(ERROR_SUCCESS == lRet);

    //Com Dll
    lRet = RegDeleteKey(HKEY_CLASSES_ROOT, c_wszCLSID);
    ASSERT(ERROR_SUCCESS == lRet);

    //RAS connectoid created, if any
    StringCchPrintf( wszSubKey, ARRAYSIZE(wszSubKey), TEXT("comm\\rasbook\\%s"), c_wszSERDMAReplEntryName);
    lRet = RegDeleteKey(HKEY_CURRENT_USER, wszSubKey);

    return S_OK;
}


//********************************************************************************************
//  FindRNAAppWindow
//
//  Purpose:
//      This function is an application-defined callback function that receives top-level 
//      window handles as a result of a call to the EnumWindows function. 
//
//  Returns:
//      TRUE continues enumeration. FALSE stops enumeration.
//********************************************************************************************
BOOL FindRNAAppWindow(HWND hWnd, LPARAM lParam)
{
    TCHAR   szClassName[32];

    GetClassName(hWnd, szClassName, sizeof(szClassName)/sizeof(TCHAR));

	//We are looking for a window with a classname that isn't dialog
	//and has the window's user data set to RNAAPP_MAGIC_NUM
    if ( !_tcscmp (szClassName, c_wszDialog ) &&
         (RNAAPP_MAGIC_NUM == GetWindowLong(hWnd, DWL_USER)) )
    {
        *((HWND *)lParam) = hWnd;
        return FALSE;
    }
    return TRUE;
}




