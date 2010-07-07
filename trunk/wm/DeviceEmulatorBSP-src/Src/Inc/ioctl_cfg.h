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
//------------------------------------------------------------------------------
//
//  File:  ioctl_cfg.h
//
//  Configuration file for the IOCTL component.
//
#ifndef __IOCTL_CFG_H
#define __IOCTL_CFG_H

#if __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
//  RESTRICTION
//
//  This file is a configuration file for the IOCTL component.
//
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//
//  Define:  IOCTL_PLATFORM_TYPE/OEM
//
//  Defines the platform type and OEM string.
//
#define IOCTL_PLATFORM_TYPE                 (L"DeviceEmulator")
#define IOCTL_PLATFORM_OEM                  (L"Microsoft DeviceEmulator")

//------------------------------------------------------------------------------
//  Define:  IOCTL_PROCESSOR_VENDOR/NAME/CORE
//
//  Defines the processor information
//

#define IOCTL_PROCESSOR_VENDOR              (L"Microsoft")
#define IOCTL_PROCESSOR_NAME                (L"DeviceEmulator")
#define IOCTL_PROCESSOR_CORE                (L"ARM920T")

//------------------------------------------------------------------------------
//
//  Define:  IOCTL_PROCESSOR_INSTRUCTION_SET
//
//  Defines the processor instruction set information
//
#define IOCTL_PROCESSOR_INSTRUCTION_SET     (0)
#define IOCTL_PROCESSOR_CLOCK_SPEED	    S3C2410X_PCLK

//------------------------------------------------------------------------------

#if __cplusplus
}
#endif

#endif
