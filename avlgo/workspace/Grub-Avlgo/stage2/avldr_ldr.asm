; ******************************************************************************
; Indigo - Indigo is another branch of Grub, served for Aero
;     Powered (r) 2002,2003,2004,2005 2006 AvaitoR Tang
;
;
; Indigo Start - asm Code by AvaitoR(jsf_f22@msn.com) 
; Description:
;    
;    This Stub is designed to load the c stub of ISOEmu, and trans control to the
;    Loader program of this Emulated CD.
;
;    -And- procedures that are most easily written in assembly. Procedures are
;    called using the 'C' calling convention. AX, CX, DX, are all clobbered.
; ******************************************************************************

; 2006-1-7 11:32 ( ver 1.10 )
; 
;    1. for being loaded by Avldrng.exe
;    2. for avldrng.exe is a bootsect(0x0:0x7c00) and ntldr (0x2000:0x0) Loader
;         so we may trans ourself from 0x2000:0x0000 -> 0x0000:0x0600, that's OK.
;

[bits 16]
[ORG 0x0000]

[global _start]
_start:		; first-stage bootloader/other bootloader transfers control to us here; 0000:8200
	jmp  real_start
	nop
	
;; =========	
NTFS_TAG          db  'NTFS', 0x0
BOOTVAL						db	'Indigo2006, Grub for Avldrng', 0ah, 0dh
ISOEMU_LOGOZ			db	0dh,0ah,'(r)Powered by Gandalf, (c)2002-2006 ',0

			
BOOT_DRV 					dw   	0

; -------------------------------------------------------------
real_start:		
  push  cs
  pop   ax
  
  cmp   ax, 0x2000
  jz    Ready_GO
  
  ; wrong loader do with me, hang!
  mov   ah, 0eh
  mov   al, 'x'
  int   0x10
  jmp   $
  
Ready_GO:  
  ; 
	cli
	mov	ax, 9000h
	mov	ss, ax
	xor	sp, sp		; ss:sp ->7000:0
	sti

  ; ----------------------------- 1st for move ourself to 0x80:0x0
  	mov	  di, 80h
  	mov	  es, di		
  	xor	  di, di		; es:di -> 80h:0
  
  	mov	  si, 2000h
  	mov	  ds, si
    xor	  si, si    ; ds:si -> 0x2000:0x0000
    
  	mov	  cx, 0x200	;   48 KB, that's enough
  
  	cld
  	rep	  movsb		; rep when cx >0 mov [si] to es:[di]
  	
    push	word 0
    push	real_move_stage2 + 0x800	    ; Indigo real_move_stage2
    retf

  ; ---------------------------- 	  	  
real_move_stage2:
  	
  	mov	  di, 800h
  	mov	  es, di		
  
  	mov	  si, 2000h
  	mov	  ds, si

    mov   cx, 5         ; 300 KB
    
re_move:    
    push  cx
  	xor	  di, di		; es:di -> 800h:0
    xor	  si, si    ; ds:si -> 0x2000:0x0000
      	
  	mov	  cx, 0x8000	;   48 KB, that's enough
  
  	cld
  	rep	  movsw		; rep when cx >0 mov [si] to es:[di]
    
    mov   ax, es
    add   ax, 0x1000
    mov   es, ax
    
    mov   ax, ds
    add   ax, 0x1000
    mov   ds, ax
        
  	pop   cx
  	loop  re_move
  	
  	
  	sti

  ; Launch out program
	push	word 0
	push	stage2_bin + 0x8000	    ; Indigo stage2
	
	retf		


times 192-($-$$) db 0
incbin "menu.lst"

; ---------------------------------------------------------------------------------
times 510-($-$$) db 0
db 0x55
db 0xAA

stage2_bin: