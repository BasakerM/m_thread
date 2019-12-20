#ifndef _m_list_h
#define _m_list_h

#include "m_public.h"

/* 列表结构体 */
typedef struct m_struct_list_t
{
    struct m_struct_list_t *next; /* 列表的下一个节点 */
    struct m_struct_list_t *prev; /* 列表的上一个节点 */
}m_list_t;
/* 重命名一个指针类型 */
// typedef m_list_t *m_list_p_t;

void m_list_init(m_list_t *list);   /* 初始化一个列表 */
void m_list_add(m_list_t *hlist,m_list_t *nlist); /* 增加一个节点 */

#endif
