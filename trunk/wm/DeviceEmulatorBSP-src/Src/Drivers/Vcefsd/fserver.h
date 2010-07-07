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
    File:       FServer.h

    Contains:   Equates for Mac File Server interface
                This file contains the equates that are shared
                between the PC Client and the Mac File Server.

    Written by: Scott Coleman

    Copyright:  © 1996 Connectix Corporation

    Change History (most recent first):

*/

#ifndef _FSERVER_H
#define _FSERVER_H

// ------------------------------------------------------------
//                      Basic Types
// ------------------------------------------------------------

#define IS_32                               ; Art Wong 991218
#if defined(MSDOS) || defined(IS_32)
typedef unsigned long   UInt32;
typedef signed long     SInt32;
typedef unsigned short  UInt16;
typedef signed short    SInt16;
typedef unsigned char   UInt8;
typedef signed char     SInt8;
typedef unsigned char   Boolean;
typedef unsigned __int64 UnsignedWide;

typedef UInt16          UniChar;
typedef UInt32          UniCharCount;
#endif

#ifndef MYFAR
#ifdef  MSDOS
#define MYHUGE  _huge
#define MYFAR   _far
#else
#define MYHUGE
#define MYFAR
#endif
#endif

#ifdef  MSDOS
#pragma pack(4)
#endif

#ifdef  IS_32
#pragma pack(4)
#endif

// ------------------------------------------------------------
//                          Equates
// ------------------------------------------------------------

#define kFServerVersion         7       // current version server API version #
#define kMinCompatibleVersion   7       // Minimum version compatible with this version of the API
#define kMaxCompatibleVersion   7       // Maximum version compatible with this version of the API

#define kFServerModuleID    'FSRV'      // Our Module identifier

#define kISAExt1            0x0F        // Escape instruction
#define kISAExt2            0x3F        // Escape instruction part 2
#define kServerExt          0x02        // Escape instruction for Server
#define kServerPollCompletion 0x00      //
#define kServerInitialize   0x01        //
#define kServerCheckConfig  0x02        //
#define kServerGetConfig    0x03        //
#define kServerGetDriveConfig 0x04      //
#define kServerCreate       0x05        //
#define kServerOpen         0x06        //
#define kServerRead         0x07        //
#define kServerWrite        0x08        //
#define kServerSetEOF       0x09        //
#define kServerClose        0x0A        //
#define kServerGetSpace     0x0B        //
#define kServerMkDir        0x0C        //
#define kServerRmDir        0x0D        //
#define kServerSetAttributes 0x0E       //
#define kServerRename       0x0F        //
#define kServerDelete       0x10        //
#define kServerGetInfo      0x11        //
#define kServerLock         0x12        //
#define kServerGetFCBInfo   0x13        //
#define kServerUseNotify    0x14        // Art Wong: 000708
#define kServerGetMaxIOSize 0x15        // Art Wong: 000708
#define kMaxServerFunction  0x15        // Max Valid function number

#define kServer32           0x80        // Set this bit for 32 bit access
#define kServerMinorOpMask  0x7F        // Use this mask to get the minor opcode

#define kErrorInvalidFunction 0x01      //

#define kDefMaxReadWriteSize   8192     // Default largest single read or write

#define kMaxLFN     255                 // # of characters in our long file names

#define kInvalidSrvHandle   0xffff      // 0 is a valid handle
// ------------------------------------------------------------
//                      Data Structures
// ------------------------------------------------------------

//  Server File I/O Parameter Block

typedef  struct ServerPB    {
    UInt16      fStructureSize;         // sizeof(ServerPB)
    UInt16      fResult;                // Request Result Code (error)
    UInt32      fFindTransactionID;     // *new* see find.c
    SInt16      fIndex;                 // Which directory entry
    UInt16      fHandle;                // Open file's handle (FServer Handle)

    UInt32      fFileTimeDate;          // file's time & date
    UInt32      fSize;                  // File's Size
    UInt32      fPosition;              // File's Position
    UInt8 MYFAR *fDTAPtr;               // Disk Transfer Address
    UInt16      fFileAttributes;        // File's Attributes
    UInt16      fOpenMode;              // Open Mode
    Boolean     fWildCard;              // TRUE if Dos Name contains a wildcard

    union {
        // Note:  emulator fserver.h contains fDos and fBoth branches to the union, but they're
        //        Unused.  Preserve the union to reduce unneeded code differences between the
        //        emulator and DeviceEmulator versions of this codebase.
        struct {
            UInt16      fNameLength;        // Unicode long file name length in bytes (excludes NULL)
            UniChar     fName[kMaxLFN+1];   // Unicode long file name (NULL terminated)
            UInt16      fName2Length;       // Unicode long file name length in bytes (excludes NULL)
            UniChar     fName2[kMaxLFN+1];  // Unicode long file name (NULL terminated)
        } fLfn;

    } u;

    UInt32      fFileCreateTimeDate;

} ServerPB, * PServerPB ;

#define TerminateUniLFN(a) {(a)->u.fLfn.fName[(a)->u.fLfn.fNameLength/sizeof(UniChar)] = 0;}

// ------------------------------------------------------------
//                      Function Prototypes
// ------------------------------------------------------------


#ifdef  MSDOS
UInt16  ServerInitialize( UInt16 inCodePage );
UInt16  ServerCheckConfig( void );
UInt32  ServerGetConfig( UInt32 inUsedDriveMask );
UInt16  ServerGetDriveConfig( ServerPB * inPB );
UInt16  ServerCreate( ServerPB * inPB );
UInt16  ServerOpen( ServerPB * inPB );
UInt16  ServerRead( ServerPB * inPB );
UInt16  ServerWrite( ServerPB * inPB );
UInt16  ServerSetEOF( ServerPB * inPB );
UInt16  ServerClose( ServerPB * inPB );
UInt16  ServerGetSpace( ServerPB * inPB );
UInt16  ServerMkDir( ServerPB * inPB );
UInt16  ServerRmDir( ServerPB * inPB );
UInt16  ServerGetAttributes( ServerPB * inPB );
UInt16  ServerSetAttributes( ServerPB * inPB );
UInt16  ServerRename( ServerPB * inPB );
UInt16  ServerDelete( ServerPB * inPB );
UInt16  ServerGetInfo( ServerPB * inPB );
UInt16  ServerGetIndexedInfo( ServerPB * inPB );
UInt16  ServerLock( ServerPB * inPB );
UInt16  ServerGetFCBInfo( ServerPB * inPB );
void    ServerGetMaxIOSize();
extern  UInt32 kMaxReadWriteSize;
#endif

#ifdef  IS_32

#if defined __cplusplus
extern "C"  {
#endif

UInt16  ServerInitialize( UInt16 inCodePage );
UInt16  ServerCheckConfig( void );
UInt32  ServerGetConfig( UInt32 inUsedDriveMask );
UInt16  ServerGetDriveConfig( ServerPB * inPB );
UInt16  ServerCreate( ServerPB * inPB );
UInt16  ServerOpen( ServerPB * inPB );
UInt16  ServerRead( ServerPB * inPB );
UInt16  ServerWrite( ServerPB * inPB );
UInt16  ServerSetEOF( ServerPB * inPB );
UInt16  ServerClose( ServerPB * inPB );
UInt16  ServerGetSpace( ServerPB * inPB );
UInt16  ServerMkDir( ServerPB * inPB );
UInt16  ServerRmDir( ServerPB * inPB );
UInt16  ServerGetAttributes( ServerPB * inPB );
UInt16  ServerSetAttributes( ServerPB * inPB );
UInt16  ServerRename( ServerPB * inPB );
UInt16  ServerDelete( ServerPB * inPB );
UInt16  ServerGetInfo( ServerPB * inPB );
UInt16  ServerGetIndexedInfo( ServerPB * inPB );
UInt16  ServerLock( ServerPB * inPB );
UInt16  ServerGetFCBInfo( ServerPB * inPB );
void    ServerGetMaxIOSize();
extern  UInt32 kMaxReadWriteSize;
#endif

#if defined __cplusplus
}
#endif

#ifdef  MSDOS
#pragma pack( )     // default
#endif

#ifdef  IS_32
#pragma pack( )     // default
#endif

#endif // _FSERVER_H
