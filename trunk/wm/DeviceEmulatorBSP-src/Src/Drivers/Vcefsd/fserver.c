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
/*
    File:       FServer.C

    Contains:   Contains Glue code for the PC to call the Mac
                file server.

    Written by: Scott Coleman

    Copyright:  © 1996 Connectix Corporation

    Change History (most recent first):

*/

#pragma check_stack( off )
#pragma warning ( disable : 4035 )      // No return value warning
#pragma warning ( disable : 4704 )      // can't optimize inline asm warning

#include <windows.h>
#include    "FServer.h"
#include    "VCEFsd.h"

// WARNING!!! WARNING!!! WARNING!!! WARNING!!! WARNING!!! WARNING!!! WARNING!!! WARNING!!!
extern ServerPB*	gpServerPB; // Kinda hacky, but the virtual addr passed into these 
								// functions are mapoped to this physical addr.
								// This will only work under CE!!!
//WARNING!!! WARNING!!! WARNING!!! WARNING!!! WARNING!!! WARNING!!! WARNING!!! WARNING!!!


// ------------------------------------------------------------
//                      Global data
// ------------------------------------------------------------

UInt32 kMaxReadWriteSize = kDefMaxReadWriteSize ; // see ServerGetMaxIOSize()

// ------------------------------------------------------------
//                      Private Equates
// ------------------------------------------------------------

//  Most server functions are called by loading a ptr to a
//  ServerPB into DS:BX and executing a specific ISA Extension
//  instruction. This macro contains the inline asm to do this.

UInt16 CallFunction(unsigned __int32 inCode, ServerPB *inPB)
{
    unsigned __int32 Result;

    WaitForSingleObject(g_FolderShareMutex, INFINITE);
    v_FolderShareDevice->ServerPB = (unsigned __int32)gpServerPB;
    v_FolderShareDevice->Code = inCode; // this executes the function

    // Poll until the function call returns
    while (v_FolderShareDevice->IOPending) {
        v_FolderShareDevice->Code = kServerPollCompletion;
    }

    // Capture the result of the call before releasing the mutex
    Result = v_FolderShareDevice->Result;
    ReleaseMutex(g_FolderShareMutex);
    inPB->fResult = (UInt16)Result;
    return (UInt16)Result;
}

#define MakeFunction(inCode) \
    { return CallFunction(inCode, inPB);}

// ------------------------------------------------------------
//                      Server Glue Functions
// ------------------------------------------------------------



/* ------------------------------------------------------------
    ServerGetDriveConfig        [Macintosh]

    Gets the config info for a specific drive

    Parameters:
        In:     inPB            - Ptr to a Server Parameter block
                ->fVRefNum      - Which drive to get config info on
                                  0 - kMaxDrives-1
        Out:    inPB            - Info returned in the parameter block
                ->fResult       - Error code
                ->fVRefNum      - The drive's vRefNum
                ->fParentID     - the dirID of the root
------------------------------------------------------------ */
UInt16  ServerGetDriveConfig( ServerPB * inPB )
        MakeFunction ( kServerGetDriveConfig )

/* ------------------------------------------------------------
    ServerCreate                [Macintosh]

    Creates a file (or opens and truncates and existing file)
    and returns information on the new file.

    NOTE: For long file name support, set fDosName[0] = 0 and
          the long file name in fName

    Parameters:
        In:     inPB            - Ptr to a Server Parameter block
                ->fFlags        - 0 = DOS Names, 1 = LFN
                ->fVRefNum      - The volume ref num
                ->fParentID     - Directory ID
                ->u.fXXX.fName  - the file name
        Out:    inPB            - Info returned in the parameter block
                ->fResult       - Error code
------------------------------------------------------------ */
UInt16  ServerCreate( ServerPB * inPB )
        MakeFunction ( kServerCreate )

/* ------------------------------------------------------------
    ServerOpen                  [Macintosh]

    Opens an existing file in the mode specified by fOpenMode
    and returns information about the open file.

    Parameters:
        In:     inPB            - Ptr to a Server Parameter block
                ->fFlags        - 0 = DOS Names, 1 = LFN
                ->fVRefNum      - The volume ref num
                ->fParentID     - Directory ID
                ->u.fXXX.fName  - the file name
                ->fOpenMode     - Open mode attributes (read only...)
        Out:    inPB            - Info returned in the parameter block
                ->fResult       - Error code
                ->fHandle       - Handle to the file
------------------------------------------------------------ */
UInt16  ServerOpen( ServerPB * inPB )
        MakeFunction ( kServerOpen )

/* ------------------------------------------------------------
    ServerRead                  [Macintosh]

    Reads data from an already open file.

    NOTE: The ->fDTAPtr is loaded into ES:DI so that the Mac
          can get the segment base.

    NOTE: For 32 bit driver, ->fDTAPtr is loaded in ES:EDI.

    Parameters:
        In:     inPB            - Ptr to a Server Parameter block
                ->fHandle       - Handle to the file
                ->fSize         - # of bytes to read
                ->fPosition     - starting position to read from
                ->fDTAPtr       - Disk Transfer Address (PC read buffer)
        Out:    inPB            - Info returned in the parameter block
                ->fResult       - Error code
                ->fSize         - # of bytes actually Read
------------------------------------------------------------ */
UInt16  ServerRead( ServerPB * inPB )
    MakeFunction ( kServerRead )


/* ------------------------------------------------------------
    ServerWrite                 [Macintosh]

    Writes data to a open file.

    NOTE: The ->fDTAPtr is loaded into ES:DI so that the Mac
          can get the segment base.

    NOTE: For 32 bit driver, ->fDTAPtr is loaded in ES:EDI.

    Parameters:
        In:     inPB            - Ptr to a Server Parameter block
                ->fHandle       - Handle to the file
                ->fSize         - # of bytes to write
                ->fPosition     - starting position to write to
                ->fDTAPtr       - Disk Transfer Address (PC write buffer)
        Out:    inPB            - Info returned in the parameter block
                ->fResult       - Error code
                ->fSize         - # of bytes actually written
------------------------------------------------------------ */
UInt16  ServerWrite( ServerPB * inPB )
    MakeFunction ( kServerWrite )

/* ------------------------------------------------------------
    ServerSetEOF                [Macintosh]

    Sets the End of File mark for a file.

    Parameters:
        In:     inPB            - Ptr to a Server Parameter block
                ->fHandle       - Handle to the file
                ->fPosition     - The new EOF mark
        Out:    inPB            - Info returned in the parameter block
                ->fResult       - Error code
------------------------------------------------------------ */
UInt16  ServerSetEOF( ServerPB * inPB )
        MakeFunction ( kServerSetEOF )

/* ------------------------------------------------------------
    ServerClose                 [Macintosh]

    Closes an open file.

    Parameters:
        In:     inPB            - Ptr to a Server Parameter block
                ->fHandle       - Handle to the file
        Out:    inPB            - Info returned in the parameter block
                ->fResult       - Error code
------------------------------------------------------------ */
UInt16  ServerClose( ServerPB * inPB )
        MakeFunction ( kServerClose )

/* ------------------------------------------------------------
    ServerGetSpace              [Macintosh]

    Retruns the free space on a volume.

    Parameters:
        In:     inPB            - Ptr to a Server Parameter block
                ->fVRefNum      - The volume ref num
        Out:    inPB            - Info returned in the parameter block
                ->fResult       - Error code
                ->fSize         - Free space on the drive / 32k
                ->fPosition     - Total space on drive / 32k
------------------------------------------------------------ */
UInt16  ServerGetSpace( ServerPB * inPB )
        MakeFunction ( kServerGetSpace )

/* ------------------------------------------------------------
    ServerMkDir                 [Macintosh]

    Creates a new directory on the server.

    NOTE: For long file name support, set fDosName[0] = 0 and
          the long file name in fName

    Parameters:
        In:     inPB            - Ptr to a Server Parameter block
                ->fFlags        - 0 = DOS Names, 1 = LFN
                ->fVRefNum      - The volume ref num
                ->fParentID     - Directory ID
                ->u.fXXX.fName  - the file name
        Out:    inPB            - Info returned in the parameter block
                ->fResult       - Error code
------------------------------------------------------------ */
UInt16  ServerMkDir( ServerPB * inPB )
        MakeFunction ( kServerMkDir )

/* ------------------------------------------------------------
    ServerRmDir                 [Macintosh]

    Removes (deletes) an existing directory.

    Parameters:
        In:     inPB            - Ptr to a Server Parameter block
                ->fFlags        - 0 = DOS Names, 1 = LFN
                ->fVRefNum      - The volume ref num
                ->fParentID     - Directory ID
                ->fName         - the name of the dir to remove (Mac format)
        Out:    inPB            - Info returned in the parameter block
                ->fResult       - Error code
------------------------------------------------------------ */
UInt16  ServerRmDir( ServerPB * inPB )
        MakeFunction ( kServerRmDir )

/* ------------------------------------------------------------
    ServerSetAttributes         [internal]

    Sets the attributes (read-only, system, hidden, etc.) for the
    specified file.

    Parameters:
        In:     inPB            - Ptr to a Server Parameter block
                ->fFlags        - 0 = DOS Names, 1 = LFN
                ->fVRefNum      - The volume ref num
                ->fParentID     - The parent directory ID
                ->u.fXXX.fName  - the directory name
                ->fFileAttributes - New attributes
                ->fFileTimeDate - New Time & Date or NULL if no change in time and date
        Out:    inPB            - Info returned in the parameter block
                ->fResult       - Error code
------------------------------------------------------------ */
UInt16  ServerSetAttributes( ServerPB * inPB )
        MakeFunction ( kServerSetAttributes )

/* ------------------------------------------------------------
    ServerRename                [Macintosh]

    Renames a specified file to a name built from the original
    filename and the rename template.

    Parameters:
        In:     inPB            - Ptr to a Server Parameter block
                ->fFlags        - 0 = DOS Names, 1 = LFN
                ->fVRefNum      - The volume ref num
                ->fParentID     - Source Directory ID
                ->fID           - Destination Directory ID
                ->u.fXXX.fName  - the name
                ->u.fXXX.fName2 - the new name
        Out:    inPB            - Info returned in the parameter block
                ->fResult       - Error code
------------------------------------------------------------ */
UInt16  ServerRename( ServerPB * inPB )
        MakeFunction ( kServerRename )

/* ------------------------------------------------------------
    ServerDelete                [Macintosh]

    Deletes the specified file.

    Parameters:
        In:     inPB            - Ptr to a Server Parameter block
                ->fFlags        - 0 = DOS Names, 1 = LFN
                ->fVRefNum      - The volume ref num
                ->fParentID     - The parent directory ID
                ->u.fXXX.fName  - the name
        Out:    inPB            - Info returned in the parameter block
                ->fResult       - Error code
------------------------------------------------------------ */
UInt16  ServerDelete( ServerPB * inPB )
        MakeFunction ( kServerDelete )

/* ------------------------------------------------------------
    ServerGetInfo               [Macintosh]

    Returns information about the specified directory or file.

    Parameters:
        In:     inPB            - Ptr to a Server Parameter block
                ->fFlags        - 0 = DOS Names, 1 = LFN
                ->fVRefNum      - The volume ref num
                ->fParentID     - ParentDirectory ID
                ->fIndex        - Which entry in this DIR to get info on (0-n)
                ->u.fXXX.fName  - the file name (ignored is fIndex != -1)
        Out:    inPB            - Info returned in the parameter block
                ->fResult       - Error code
                ->u.fXXX.fName  - the file name
                ->fFileTimeDate - File's time & date
                ->fSize         - File's size
                ->fFileAttributes   - Directory or file's attributes
                ->fID           - Dir ID of the directory or File
------------------------------------------------------------ */
UInt16  ServerGetInfo( ServerPB * inPB )
        MakeFunction ( kServerGetInfo )

/* ------------------------------------------------------------
    ServerLock                  [Macintosh]

    Locks a range of an open file.

    Parameters:
        In:     inPB            - Ptr to a Server Parameter block
                ->fHandle       - Handle to the file
                ->fPosition     - The start of the lock range
                ->fSize         - The length of the lock range
                ->fOpenMode     - 0 = Lock , !0 = Unlock
        Out:    inPB            - Info returned in the parameter block
                ->fResult       - Error code
------------------------------------------------------------ */
UInt16  ServerLock( ServerPB * inPB )
        MakeFunction ( kServerLock )

/* ------------------------------------------------------------
    ServerGetFCBInfo                    [Macintosh]

    Returns information about an open file.

    Parameters:
        In:     inPB            - Ptr to a Server Parameter block
                ->fFlags        - 0 = DOS Names, 1 = LFN
                ->fHandle       - Handle to the file
        Out:    inPB            - Info returned in the parameter block
                ->fResult       - Error code
                ->fVRefNum      - Macintosh volume ref num
                ->fParentID     - Macintosh parent folder ID
                ->u.fXXX.fName  - the file name
                ->fID           - Macintosh file ID
                ->fPosition     - The current file position
                ->fSize         - The length of the file
                ->fOpenMode     - The open mode
                ->fFileAttributes-0 - R/W, 1 = Read Only
------------------------------------------------------------ */
UInt16  ServerGetFCBInfo( ServerPB * inPB )
        MakeFunction ( kServerGetFCBInfo )

/* ------------------------------------------------------------
    ServerGetMaxIOSize             [Macintosh]

    Get the maximum I/O size if supported, 0 if not.

    Parameters:
        In:     none
        Out:    none
------------------------------------------------------------ */
void  ServerGetMaxIOSize()
{
    unsigned __int32 Result;

    WaitForSingleObject(g_FolderShareMutex, INFINITE);
    v_FolderShareDevice->ServerPB = (unsigned __int32)gpServerPB;
    v_FolderShareDevice->Code = kServerGetMaxIOSize; // this executes the function
    Result = v_FolderShareDevice->Result;
    ReleaseMutex(g_FolderShareMutex);

    kMaxReadWriteSize = Result;
}  // ServerGetMaxIOSize
