#include "m_thread.h"
#include "stm32f4xx.h"

/**** 宏定义 ****/

#define _m_thread_statck_size 512 /* 单个线程的栈空间大小 */

#define _m_null (0)

/**** 功能宏定义 ****/

#define ALIGN(n) __attribute__((aligned(n)))
/* 向下对齐 */
#define _m_align_down(size, align) ((size) & ~((align) - 1))
/* 已知一个结构体里面的成员的地址，反推出该结构体的首地址 */
#define _m_container_of(ptr, type, member) \
((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))

#define _m_list_entry(node, type, member) \
_m_container_of(node, type, member)

/**************** 类型重定义 ****************/

typedef unsigned long _m_uint32_t;
typedef unsigned short _m_uint16_t;
typedef unsigned char _m_uint8_t;
typedef unsigned char _m_statck_t; /* 栈类型 */

/**************** 数据结构定义 ****************/

/* 链表结构体 */
typedef struct _m_struct_list_t
{
    struct _m_struct_list_t *next; /* 列表的下一个节点 */
    struct _m_struct_list_t *prev; /* 列表的上一个节点 */
}_m_list_t;

/* 线程控制块结构体 */
typedef struct _m_struct_thread_t
{
	void *sp; /* 线程栈指针 */
	
	_m_uint8_t priority; /* 线程优先级 */
	_m_uint32_t timeout; /* 超时时间 */
	_m_uint8_t state; /* 就绪状态 */
	
	_m_list_t timer_list; /* 定时器 */
}_m_thread_t;

struct _exception_stack_frame
{
	/* 异常发生时，自动加载到 CPU 寄存器的内容 */
	_m_uint32_t r0;
	_m_uint32_t r1;
	_m_uint32_t r2;
	_m_uint32_t r3;
	_m_uint32_t r12;
	_m_uint32_t lr;
	_m_uint32_t pc;
	_m_uint32_t psr;
};
	
struct _stack_frame
{
	/* 异常发生时，需手动加载到 CPU 寄存器的内容 */
	_m_uint32_t r4;
	_m_uint32_t r5;
	_m_uint32_t r6;
	_m_uint32_t r7;
	_m_uint32_t r8;
	_m_uint32_t r9;
	_m_uint32_t r10;
	_m_uint32_t r11;

	struct _exception_stack_frame _exception_stack_frame;
};

/**************** 变量定义 ****************/

/* 异常和中断处理表 */
_m_uint32_t _m_interrupt_from_thread; /* 用于存储上一个线程的栈的 sp 的指针 */
_m_uint32_t _m_interrupt_to_thread; /* 用于存储下一个将要运行的线程的栈的 sp 的指针 */
_m_uint32_t _m_thread_switch_interrupt_flag; /* PendSV 中断服务函数执行标志 */

ALIGN(4)
static _m_statck_t _m_thread_statck[m_thread_max+1][_m_thread_statck_size]; /* 线程栈 */
static _m_thread_t _m_thread[m_thread_max+1]; /* 线程控制块 */
static _m_list_t _m_timer_list_head; /* 定时器链表头 */
static _m_thread_t *_m_current_thread; /* 当前线程控制块指针 */
static _m_uint32_t _m_tick; /* 时基计数 */
static _m_uint32_t _m_priority_ready_group; /* 优先级就绪组 */

/* _m_lowest_1_bitmap[] 数组的解析
* 将一个 8 位整形数的取值范围 0~255 作为数组的索引，
* 索引值第一个出现 1(从最低位开始)的位号作为该数组索引下的成员值。
* 举例：十进制数 10 的二进制为： 0000 1010,从最低位开始，
* 第一个出现 1 的位号为 bit1，_m_lowest_1_bitmap[10]=1
* 注意：只需要找到第一个出现 1 的位号即可
*/
const _m_uint8_t _m_lowest_1_bitmap[] =
{/* 位号 */
/* 00 */ 0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
/* 10 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
/* 20 */ 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
/* 30 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
/* 40 */ 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
/* 50 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
/* 60 */ 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
/* 70 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
/* 80 */ 7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
/* 90 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
/* A0 */ 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
/* B0 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
/* C0 */ 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
/* D0 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
/* E0 */ 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
/* F0 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
};

/**************** 函数声明 ****************/

//void _m_thread_scheduler_init(void); /* 初始化线程调度 */
void _m_free_thread_entry(void);
void _m_thread_scheduler(void); /* 线程调度 */
_m_statck_t* _m_thread_statck_init(void *entry,_m_statck_t *addr); /* 初始化线程栈 */

void _m_list_init(_m_list_t *list);   /* 初始化一个列表 */
void _m_list_insert_before(_m_list_t *list,_m_list_t *ilist); /* 向后增加一个节点 */
void _m_list_insert_after(_m_list_t *list,_m_list_t *ilist); /* 向前增加一个节点 */
void _m_list_remove(_m_list_t *rlist); /* 移除一个节点 */

void _m_thread_switch_to(_m_uint32_t to); /* 首次切换线程 */
void _m_thread_switch(_m_uint32_t from, _m_uint32_t to); /* 切换线程 */

/**************** 函数定义 ****************/


	static unsigned short thread1_tick = 0,thread2_tick = 0;

/**************** 外部可调用函数 ****************/

//void m_interrupt_enter(void)
//{
//}

//void m_interrupt_leave(void)
//{
//}

/*
*
*/
uint32_t m_systick_init(uint32_t ticks)
{
  if ((ticks - 1UL) > SysTick_LOAD_RELOAD_Msk)
  {
    return (1UL);                                                   /* Reload value impossible */
  }

  SysTick->LOAD  = (uint32_t)(ticks - 1UL);                         /* set reload register */
  NVIC_SetPriority (SysTick_IRQn, (1UL << __NVIC_PRIO_BITS) - 1UL); /* set Priority for Systick Interrupt */
  SysTick->VAL   = 0UL;                                             /* Load the SysTick Counter Value */
  SysTick->CTRL  = SysTick_CTRL_CLKSOURCE_Msk |
                   SysTick_CTRL_TICKINT_Msk   |
                   SysTick_CTRL_ENABLE_Msk;                         /* Enable SysTick IRQ and SysTick Timer */
  return (0UL);                                                     /* Function successful */
}

/*
* 线程初始化函数
* 参数 :
* 	_void
* 返回 :
* 	void
*/
void m_thread_init(void)
{
	/* 初始化定时器链表头指针 */
	_m_list_init(&_m_timer_list_head);
	/* 初始化当前线程控制块指针 */
	_m_current_thread = _m_null;
	/* 初始化时基计数器 */
	_m_current_thread = 0;
	/* 初始化空闲线程 */
	m_thread_creat(0,_m_free_thread_entry);
	/* 手动指定第一个运行的线程 */
	_m_current_thread = &_m_thread[0]; /* 空闲线程 */
	/* 初始化 systick */
	m_systick_init(SystemCoreClock/1000);
}

/*
*
*/
void SysTick_Handler(void)
{
	/* 时基更新 */
	m_tick_update();
}

/*
* 线程创建函数
* 参数 :
* 	_m_uint8_t priority : 线程优先级
* 	void *entry : 线程入口地址
* 返回 :
* 	void
*/
void m_thread_creat(_m_uint8_t priority, void *entry)
{
	/* 不允许创建空闲线程和超出范围的线程 */
	if(priority > m_thread_max) { return; }
	/* 初始化线程 */
	_m_list_init(&(_m_thread[priority].timer_list));
	_m_thread[priority].priority = priority;
	_m_thread[priority].state = 1;
	/* 初始化线程栈,获取栈顶指针	*/
	_m_thread[priority].sp = (void*)_m_thread_statck_init(entry,
			(void*)((char*)(_m_thread_statck[priority]) + _m_thread_statck_size - 2));
}

/*
* 线程启动函数
* 参数 :
* 	_m_uint8_t priority : 线程优先级
* 返回 :
* 	void
*/
void m_thread_startup(_m_uint8_t priority)
{
	_m_uint8_t temp = 0;
	/* 关中断 */
	temp = m_interrupt_disable();
	/* 置位对应的优先级就绪组,因为空线程占用了优先级0但不会被添加到组中,所以优先级1处于位0,所以要-1 */
	_m_priority_ready_group |= 1 << (priority - 1);
	/* 线程调度 */
//	_m_thread_scheduler();
	/* 开中断 */
	m_interrupt_enable(temp);
}


void m_thread_scheduler_startup(void)
{
	/* 切换到第一个线程 */
	_m_thread_switch_to((_m_uint32_t)&_m_current_thread->sp);
}

/*
* 线程挂起函数
* 参数 :
* 	_m_uint32_t tick
* 返回 :
* 	void
*/
void m_thread_suspend(_m_uint32_t tick)
{
	m_interrupt_disable();
	
	if(_m_current_thread == &_m_thread[1])
	{
		thread1_tick = 10;
		_m_thread[1].timeout = 10;//_m_tick + 10;
		_m_thread[1].state = 0;
	}
	else if(_m_current_thread == &_m_thread[2])
	{
		thread2_tick = 10;
		_m_thread[2].timeout = 10;//_m_tick + 10;
		_m_thread[2].state = 0;
	}
		
	_m_thread_scheduler();
	
	m_interrupt_enable(0);
	return;
//	_m_uint8_t temp = 0;
	/* 关中断 */
	/*temp =*/ m_interrupt_disable();
	/* 计算超时时间 */
	_m_current_thread->timeout = tick + _m_tick;
	/* 排序插入优先级就绪链表 */
	_m_list_insert_before(&_m_timer_list_head,&(_m_current_thread->timer_list));
	/* 线程调度 */
	_m_thread_scheduler();
	/* 开中断 */
	m_interrupt_enable(0);
}

/*
* 线程定时器,时基更新函数
* 参数 :
* 	_void
* 返回 :
* 	void
* 说明 :
* 	需要放在systick中断中,中断时间视具体情况而定,线程挂起的时间是以中断时间为基础单位的
*/
void m_tick_update(void)
{
	_m_thread_t *thread;
	_m_list_t *index_list;
	
	////////////////////////////////////
	++_m_tick;
	if(_m_thread[1].timeout > 0)
	{
		--_m_thread[1].timeout;
	}
	if(_m_thread[2].timeout > 0)
	{
		--_m_thread[2].timeout;
	}
	if(_m_thread[1].timeout == 0)
	{
		_m_thread[1].state = 1;
	}
	if(_m_thread[2].timeout == 0)
	{
		_m_thread[2].state = 1;
	}
//	if(thread1_tick > 0)
//	{
//		--thread1_tick;
//	}
//	if(thread2_tick > 0)
//	{
//		--thread2_tick;
//	}
//	if(thread1_tick == 0)
//	{
//		_m_thread[1].state = 1;
//	}
//	if(thread2_tick == 0)
//	{
//		_m_thread[2].state = 1;
//	}
	_m_thread_scheduler();	
	
	return;
	///////////////////////////////////
	
	/* 计时器计数 */
	++_m_tick;
	
	while(1)
	{
		/* 获取下一个节点 */
		index_list = _m_timer_list_head.next;
		/* 指向头节点,说明已经遍历了链表 */
		if(index_list == &_m_timer_list_head)
		{
			break;
		}
		/* 获取线程控制块指针 */
		thread = _m_list_entry(index_list,
														_m_thread_t,
														timer_list);
		/* 按照顺序检索是否超时,定时器链表的排序规则依据超时时间和优先级。 */
		/* 首先按照超时时间做升序,当相同时,则按照优先级做降序 */
		if(thread->timeout != _m_tick)
		{
			/* 时间不相等,则说明时间未到,如果排序在前的时间都不相等,那后面的也不会相等,则直接退出循环 */
			break;
		}
		else if(thread->timeout == _m_tick)
		{
			/* 时间相等,则说明时间到了,将该线程在优先级就绪组中置位,因为空线程占用了优先级0但不会被添加到组中,所以优先级1处于位0,所以要-1 */
			_m_priority_ready_group |= 1 << (thread->priority-1);
			/* 就绪后从定时器链表中移除 */
			_m_list_remove(index_list);
			/* 检索下一个节点,因为有可能下一个节点的超时时间相同但优先级低 */
			continue;
		}
		/* 只有当检索到时间未到的线程,或者说处理完所有时间到达的线程后才会退出循环 */
		/* 因为时间相同时按照优先级排序,所有当出现该情况时,for需要继续处理排在后面优先级较低的线程,所以设定为检索到时间未到的线程时才退出循环 */
	}
	/* 线程调度 */
	_m_thread_scheduler();
}

/**************** 内部调用函数 ****************/

/* 空闲线程 */
void _m_free_thread_entry(void)
{
	while(1);
}

/*
* 线程调度函数
* 参数 :
* 	_void
* 返回 :
* 	void
*/
void _m_thread_scheduler(void)
{
	volatile _m_uint8_t index = 0;
//	_m_thread_t *to_thread = _m_null;
	_m_thread_t *from_thread = _m_null;
	
	if(_m_thread[1].state == 1)
	{
		from_thread = _m_current_thread;
		_m_current_thread = &_m_thread[1];
	}
	else if(_m_thread[2].state == 1)
	{
		from_thread = _m_current_thread;
		_m_current_thread = &_m_thread[2];
	}
	else
	{
		from_thread = _m_current_thread;
		_m_current_thread = &_m_thread[0];
	}
	
//	if(thread1_tick != 0 && thread2_tick != 0)
//	{
//		if(_m_current_thread == &_m_thread[0]) return;
//		from_thread = _m_current_thread;
//		_m_current_thread = &_m_thread[0];
//	}
//	else if(thread1_tick == 0)
//	{
//		from_thread = _m_current_thread;
//		_m_current_thread = &_m_thread[1];
//	}
//	else if(thread2_tick == 0)
//	{
//		from_thread = _m_current_thread;
//		_m_current_thread = &_m_thread[2];
//	}
	_m_thread_switch((_m_uint32_t)&from_thread->sp,(_m_uint32_t)&_m_current_thread->sp);
	
	return;
	
	/* 为0,则没有线程就绪,那就运行空闲线程 */
	if(_m_priority_ready_group == 0)
	{
		index = 0;
		/* 当前就是空白线程 */
		if(_m_current_thread->priority == index)
		{
			return;
		}
	}
	/* 有线程就绪 */
	else
	{
		if(_m_priority_ready_group <= 0xff)
		{
			index = 1 + _m_lowest_1_bitmap[_m_priority_ready_group&0xff];
		}
		else if(_m_priority_ready_group <= 0xff00)
		{
			index = 1 + 8 + _m_lowest_1_bitmap[_m_priority_ready_group>>8&0xff];
		}
		else if(_m_priority_ready_group <= 0xff0000)
		{
			index = 1 + 16 + _m_lowest_1_bitmap[_m_priority_ready_group>>16&0xff];
		}
		else /* if(_m_priority_ready_group <= 0xff000000) */
		{
			index = 1 + 24 + _m_lowest_1_bitmap[_m_priority_ready_group>>24&0xff];
		}
		/* 将优先级就绪组中的对应位清零 */
		_m_priority_ready_group &= ~(0x00000001 << (index - 1));
	}
	/* 获取对应的线程块 */
	from_thread = _m_current_thread;
//	to_thread = &_m_thread[index];
	_m_current_thread = &_m_thread[index];
//	to_thread = &_m_thread[index];
//	_m_current_thread = to_thread;
	/* 上下文切换 */
	_m_thread_switch((_m_uint32_t)&from_thread->sp,(_m_uint32_t)&_m_current_thread->sp);
}

/*
* 线程栈初始化函数
* 参数 :
* 	void *entry : 线程入口地址
* 	_m_statck_t *addr : 栈顶地址
* 返回 :
* 	_m_statck_t* : 线程栈指针
*/
_m_statck_t* _m_thread_statck_init(void *entry,_m_statck_t *addr)
{
	struct _stack_frame *_stack_frame;
	_m_statck_t *stk;
	_m_uint32_t i;
	
	/* 因为调试中发现最好栈顶指针会向后偏移104个字节，所以这里主动向前偏移以防溢出 */
	addr -= 112;
	
	/* 获取向下8字对齐后的栈顶指针 */
	stk = (_m_statck_t*)_m_align_down((_m_uint32_t)addr,8);
	/* 让栈顶指针向下偏移 sizeof(struct _stack_frame) */
	stk -= sizeof(struct _stack_frame);
	/* 将 stk 指针强制转化为 _stack_frame 类型后存到 _stack_frame */
	_stack_frame = (struct _stack_frame *)stk;
	/* 以 _stack_frame 为起始地址，将栈空间里面的 sizeof(struct _stack_frame)个
		 内存初始化为 0xdeadbeef	*/
	for (i = 0; i < sizeof(struct _stack_frame) / sizeof(_m_uint32_t); i ++)
	{
		((_m_uint32_t *)_stack_frame)[i] = 0xdeadbeef;
	}
	/* 初始化异常发生时自动保存的寄存器 */
	_stack_frame->_exception_stack_frame.r0 = (unsigned long)_m_null; /* r0 : argument */
	_stack_frame->_exception_stack_frame.r1 = 0; /* r1 */
	_stack_frame->_exception_stack_frame.r2 = 0; /* r2 */
	_stack_frame->_exception_stack_frame.r3 = 0; /* r3 */
	_stack_frame->_exception_stack_frame.r12 = 0; /* r12 */
	_stack_frame->_exception_stack_frame.lr = 0; /* lr：暂时初始化为 0 */
	_stack_frame->_exception_stack_frame.pc = (unsigned long)entry; /* entry point, pc */
	_stack_frame->_exception_stack_frame.psr = 0x01000000L; /* PSR */
	
	
	/* 返回线程栈指针 */
	return stk;
}

/*
* 功能 :
* 	初始化列表,使列表指向自身
* 参数 :
* 	_m_list_t *list //列表指针
* 返回 :
* 	void
*/
void _m_list_init(_m_list_t *list)
{
    list->next = list;
    list->prev = list;
}

/*
* 功能 :
* 	在某节点之前插入一个新节点
* 参数 :
* 	_m_list_t *hlist //根据节点
* 	_m_list_t *ilist //要插入的节点
* 返回 :
* 	void
*/
void _m_list_insert_before(_m_list_t *list,_m_list_t *ilist)
{
	ilist->prev = list->prev;
	ilist->prev->next = ilist;
	ilist->next = list;
	list->prev = ilist;
}

/*
* 功能 :
* 	在某节点之后插入一个新节点
* 参数 :
* 	_m_list_t *hlist //根据节点
* 	_m_list_t *ilist //要插入的节点
* 返回 :
* 	void
*/
void _m_list_insert_after(_m_list_t *list,_m_list_t *ilist)
{
	ilist->next = list->next;
	ilist->next->prev = ilist;
	ilist->prev = list;
	list->next = ilist;
}

/*
* 功能 :
* 	删除节点
* 参数 :
* 	_m_list_t *rlist //要移除的节点
* 返回 :
* 	void
*/
void _m_list_remove(_m_list_t *rlist)
{
	rlist->prev->next = rlist->next;
	rlist->next->prev = rlist->prev;
	rlist->next = rlist;
	rlist->prev = rlist;
	
}

