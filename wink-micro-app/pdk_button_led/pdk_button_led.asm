;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.6.0 #16555 (MINGW64)
;--------------------------------------------------------
	.module pdk_button_led
	
	.optsdcc -mpdk14

; default segment ordering in RAM for linker
	.area DATA
	.area OSEG (OVR,DATA)

;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _main
	.globl __misclvr
	.globl __misc2
	.globl __misc
	.globl __pwmg2cubl
	.globl __pwmg2cubh
	.globl __pwmg2dtl
	.globl __pwmg2dth
	.globl __pwmg2s
	.globl __pwmg2c
	.globl __pwmg1cubl
	.globl __pwmg1cubh
	.globl __pwmg1dtl
	.globl __pwmg1dth
	.globl __pwmg1s
	.globl __pwmg1c
	.globl __pwmg0cubl
	.globl __pwmg0cubh
	.globl __pwmg0dtl
	.globl __pwmg0dth
	.globl __pwmg0s
	.globl __pwmg0c
	.globl __rfccrl
	.globl __rfccrh
	.globl __rfcc
	.globl __gpcs
	.globl __gpcc
	.globl __bgtr
	.globl __tm3b
	.globl __tm3s
	.globl __tm3ct
	.globl __tm3c
	.globl __tm2b
	.globl __tm2s
	.globl __tm2ct
	.globl __tm2c
	.globl __t16c
	.globl __t16m
	.globl __pbph
	.globl __pbc
	.globl __pb
	.globl __pbdier
	.globl __paph
	.globl __pac
	.globl __pa
	.globl __padier
	.globl __integs
	.globl __intrq
	.globl __inten
	.globl __eoscr
	.globl __ilrcr
	.globl __ihrcr
	.globl __clkmd
	.globl __sp
	.globl __flag
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
	.area RSEG (ABS)
	.org 0x0000
__flag	=	0x0000
__sp	=	0x0002
__clkmd	=	0x0003
__ihrcr	=	0x000b
__ilrcr	=	0x0039
__eoscr	=	0x000a
__inten	=	0x0004
__intrq	=	0x0005
__integs	=	0x000c
__padier	=	0x000d
__pa	=	0x0010
__pac	=	0x0011
__paph	=	0x0012
__pbdier	=	0x000e
__pb	=	0x0014
__pbc	=	0x0015
__pbph	=	0x0016
__t16m	=	0x0006
__t16c::
	.ds 2
__tm2c	=	0x001c
__tm2ct	=	0x001d
__tm2s	=	0x0017
__tm2b	=	0x0009
__tm3c	=	0x0032
__tm3ct	=	0x0033
__tm3s	=	0x0034
__tm3b	=	0x0035
__bgtr	=	0x001a
__gpcc	=	0x0018
__gpcs	=	0x0019
__rfcc	=	0x0036
__rfccrh	=	0x0037
__rfccrl	=	0x0038
__pwmg0c	=	0x0020
__pwmg0s	=	0x0021
__pwmg0dth	=	0x0022
__pwmg0dtl	=	0x0023
__pwmg0cubh	=	0x0024
__pwmg0cubl	=	0x0025
__pwmg1c	=	0x0026
__pwmg1s	=	0x0027
__pwmg1dth	=	0x0028
__pwmg1dtl	=	0x0029
__pwmg1cubh	=	0x002a
__pwmg1cubl	=	0x002b
__pwmg2c	=	0x002c
__pwmg2s	=	0x002d
__pwmg2dth	=	0x002e
__pwmg2dtl	=	0x002f
__pwmg2cubh	=	0x0030
__pwmg2cubl	=	0x0031
__misc	=	0x0008
__misc2	=	0x000f
__misclvr	=	0x001b
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area DATA
;--------------------------------------------------------
; overlayable items in ram
;--------------------------------------------------------
;--------------------------------------------------------
; Stack segment in internal ram
;--------------------------------------------------------
	.area SSEG
__start__stack:
	.ds	1

;--------------------------------------------------------
; absolute external ram data
;--------------------------------------------------------
	.area DABS (ABS)
;--------------------------------------------------------
; interrupt vector
;--------------------------------------------------------
	.area HOME
__interrupt_vect:
	.area	HEADER (ABS)
	.org	 0x0020
	reti
;--------------------------------------------------------
; global & static initialisations
;--------------------------------------------------------
	.area HOME
	.area GSINIT
	.area GSFINAL
	.area GSINIT
	.area	PREG (ABS)
	.org 0x00
p::
	.ds 2
	.area	HEADER (ABS)
	.org 0x0000
	nop
	clear	p+1
	mov	a, #s_OSEG
	add	a, #l_OSEG + 1
	and	a, #0xfe
	mov.io	sp, a
	call	___sdcc_external_startup
	cneqsn	a, #0x00
	goto	__sdcc_init_data
	goto	__sdcc_program_startup
	.area GSINIT
__sdcc_init_data:
	mov	a, #s_DATA
	mov	p, a
	goto	00002$
00001$:
	mov	a, #0x00
	idxm	p, a
	inc	p
	mov	a, #s_DATA
00002$:
	add	a, #l_DATA
	ceqsn	a, p
	goto	00001$
	.area GSFINAL
	goto	__sdcc_program_startup
;--------------------------------------------------------
; Home
;--------------------------------------------------------
	.area HOME
	.area HOME
__sdcc_program_startup:
	goto	_main
;	return from main will return to caller
;--------------------------------------------------------
; code
;--------------------------------------------------------
	.area CODE
;	D:\MyWorkSpace_program\lowcode-nocode\ai-app\wink-ai-embedded\wink-micro-app\pdk_button_led\pdk_button_led.c: 23: void main(void) {
;	-----------------------------------------
;	 function main
;	-----------------------------------------
_main:
;	D:\MyWorkSpace_program\lowcode-nocode\ai-app\wink-ai-embedded\wink-micro-app\pdk_button_led\pdk_button_led.c: 25: PADIER |= (1 << BTN_BIT);       // 使能 PA.5 为数字输入模式
	set1.io	__padier, #5
;	D:\MyWorkSpace_program\lowcode-nocode\ai-app\wink-ai-embedded\wink-micro-app\pdk_button_led\pdk_button_led.c: 26: PAPH   |= (1 << BTN_BIT);       // 使能 PA.5 内部硬件上拉电阻
	set1.io	__paph, #5
;	D:\MyWorkSpace_program\lowcode-nocode\ai-app\wink-ai-embedded\wink-micro-app\pdk_button_led\pdk_button_led.c: 27: PAC    &= ~(1 << BTN_BIT);      // 设为输入方向
	set0.io	__pac, #5
;	D:\MyWorkSpace_program\lowcode-nocode\ai-app\wink-ai-embedded\wink-micro-app\pdk_button_led\pdk_button_led.c: 30: PAC    |= (1 << LED_BIT);       // 设为输出方向
	set1.io	__pac, #4
;	D:\MyWorkSpace_program\lowcode-nocode\ai-app\wink-ai-embedded\wink-micro-app\pdk_button_led\pdk_button_led.c: 31: set_led_off();                  // 初始状态：熄灭
	set0.io	__pa, #4
;	D:\MyWorkSpace_program\lowcode-nocode\ai-app\wink-ai-embedded\wink-micro-app\pdk_button_led\pdk_button_led.c: 34: while (1) {
00105$:
;	D:\MyWorkSpace_program\lowcode-nocode\ai-app\wink-ai-embedded\wink-micro-app\pdk_button_led\pdk_button_led.c: 35: if (is_button_pressed()) {
	t0sn.io	__pa, #5
	goto	00102$
00123$:
;	D:\MyWorkSpace_program\lowcode-nocode\ai-app\wink-ai-embedded\wink-micro-app\pdk_button_led\pdk_button_led.c: 36: set_led_on();           // 按下时点亮
	set1.io	__pa, #4
	goto	00103$
00102$:
;	D:\MyWorkSpace_program\lowcode-nocode\ai-app\wink-ai-embedded\wink-micro-app\pdk_button_led\pdk_button_led.c: 38: set_led_off();          // 松开时熄灭
	set0.io	__pa, #4
00103$:
;	D:\MyWorkSpace_program\lowcode-nocode\ai-app\wink-ai-embedded\wink-micro-app\pdk_button_led\pdk_button_led.c: 40: __asm__("nop");             // 协作式微步推进
	nop
	goto	00105$
;	D:\MyWorkSpace_program\lowcode-nocode\ai-app\wink-ai-embedded\wink-micro-app\pdk_button_led\pdk_button_led.c: 42: }
	ret
	.area CODE
	.area CONST
	.area CABS (ABS)
