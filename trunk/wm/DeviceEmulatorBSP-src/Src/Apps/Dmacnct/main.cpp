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
#include <windows.h>
#include <ras.h>
#include "dmacnect.h"


VOID DeleteLink(HINSTANCE hinst)
{
    TCHAR szDMAcnectLnk[256];
    LoadString(hinst, IDS_DMACNECT_LINK, szDMAcnectLnk, 256);
    BOOL fDeleted = DeleteFile(szDMAcnectLnk);
    if (!fDeleted)
    {
        DWORD dwDeletedError = GetLastError();
        DEBUGMSG(1, (TEXT("DMAcnect.lnk not deleted.  Error %i\r\n"), (UINT) dwDeletedError));
    }
}

 VOID CreateRASEntry(HINSTANCE hinst) 
 {
     DWORD           cb;
     RASENTRY        RasEntry;
 
     TCHAR name[256];
     LoadString(hinst, IDS_DEFAULT_NAME, name, 256);
 
     // This will create the default entries if the key does not exist. 
     RasEntry.dwSize = sizeof(RASENTRY);
     cb = sizeof(RASENTRY);
     RasGetEntryProperties (NULL, TEXT(""), &RasEntry, &cb, NULL, NULL);
 
     // Now set up the entry the way we want it (like "`115200 Default")
     LoadString(hinst, SOCKET_FRIENDLY_NAME, RasEntry.szDeviceName, RAS_MaxDeviceName + 1);
 
     // And finally, write the new entry out
     if ( RasSetEntryProperties (NULL, name,
                                 &RasEntry, sizeof(RasEntry), NULL, 0) ) 
     {
         DEBUGMSG (1, (TEXT("Error %d from RasSetEntryProperties\r\n"),
                       GetLastError()));
     } 
     else 
     {
         HKEY hKey;
         DWORD dwDisp;
         DEBUGMSG (1, (TEXT("RasEntry '%s' Created\r\n"), name));
         if (ERROR_SUCCESS==RegCreateKeyEx(HKEY_CURRENT_USER, RK_CONTROLPANEL_COMM, 0, NULL, REG_OPTION_NON_VOLATILE,
                KEY_ALL_ACCESS, NULL, &hKey, &dwDisp))
         {
            RegSetValueEx(hKey, RV_CNCT, 0, REG_SZ, (LPBYTE)name, sizeof(TCHAR)*(1+lstrlen(name)));
            RegCloseKey(hKey);
         }
     }
 
     // Now, delete the link file.
     DeleteLink(hinst);
}
 

int WINAPI WinMain(
    HINSTANCE hinst,
    HINSTANCE hinstPrev,
    LPWSTR szCmdLine,
    int iCmdShow
)
{
    CreateRASEntry(hinst);
    return 0;
}

