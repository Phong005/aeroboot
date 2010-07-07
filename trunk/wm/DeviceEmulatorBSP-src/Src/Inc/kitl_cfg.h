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
#ifndef __KITL_CFG_H
#define __KITL_CFG_H

//------------------------------------------------------------------------------

OAL_KITL_ETH_DRIVER g_kitlEthCS8900A = OAL_ETHDRV_CS8900A;

OAL_KITL_DEVICE g_kitlDevices[] = {
    { 
        L"CS8900A", Internal, BSP_BASE_REG_PA_CS8900A_IOBASE, 0, OAL_KITL_TYPE_ETH, 
        &g_kitlEthCS8900A
    }, {
        NULL, 0, 0, 0, 0, NULL
    }
};    

//------------------------------------------------------------------------------

#endif
