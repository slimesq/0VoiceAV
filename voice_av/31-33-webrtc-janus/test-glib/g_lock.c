/*
 * file: g_lock.c
 * desc: 这个文件用于演示glib库里LOCK的用法
 * compile: gcc -o g_lock g_lock.c `pkg-config --cflags --libs glib-2.0`
 */
#include <glib.h>
#include <glib/gprintf.h>
#include <unistd.h>
#include <sys/syscall.h>
 
G_LOCK_DEFINE (thread_mutex);
gint count = 0;
 
gpointer thread_func1 (gpointer data)
{
	gint tmp;
	while(1)
	{
		G_LOCK (thread_mutex);
		g_printf("%s %ld count: %d\n", __func__, syscall(__NR_gettid), count);
		tmp = count;
		count++;
		g_usleep (2000000);
		g_printf("%s %ld count++ tmp: %d count: %d\n", __func__, syscall(__NR_gettid), tmp, count);
		G_UNLOCK (thread_mutex);
		g_usleep (20000);
	}
}
 
gpointer thread_func2 (gpointer data)
{
	gint tmp;
	while(1)
	{
		G_LOCK (thread_mutex);
		g_printf("%s %ld count: %d\n", __func__, syscall(__NR_gettid), count);
		tmp = count;
		count++;
		g_usleep (2000000);
		g_printf("%s %ld count++ tmp: %d count: %d\n", __func__, syscall(__NR_gettid), tmp, count);
		G_UNLOCK (thread_mutex);
		g_usleep (20000);
	}
}
 
 
int main(int argc, char **argv)
{
	g_printf ("main in\n");
 
	GThread *gthread1 = NULL, *gthread2 = NULL;
	gthread1 = g_thread_new ("func1", thread_func1, NULL);
	gthread2 = g_thread_new ("func2", thread_func2, NULL);
	g_thread_join (gthread1);
	g_thread_join (gthread2);
	
	g_printf ("main out\n");
    	return 0;
}
