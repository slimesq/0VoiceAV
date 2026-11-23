/*
 * file: g_list.c
 * desc: 这个文件用于演示glib库里双向链表GList的用法
 * compile: gcc -o g_list g_list.c `pkg-config --cflags --libs glib-2.0`
 */
#include <stdio.h>
#include <glib.h>

void display_list(GList *list)
{
    GList *it = NULL;

    /* 遍历链表 */
    for (it = list; it; it = it->next) {
        printf("%s", it->data);
    }

    printf("\n");
}

int main(int argc, char *argv[])
{
    GList *list = NULL;

    /*
     * 向链表尾部追加节点
     */
    list = g_list_append(list, "one ");
    list = g_list_append(list, "two ");
    list = g_list_append(list, "three ");
    list = g_list_append(list, "four ");
    list = g_list_append(list, "five ");
    display_list(list);

    /* 在链表头部插入*/
    list = g_list_prepend(list, "zero ");
    display_list(list);

    GList *it = NULL;

    /* 查找链表中的节点 */
    it = g_list_find(list, "two ");
    printf("Find data \"two \": %s\n", it->data);

    int index = 0;

    /* 确定链表指针指向链表中的第几个节点 */
    index = g_list_position(list, it);
    printf("index of \"two \" is: %d\n", index);

    it = g_list_nth(list, 1);
    printf("%s\n", it->data);

    /* 从链表里删除一个节点 */
    printf("After remove data \"three \", the list:\n");
    list = g_list_remove(list, "three ");
    display_list(list);

    /* 向链表里插入一个节点 */
    printf("After insert string \"INSERT \" at index 3, the list:\n");
    list = g_list_insert(list, "INSERT ", 3);
    display_list(list);

    /* 采用内循环遍历链表节点 */
    g_list_foreach(list, (GFunc)printf, NULL);
    printf("\n\n");

    GList *last = NULL;

    /* 取链表的最后一个节点 */
    last = g_list_last(list);

    /* 从最后一个节点开始遍历链表 */
    for (it = last; it; it = it->prev) {        
	printf("%s", it->data);    
    }    
    printf("\n");    
    /* 销毁链表 */    
    g_list_free(list);    
    return 0;
}
