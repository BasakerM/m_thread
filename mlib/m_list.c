#include "m_list.h"

/*
*   原型 :
*       void m_list_init(m_list_t *list)
*   功能 :
*       初始化列表,使列表指向自身
*   参数 :
*       m_list_t *list //列表指针
*   返回 :
*       void
*/
void m_list_init(m_list_t *list)
{
    list->next = list;
    list->prev = list;
}

/*
*   原型 :
*       void m_list_add(m_list_t *hlist,m_list_t *nlist)
*   功能 :
*       增加节点
*   参数 :
*       m_list_t *hlist //头节点
*       m_list_t *nlist //新节点
*   返回 :
*       void
*/
void m_list_add(m_list_t *hlist,m_list_t *nlist)
{
    hlist->next->prev = nlist;
    nlist->next = hlist->next;
    hlist->next = nlist;
    nlist->prev = hlist;
}
