#include "m_public.h"
#include "m_thread.h"

#include "stm32f4xx.h"

unsigned char flag1;
unsigned char flag2;
unsigned char flag3;

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

/* 软件延时，不必纠结具体的时间 */
void delay(unsigned char count )
{
	for (; count!=0; count--);
}

void f1(void)
{
	for (;;)
	{
		flag1 = 1;
//		delay(100);
		m_thread_suspend(10);
		flag1 = 0;
//		delay(100);
		m_thread_suspend(10);
	}
}

void f2(void)
{
	for (;;)
	{
		flag2 = 1;
//		delay(100);
		m_thread_suspend(5);
		flag2 = 0;
//		delay(100);
		m_thread_suspend(5);
	}
}

void f3(void)
{
	for (;;)
	{
		flag3 = 1;
//		delay(100);
		m_thread_suspend(5);
		flag3 = 0;
//		delay(100);
		m_thread_suspend(5);
	}
}

int main(void)
{
	unsigned char temp = 0;
	/* 关中断 */
	temp = m_interrupt_disable();
	
	/* 初始化线程 */
	m_thread_init();
	/* 初始化 systick */
	m_systick_init(SystemCoreClock/1000);
	
	/* 创建线程,优先级1 */
	m_thread_creat(1,f1);
	/* 创建线程,优先级2 */
	m_thread_creat(2,f2);
	/* 创建线程,优先级3 */
	m_thread_creat(3,f3);
	/* 启动线程*/
	m_thread_startup(1);
	m_thread_startup(2);
//	m_thread_startup(3);
	m_thread_scheduler_startup();
	/* 开中断 */
	m_interrupt_enable(temp);
}

void SysTick_Handler(void)
{
	/* 时基更新 */
	m_tick_update();
}
