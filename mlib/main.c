#include "m_thread.h"

#include "stm32f4xx.h"

unsigned char flag1;
unsigned char flag2;
unsigned char flag3;
unsigned char flag4;

void f1(void)
{
	for (;;)
	{
		flag1 = 1;
		m_thread.sleep(2);
		flag1 = 0;
		m_thread.sleep(2);
	}
}

void f2(void)
{
	for (;;)
	{
		flag2 = 1;
		m_thread.sleep(5);
		flag2 = 0;
		m_thread.sleep(5);
	}
}

void f3(void)
{
	for (;;)
	{
		flag3 = 1;
		m_thread.sleep(5);
		flag3 = 0;
		m_thread.sleep(5);
	}
}

void f4(void)
{
	for (;;)
	{
		flag4 = 1;
		m_thread.sleep(10);
		flag4 = 0;
		m_thread.sleep(10);
	}
}

/*
*	初始化 systick
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

int main(void)
{
	unsigned char temp = 0;
	/* 关中断 */
	temp = m_interrupt_disable();
	
	m_systick_init(SystemCoreClock/1000);
	
	/* 初始化线程 */
	m_thread.init(4,256);
	/* 创建线程,优先级1 */
	m_thread.creat(1,f1);
	/* 创建线程,优先级2 */
	m_thread.creat(2,f2);
	/* 创建线程,优先级3 */
	m_thread.creat(3,f3);
	/* 创建线程,优先级4 */
	m_thread.creat(4,f4);
	/* 启动线程*/
	m_thread.startup();
	
	/* 开中断 */
	m_interrupt_enable(temp);
}

/*
*
*/
void SysTick_Handler(void)
{
	/* 线程的时基更新 */
	m_thread.tick();
}
