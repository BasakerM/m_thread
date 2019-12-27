#include "m_thread.h"
#include "stm32f4xx.h"

/**** 宏定义 ****/

#define _m_thread_statck_size 256 /* 单个线程的栈空间大小 */

#define _m_null (0)

/**** 功能宏定义 ****/

#define ALIGN(n) __attribute__((aligned(n)))
/* 向下对齐 */
#define _m_align_down(size, align) ((size) & ~((align) - 1))
/* 已知一个结构体里面的成员的地址，反推出该结构体的首地址 */
//#define _m_container_of(ptr, type, member) \
//((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))

#define _m_getThread(node, type, member) ((type *)((char *)(node) - (unsigned long)(&((type *)0)->member)))
//_m_container_of(node, type, member)

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
	_m_list_t timeout_list; /* 超时链表 */
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

/* 异常和中断处理表,在实现上下文切换的汇编文件中被调用 */
_m_uint32_t _m_interrupt_from_thread; /* 用于存储上一个线程的栈的 sp 的指针 */
_m_uint32_t _m_interrupt_to_thread; /* 用于存储下一个将要运行的线程的栈的 sp 的指针 */
_m_uint32_t _m_thread_switch_interrupt_flag; /* PendSV 中断服务函数执行标志 */

ALIGN(4)
_m_statck_t _m_thread_statck[m_thread_max+1][_m_thread_statck_size]; /* 线程栈 */
_m_thread_t _m_thread[m_thread_max+1]; /* 线程控制块 */
_m_thread_t *_m_current_thread; /* 当前线程控制块指针 */
_m_list_t _m_timeout_list_head; /* 超时链表头 */
_m_uint32_t _m_tick; /* 时基计数 */
_m_uint32_t _m_priority_ready_group; /* 优先级就绪组 */
_m_uint32_t _m_priority_ready_group_temp; /* 临时的就绪组,在初始化线程的时候先添加到这个组,在启动线程后拷贝到正式的就绪组 */

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

void m_thread_init(unsigned char threadmax, unsigned short stackmax); /* 初始化线程功能 */
void m_thread_creat(_m_uint8_t priority, void *entry); /* 创建一个线程 */
void m_thread_scheduler_startup(void); /* 启动线程 */
void m_thread_suspend(_m_uint32_t tick); /* 挂起线程 */

void m_tick_update(void); /* 计时函数 */

/**** 全局变量 ****/
m_thread_t m_thread = {m_thread_init, m_tick_update, m_thread_creat, m_thread_scheduler_startup, m_thread_suspend};

/**************** 函数定义 ****************/

/**************** 外部可调用函数 ****************/

//void m_interrupt_enter(void)
//{
//}

//void m_interrupt_leave(void)
//{
//}

/*
*	设置优先级就绪组中的对应位置
*/
static inline void _m_set_priorityReadyGroup(_m_uint8_t priority)
{
	if(priority >= 1 && priority <= m_thread_max)
	{
		_m_priority_ready_group |= 1 << ((priority - 1));
	}
}

/*
*	设置临时优先级就绪组中的对应位置
*/
static inline void _m_set_priorityReadyGroupTemp(_m_uint8_t priority)
{
	if(priority >= 1 && priority <= m_thread_max)
	{
		_m_priority_ready_group_temp |= 1 << ((priority - 1));
	}
}

/*
*	清除优先级就绪组中的对应位置
*/
static inline void _m_reset_priorityReadyGroup(_m_uint8_t priority)
{
	if(priority >= 1 && priority <= m_thread_max)
	{
		_m_priority_ready_group &= ~(1 << ((priority - 1)));
	}
}

/*
* 线程初始化函数
* 参数 :
* 	_void
* 返回 :
* 	void
*/
void m_thread_init(unsigned char threadmax, unsigned short stackmax)
{
	/* 初始化当前线程控制块指针 */
	_m_current_thread = _m_null;
	/* 初始化时基计数器 */
	_m_current_thread = 0;
	/* 初始化超时链表头指针 */
	_m_list_init(&_m_timeout_list_head);
	/* 初始化空闲线程 */
	m_thread_creat(0,_m_free_thread_entry);
	/* 手动指定第一个运行的线程 */
	_m_current_thread = &_m_thread[0]; /* 空闲线程 */
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
	/* 设置优先级 */
	_m_thread[priority].priority = priority;
	/* 初始化线程栈,获取栈顶指针	*/
	_m_thread[priority].sp = (void*)_m_thread_statck_init(entry,
			(void*)((char*)(_m_thread_statck[priority]) + _m_thread_statck_size - 2));
	/* 初始化超时链表 */
	_m_list_init(&(_m_thread[priority].timeout_list));
	/* 设置优先级就绪组的对应位 */
	_m_set_priorityReadyGroupTemp(priority);
}

/*
* 线程启动函数
* 返回 :
* 	void
*/
void m_thread_scheduler_startup(void)
{
	/* 将临时就绪组的值赋值到正式就绪组 */
	_m_priority_ready_group = _m_priority_ready_group_temp;
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
	_m_list_t* orderList = _m_null;
	_m_thread_t* orderthread = _m_null;

	/* 关中断 */
	m_interrupt_disable();
	/* 无效的参数 */
	if(tick == 0) { m_interrupt_enable(0); return; }
	/* 拿到超时链表中第一个节点 */
	orderList = _m_timeout_list_head.next;
	/* 计算超时时间 */
	_m_current_thread->timeout = tick + _m_tick;
	/* 排序插入超时链表 */
	for(; orderList != &_m_timeout_list_head; orderList = orderList->next)
	{
		/* 获取线程控制块的指针 */
		orderthread = _m_getThread(orderList,_m_thread_t,timeout_list);
		/* 超时时间小的排在前面,因为更接近计数时基(虽然小但也比计数时基大),所以从头开始比较(头上的最小),找到比自己大的链表,出循环后插在它前面 */
		/* 如果没有找到比自己小的,或者链表还没有挂载线程块,那出了循环后指针指向表头,插到表头前也就是插到表尾 */
		
		/* 当前线程的超时时间比该节点的小 */
		if(_m_current_thread->timeout < orderthread->timeout)
		{
			/* 有一种情况是,当发生上溢后,虽然数值上看是最小的,但实际上要最后运行应该插在链表的表尾 */
			/* 如果比其他线程超时时间小且比当前的计数时基还小,那就是发生了上溢,那么虽然数值更小,但是实际上在时间轴上应该排在后面 */
			if(_m_current_thread->timeout < _m_tick)
			{
				/* 链表该节点的超时时间比计时计数大,说明只有当前线程的超时时间发生了上溢,那么继续查找 */
				if(orderthread->timeout > _m_tick) { continue; }
				/* 链表该节点的超时时间也比计时计数小,说明都发生了上溢,那么数值小的先运行,应排序在该节点前面,退出循环插入 */
				else if(orderthread->timeout < _m_tick) { break; }
			}
			/* 没有发生上溢的情况,那么找到比自己大的就退出循环插入节点 */
			break;
		}
		/* 超时时间大 */
		else if(_m_current_thread->timeout > orderthread->timeout)
		{
			/* 有一种情况是,只有当前线程的超时时间没有发生上溢,虽然看上去数指大,但实际上应该插在链表的头上 */
			if(_m_current_thread->timeout > _m_tick)
			{
				/* 链表该节点的超时时间比计时计数小,说明只有当前线程的超时时间没有发生了上溢,应排序在该节点前面,退出循环插入 */
				if(orderthread->timeout < _m_tick) { break; }
				/* 链表该节点的超时时间也比计时计数大,说明都没有发生上溢,那么数值大的后运行,那么继续查找 */
				else if(orderthread->timeout > _m_tick) { continue; }
			}
		}
		/* 超时时间相等,那就比较优先级,优先级越小,等级越高 */
		else if(_m_current_thread->timeout == orderthread->timeout)
		{
			/* 找到比自己优先级大的就退出循环插入节点 */
			if(_m_current_thread->priority < orderthread->priority) { break; }
		}
	}
	/* 插入到超时链表 */
	_m_list_insert_before(orderList,&(_m_current_thread->timeout_list));
	/* 复位优先级就绪组中对应的位 */
	_m_reset_priorityReadyGroup(_m_current_thread->priority);
	
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
	_m_list_t* compareList = _m_null;
	_m_thread_t* compareThread = _m_null;
	
	/* 计时器计数 */
	++_m_tick;
	
//	/* 如果超时链表还没有节点,说明当前没有线程被挂起 */
//	/* 这种情况只有可能是刚启动线程功能时,只有空闲线程在运行,其他线程在初始化时已就绪,但未设置超时 */
//	/* 或者当前只有一个用户线程,且该线程正在运行未挂起 */
//	/* 所以这种情况需要先检查优先级就绪组,查看是否有线程已就绪,然后判断优先级最高的已就绪线程是否是当前线程,不是则执行调度 */
//	if(_m_timeout_list_head.next == &_m_timeout_list_head)
//	{
//		return;
//	}
	/* 查找超时链表中,超时时间到达的一个或多个线程,将他们在优先级就绪组中对应的位置置位 */
	compareList = _m_timeout_list_head.next;
	/* 当 */
	while(compareList != &_m_timeout_list_head)
	{
		/* 拿到线程控制块 */
		compareThread = _m_getThread(compareList,_m_thread_t,timeout_list);
		/* 超时时间到达 */
		if(_m_tick == compareThread->timeout)
		{
			/* 设置优先级就绪组 */
			_m_set_priorityReadyGroup(compareThread->priority);
			/* 指向下一个节点 */
			compareList = compareList->next;
			/* 从超时链表中移除 */
			_m_list_remove(compareList->prev);
			continue;
		}
		/* 因为超时链表是按照顺序排列的,所以一个未到达,那之后的都未到达,所以遇到未到达则直接跳出 */
		else { break; }
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
static inline void _m_thread_scheduler(void)
{
	_m_uint8_t index = 0; /* 线程控制块数组的下标,相当于是线程的优先级 */
//	_m_thread_t *to_thread = _m_null;
	_m_thread_t *from_thread = _m_null;
	
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
//		/* 将优先级就绪组中的对应位清零 */
//		_m_priority_ready_group &= ~(0x00000001 << (index - 1));
	}
	/* 如果就绪的线程就是当前运行的线程则不进行调度 */
	/* 通过优先级就绪组得到的就绪线程肯定是优先级最高的就绪线程 */
	/* 所以当前线程要么等于获得的就绪线程,要么优先级低于获得的就绪线程 */
	/* 所以只要在相等的时候不调度就行了 */
	if(_m_current_thread->priority == index)
	{
		return;
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
static inline void _m_list_init(_m_list_t *list)
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
static inline void _m_list_insert_before(_m_list_t *list,_m_list_t *ilist)
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
/*	未使用
static inline void _m_list_insert_after(_m_list_t *list,_m_list_t *ilist)
{
	ilist->next = list->next;
	ilist->next->prev = ilist;
	ilist->prev = list;
	list->next = ilist;
}
*/

/*
* 功能 :
* 	删除节点
* 参数 :
* 	_m_list_t *rlist //要移除的节点
* 返回 :
* 	void
*/
static inline void _m_list_remove(_m_list_t *rlist)
{
	rlist->prev->next = rlist->next;
	rlist->next->prev = rlist->prev;
	rlist->next = rlist;
	rlist->prev = rlist;
	
}

