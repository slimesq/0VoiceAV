/* g_async_queue.c: 这个文件用于演示glib库里GAsyncQueue的用法
 * compile: gcc -o g_async_queue g_async_queue.c `pkg-config --cflags --libs glib-2.0`
 */
#include <glib.h>
#include <glib/gprintf.h>
 
GAsyncQueue *async_queue = NULL;
 
gpointer thread_func1 (gpointer data)
{
	gint *usr_data;
	while(1)
	{
		g_printf ("%s in\n", __func__);
		g_async_queue_lock (async_queue);
		usr_data = (gint *)g_async_queue_pop_unlocked (async_queue);
		g_printf("%s pop: %d\n", __func__, *usr_data);
		g_free (usr_data);
		g_printf ("queue length %d\n", g_async_queue_length_unlocked (async_queue));
		g_async_queue_unlock (async_queue);
		g_usleep (2000000);
	}
}
 
gpointer thread_func2 (gpointer data)
{
	gint *tmp;
	gint count = 0;
	while(1)
	{
		g_printf ("%s in\n", __func__);
		g_async_queue_lock (async_queue);
		tmp = (gint *)g_new0(gint, 1);
		*tmp = count++;
		g_async_queue_push_unlocked (async_queue, tmp);
		g_printf("%s count: %d\n", __func__, *tmp);
		g_async_queue_unlock (async_queue);
		g_usleep (1000000);
	}
}
 
 
int main(int argc, char **argv)
{
	g_printf ("%s in\n", __func__);
 
	GThread *gthread1 = NULL, *gthread2 = NULL;
	async_queue = g_async_queue_new ();
	gthread2 = g_thread_new ("func2", thread_func2, NULL);
	g_usleep (1000000);
	gthread1 = g_thread_new ("func1", thread_func1, NULL);
	g_thread_join (gthread1);
	g_thread_join (gthread2);
	
	g_printf ("%s out\n", __func__);
    	return 0;
}
