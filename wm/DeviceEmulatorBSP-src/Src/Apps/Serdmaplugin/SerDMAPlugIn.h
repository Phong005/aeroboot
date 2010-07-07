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

#pragma once

#include <windows.h>
#include <ocidl.h>




//
// Class IDs
//
class __declspec(uuid("f672ddb2-bb6f-4b8c-b580-e8b71758e394")) 
CLS_CSERDMAPlugin;

STDAPI_(ULONG) DllAddRef(void);
STDAPI_(ULONG) DllRelease(void);

//********************************************************************************************
//  CSERDMAPlugSite
//
//  Purpose:
//      Implements the IObjectWithSite interface.
//
//********************************************************************************************
class CSERDMAPlugSite : public IObjectWithSite
{
    
public:

    CSERDMAPlugSite();
    ~CSERDMAPlugSite();

   //  IUnknown methods
    HRESULT STDMETHODCALLTYPE QueryInterface (REFIID rif, LPVOID* ppobj);
    ULONG   STDMETHODCALLTYPE AddRef         ();
    ULONG   STDMETHODCALLTYPE Release        ();

    STDMETHOD(SetSite)(IUnknown* pUnkSite);
    STDMETHOD(GetSite)(REFIID riid, void** ppvSite);

private:
    long   m_lRefCount;

};

//********************************************************************************************
//  CSERDMAPlugIn
//
//  Purpose:
//      Implements the IContextMenu interface.
//
//********************************************************************************************
class CSERDMAPlugIn : public IContextMenu
{

public:
    CSERDMAPlugIn();
    ~CSERDMAPlugIn();

   //  IUnknown methods
    HRESULT STDMETHODCALLTYPE QueryInterface (REFIID rif, LPVOID* ppobj);
    ULONG   STDMETHODCALLTYPE AddRef         ();
    ULONG   STDMETHODCALLTYPE Release        ();

   //IContextMenu functions
    STDMETHOD(QueryContextMenu)(HMENU hmenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags);
    STDMETHOD(InvokeCommand)(LPCMINVOKECOMMANDINFO lpici);
    STDMETHOD(GetCommandString)(UINT idCmd, UINT uType, UINT * pwReserved, LPSTR pszName, UINT cchMax);




private:
    long         m_lRefCount;
    CSERDMAPlugSite* pObjectWithSite;


};



//********************************************************************************************
//  CClassFactory
//
//  Purpose:
//      Implements the IClassFactory interface.
//
//********************************************************************************************
class CClassFactory : public IClassFactory
{
public:
    CClassFactory() : m_cRef(1) {DllAddRef();};
    ~CClassFactory() {DllRelease();};

    STDMETHOD(QueryInterface)(REFIID riid, void FAR* FAR* ppv);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

    // *** IClassFactory methods ***
    STDMETHOD(CreateInstance)(LPUNKNOWN pUnkOuter,
                              REFIID riid,
                              LPVOID FAR* ppvObject);
    STDMETHOD(LockServer)(BOOL fLock);

private:
    LONG m_cRef;

};


BOOL FindRNAAppWindow(HWND hWnd, LPARAM lParam);
BOOL FindAppWindow(HWND hWnd, LPARAM lParam);
