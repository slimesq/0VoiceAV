/*
 * file: g_mutex_cond.c
 * desc: 这个文件用于演示glib库里GMutex和GCond的用法
 * compile: gcc -o g_mutex_cond g_mutex_cond.c `pkg-config --cflags --libs glib-2.0`
 */
#include <glib.h>
#include <glib/gprintf.h>
#include <unistd.h>
#include <sys/syscall.h>
 
GMutex *thread_mutex, *broad_mutex;
GCond *cond1, *broad_cond;
gint count = 0, broad_flag = 0;
 
gpointer decrement (gpointer data)
{
	g_printf("%s in\n", __func__);
	
	g_mutex_lock (thread_mutex);
	
	while (count == 0)
        g_cond_wait(cond1, thread_mutex);
	
	count--;
	g_printf("%s decrement:%d.\n", __func__, count);
	g_printf("out decrement.\n");
	
	g_mutex_unlock (thread_mutex);
	
	g_printf("%s out\n", __func__);
	return NULL;
}
 
gpointer increment (gpointer data)
{
	g_printf("%s in\n", __func__);
	
	g_mutex_lock (thread_mutex);
	count++;
	g_printf("%s increment:%d.\n", __func__, count);
	if (count != 0)
        g_cond_signal (cond1);
 
	g_mutex_unlock (thread_mutex);
 
	g_printf("%s out\n", __func__);
	return NULL;
}
 
gpointer broadcast (gpointer data)
{
	g_printf("%s in\n", __func__);
	
	g_usleep (5000000);
	g_mutex_lock (broad_mutex);
 
	//todo something
	broad_flag = 1;
    g_cond_broadcast (broad_cond);
 
	g_mutex_unlock (broad_mutex);
 
	g_printf("%s out\n", __func__);
	return NULL;
}
 
gpointer trigger1 (gpointer data)
{
	g_printf("%s in\n", __func__);
	g_mutex_lock (broad_mutex);
	
	while (broad_flag == 0)
        g_cond_wait(broad_cond, broad_mutex);
	g_printf("%s recv broad_cond\n", __func__);
	
	g_mutex_unlock (broad_mutex);
	g_printf("%s out\n", __func__);
}
 
gpointer trigger2 (gpointer data)
{
	g_printf("%s in\n", __func__);
	g_mutex_lock (broad_mutex);
	
	while (broad_flag == 0)
        g_cond_wait(broad_cond, broad_mutex);
	g_printf("%s recv broad_cond\n", __func__);
	
	g_mutex_unlock (broad_mutex);
	g_printf("%s out\n", __func__);
}
 
gpointer trigger3 (gpointer data)
{
	g_printf("%s in\n", __func__);
	g_mutex_lock (broad_mutex);
	
	while (broad_flag == 0)
        g_cond_wait(broad_cond, broad_mutex);
	g_printf("%s recv broad_cond\n", __func__);
	
	g_mutex_unlock (broad_mutex);
	g_printf("%s out\n", __func__);
}
 
int main(int argc, char **argv)
{
	g_printf ("main in\n");
 
	GThread *gthread1 = NULL, *gthread2 = NULL;
	cond1 = g_new(GCond, 1);
	g_cond_init (cond1);
	thread_mutex = g_new(GMutex, 1);
	g_mutex_init (thread_mutex);
	
	gthread1 = g_thread_new ("func1", decrement, NULL);
	g_usleep (2000000);
	gthread2 = g_thread_new ("func2", increment, NULL);
	g_thread_join (gthread1);
	g_thread_join (gthread2);
	
	g_mutex_clear (thread_mutex);
	g_cond_clear(cond1);
	
	g_printf ("----------broadcast cond test-----------\n");
	GThread *gthread3 = NULL, *gthread4 = NULL;
	broad_cond = g_new(GCond, 1);
	g_cond_init (broad_cond);
	broad_mutex = g_new(GMutex, 1);
	g_mutex_init (broad_mutex);
	
	gthread1 = g_thread_new ("broadcast", broadcast, NULL);
	gthread2 = g_thread_new ("trigger1", trigger1, NULL);
	gthread3 = g_thread_new ("trigger2", trigger2, NULL);
	gthread4 = g_thread_new ("trigger3", trigger3, NULL);
	g_thread_join (gthread1);
	g_thread_join (gthread2);
	g_thread_join (gthread3);
	g_thread_join (gthread4);
	
	g_mutex_clear (broad_mutex);
	g_cond_clear(broad_cond);
	
	g_printf ("main out\n");
 	return 0;
}
