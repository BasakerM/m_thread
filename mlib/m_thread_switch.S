	IMPORT _m_interrupt_from_thread
	IMPORT _m_interrupt_to_thread
	IMPORT _m_thread_switch_interrupt_flag

SCB_VTOR        EQU     0xE000ED08               ; Vector Table Offset Register
NVIC_INT_CTRL   EQU     0xE000ED04               ; interrupt control state register
NVIC_SYSPRI2    EQU     0xE000ED20               ; system priority register (2)
NVIC_PENDSV_PRI EQU     0x00FF0000               ; PendSV priority value (lowest)
NVIC_PENDSVSET  EQU     0x10000000               ; value to trigger PendSV exception

	AREA |.text|, CODE, READONLY, ALIGN=2
	THUMB
	REQUIRE8
	PRESERVE8

m_interrupt_disable PROC
		EXPORT m_interrupt_disable
		MRS r0, PRIMASK
		CPSID I
		BX LR
		ENDP

m_interrupt_enable PROC
		EXPORT m_interrupt_enable
		MSR PRIMASK, r0
		BX LR
		ENDP

;实现线程切换的中断
PendSV_Handler   PROC
    EXPORT PendSV_Handler

    ;失能中断，为了保护上下文切换不被中断
    MRS     r2, PRIMASK
    CPSID   I

    ;获取中断标志位，看看是否为 0
    LDR     r0, =_m_thread_switch_interrupt_flag
    LDR     r1, [r0]
	;判断是否为 0,为 0 则跳转到 pendsv_exit
    CBZ     r1, pendsv_exit
    ;不为 0 则清 0
    MOV     r1, #0x00
    STR     r1, [r0]
	
	;判断上一个线程的值是否为 0,即是否存在
    LDR     r0, =_m_interrupt_from_thread
    LDR     r1, [r0]
	;为 0 则不存在,跳转到 switch_to_thread
	;第一次线程切换时上一个线程肯定不存在
    CBZ     r1, switch_to_thread    ; skip register save at the first time
	
	;/* 上文保存 */
	;当进入 PendSVC Handler 时，上一个线程运行的环境即：
	;xPSR， PC（线程入口地址）， R14， R12， R3， R2， R1， R0（线程的形参）
	;这些 CPU 寄存器的值会自动保存到线程的栈中，剩下的 r4~r11 需要手动保存
	;获取线程栈指针到 r1
    MRS     r1, psp
	;将 CPU 寄存器 r4~r11 的值存储到 r1 指向的地址(每操作一次地址将递减一次)
    STMFD   r1!, {r4 - r11}
	;加载 r0 指向值到 r0，即 r0=_m_interrupt_from_thread
    LDR     r0, [r0]
	;将 r1 的值存储到 r0，即更新线程栈 sp
    STR     r1, [r0]

;/* 下文切换 */
switch_to_thread
	;线程控制块有一个成员数据是 线程栈的指针 " sp "
	;_m_interrupt_to_thread 里面存的是 " SP 的地址"
	;加载  " SP 的地址"  到 r1
    LDR     r1, =_m_interrupt_to_thread
	;加载  " SP 的地址"  的值到 r1， 即 " SP "
    LDR     r1, [r1]
	;加载  " SP "  的值到 r1，即 " SP " 所在线程控制块的线程栈的栈顶地址
    LDR     r1, [r1]
	
	;将线程栈指针 r1(操作之前先递减)指向的内容加载到 CPU 寄存器 r4~r11
    LDMFD   r1!, {r4 - r11}
	;将线程栈指针更新到 PSP
    MSR     psp, r1

pendsv_exit
    ; 恢复中断
    MSR     PRIMASK, r2
	;确保异常返回使用的栈指针是 PSP,即 LR 寄存器的位 2 要为 1
    ORR     lr, lr, #0x04
	; 异常返回，这个时候栈中的剩下内容将会自动加载到 CPU 寄存器：
	; xPSR,PC(线程入口地址), R14， R12， R3， R2， R1， R0（线程的形参）
	; 同时 PSP 的值也将更新,即指向线程栈的栈顶
	;BX 直接跳转到由寄存器给定的地址
    BX      lr
    ENDP

;用于第一次执行线程切换
_m_thread_switch_to PROC
	EXPORT _m_thread_switch_to
	
	;r0 是函数的第一个参数,即将要切换的线程栈
	;将要切换的线程栈放到 _m_interrupt_to_thread 中
	LDR r1, =_m_interrupt_to_thread
	STR     r0, [r1]
	
	;因为是第一次执行切换,所以没有上一个线程,所以置0
	LDR r1, =_m_interrupt_from_thread
	MOV r0, #0x0
	STR r0, [r1]
	
	;将切换标志置位
	LDR     r1, =_m_thread_switch_interrupt_flag
	MOV     r0, #1
	STR     r0, [r1]

	;设置 PendSV 异常的优先级
	LDR     r0, =NVIC_SYSPRI2
	LDR     r1, =NVIC_PENDSV_PRI
	LDR.W   r2, [r0,#0x00]
	ORR     r1,r1,r2
	STR     r1, [r0]

	;触发 PendSV 异常 (产生上下文切换)
	LDR     r0, =NVIC_INT_CTRL
	LDR     r1, =NVIC_PENDSVSET
	STR     r1, [r0]

;    ; restore MSP
;    LDR     r0, =SCB_VTOR
;    LDR     r0, [r0]
;    LDR     r0, [r0]
;    MSR     msp, r0

	;开中断
	CPSIE   F
	CPSIE   I

	ENDP
		
_m_thread_switch    PROC
    EXPORT _m_thread_switch

	;判断中断标志位是否为 1
    LDR     r2, =_m_thread_switch_interrupt_flag
    LDR     r3, [r2]
    CMP     r3, #1
	;为 1 则跳到 _reswitch
    BEQ     _reswitch
    ;设置中断标志位为 1
    MOV     r3, #1
    STR     r3, [r2]
	
	;加载 _m_interrupt_from_thread 的地址到 r2
    LDR     r2, =_m_interrupt_from_thread
	;存储 r0 的值到 _m_interrupt_from_thread,即和上一个线程有关
    STR     r0, [r2]

_reswitch
	;加载 _m_interrupt_to_thread 的地址到 r2
    LDR     r2, =_m_interrupt_to_thread
	;存储 r1 的值到 _m_interrupt_to_thread,即和下一个线程有关
    STR     r1, [r2]

	;触发 PendSV 异常,实现上下文切换
    LDR     r0, =NVIC_INT_CTRL              ; trigger the PendSV exception (causes context switch)
    LDR     r1, =NVIC_PENDSVSET
    STR     r1, [r0]
    BX      LR
    ENDP

	
	ALIGN 4
		
	END