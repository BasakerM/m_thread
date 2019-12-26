#ifndef _m_thread_h
#define _m_thread_h

#include "m_public.h"

/*
*	要解决的问题:
*		1.超时链表的排序问题
*/

/* 宏定义 */

#define m_thread_max 4 /* 可创建最大线程数是16,优先级1-16 */

/* 线程结构体 */
typedef struct
{
	/*
	*	初始化线程功能
	*	参数:
	*		unsigned char : 允许创建的最大线程数(目前是固定的,在实现动态内存后改参数有效)
	*		unsigned short : 每个线程的栈空间(目前是固定的,在实现动态内存后改参数有效)
	*/
	void (*init)(unsigned char, unsigned short);
	/*
	*	线程的计时函数(需要每1ms调用一次,建议放在systick中)
	*/
	void (*tick)(void);
	/*
	*	创建一个线程
	*	参数:
	*		unsigned char : 线程的优先级
	*		void* : 线程的函数
	*/
	void (*creat)(unsigned char, void*);
	/*
	*	启动线程(在所有线程创建完成后再调用启动函数)
	*/
	void (*startup)(void);
	/*
	*	线程休眠
	*	参数:
	*		unsigned short : 休眠的时间,单位ms
	*/
	void (*sleep)(unsigned long);
}m_thread_t;
/* 线程的全局变量,通过这个变量可以使用线程功能 */
extern m_thread_t m_thread;

/* 函数声明 */
m_uint8_t m_interrupt_disable(void);
void m_interrupt_enable(m_uint8_t);

#endif
