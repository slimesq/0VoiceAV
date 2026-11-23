/*
 * file: g_thread.c
 * desc: 这个文件用于演示glib库里gthread的用法
 * compile: gcc -o g_thread g_thread.c `pkg-config --cflags --libs glib-2.0`
 */
#include <glib.h>
#include <glib/gprintf.h>
#include <unistd.h>
#include <sys/syscall.h>
 
gpointer thread_func1 (gpointer data)
{
	g_printf("%s %ld in\n", __func__, syscall(__NR_gettid));
	g_printf("%s %ld data is : %d\n", __func__, syscall(__NR_gettid), *((gint *)data));
	g_usleep (2000000);
	g_printf("%s %ld out\n", __func__, syscall(__NR_gettid));
 
	g_thread_exit ((gpointer)20);//等价于return
	//return (gpointer)20;
}
 
int main(int argc, char **argv)
{
	g_printf ("main in\n");
 
	gint func1_data = 11;
	GThread *gthread = NULL;
	gthread = g_thread_new ("func1", thread_func1, &func1_data);
	//gthread = g_thread_try_new ("func1", thread_func1, &func1_data);
	//if(gthread == NULL) g_printf ("g_thread_try_new fail\n");
 
	g_printf ("g_thread_join %ld\n", (gint64)g_thread_join (gthread));
	g_usleep (5000000);
	
	g_printf ("main out\n");
    	return 0;
}
