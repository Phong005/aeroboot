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
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.
--*/

#ifndef __S3C2410_LCD_H__
#define __S3C2410_LCD_H__


#ifdef ROTATE
class S3C2410DISP : public GPERotate
#else
class S3C2410DISP : public GPE
#endif //ROTATION
{
private:
	GPEMode			m_ModeInfo;
	DWORD			m_cbScanLineLength;
	DWORD			m_colorDepth;
	DWORD			m_VirtualFrameBuffer;
	DWORD			m_FrameBufferSize;
	BOOL			m_CursorDisabled;
	BOOL			m_CursorVisible;
	BOOL			m_CursorForcedOff;
	RECTL			m_CursorRect;
	POINTL			m_CursorSize;
	POINTL			m_CursorHotspot;
	UCHAR			*m_CursorBackingStore;
	UCHAR			*m_CursorXorShape;
	UCHAR			*m_CursorAndShape;

	volatile	struct lcdregs	*m_LCDRegs;
	volatile	struct gpioreg	*m_GPIORegs;

public:
					S3C2410DISP(void);
	virtual INT		NumModes(void);
	virtual SCODE	SetMode(INT modeId,	HPALETTE *palette);
	virtual INT		InVBlank(void);
	virtual SCODE	SetPalette(const PALETTEENTRY *source, USHORT firstEntry,
								USHORT numEntries);
	virtual SCODE	GetModeInfo(GPEMode *pMode,	INT modeNumber);
	virtual SCODE	SetPointerShape(GPESurf *mask, GPESurf *colorSurface,
									INT xHot, INT yHot, INT cX, INT cY);
	virtual SCODE	MovePointer(INT xPosition, INT yPosition);
	virtual void	WaitForNotBusy(void);
	virtual INT		IsBusy(void);
	virtual void	GetPhysicalVideoMemory(unsigned long *physicalMemoryBase, unsigned long *videoMemorySize);
	virtual SCODE	AllocSurface(GPESurf **surface, INT width, INT height,
									EGPEFormat format, INT surfaceFlags);
	virtual SCODE	Line(GPELineParms *lineParameters, EGPEPhase phase);
	virtual SCODE	BltPrepare(GPEBltParms *blitParameters);
	virtual SCODE	BltComplete(GPEBltParms *blitParameters);
	virtual ULONG	GetGraphicsCaps();
#if defined(CLEARTYPE) || defined(ROTATE)
	virtual ULONG   DrvEscape(
                        SURFOBJ *pso,
                        ULONG    iEsc,
                        ULONG    cjIn,
                        PVOID    pvIn,
                        ULONG    cjOut,
                        PVOID    pvOut);
#endif 
	SCODE			WrappedEmulatedLine (GPELineParms *lineParameters);
	void			CursorOn (void);
	void			CursorOff (void);

	void			InitializeHardware (void);
#ifdef ROTATE
	void SetRotateParms();
	LONG DynRotate(int angle);
	int GetRotateModeFromReg();
#endif //ROTATION
};

#endif // __S3C2410_LCD_H__.

