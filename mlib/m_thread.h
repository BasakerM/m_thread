#ifndef _m_thread_h
#define _m_thread_h

#include "m_public.h"

/* 宏定义 */

#define m_thread_max 2 /* 可创建最大线程数是16,优先级1-16 */

/* 函数声明 */
void m_thread_init(void);
void m_thread_creat(m_uint8_t priority, void *entry);
void m_thread_startup(m_uint8_t priority);
void m_thread_scheduler_startup(void);
void m_thread_suspend(m_uint32_t tick);

void m_tick_update(void);

//void m_interrupt_enter(void);
//void m_interrupt_leave(void);

m_uint8_t m_interrupt_disable(void);
void m_interrupt_enable(m_uint8_t);
#define m_interrupt_enter()
#define m_interrupt_leave()

#endif
