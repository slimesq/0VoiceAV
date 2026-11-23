/*g_thread_pool.c: 这个文件用于演示glib库里GThreadPool的用法
 * compile: gcc -o g_thread_pool g_thread_pool.c `pkg-config --cflags --libs glib-2.0`
 */
#include <glib.h>
#include <glib/gprintf.h>
#include <unistd.h>
#include <sys/syscall.h>
//#include <pthread.h>
 
//用来排序任务列表
gint compare_data (gconstpointer a, gconstpointer b, gpointer user_data)
{
	if(user_data == NULL)//从小到大
	{
		if(*((gint *)a) > *((gint *)b))
			return 1;
		else
			return -1;
	}
	else//从大到小
	{
		if(*((gint *)a) < *((gint *)b))
			return 1;
		else
			return -1;
	}
}
 
//线程执行函数，进入后打印数据，并睡眠5秒
void thread_execute (gpointer data, gpointer user_data)
{
	g_printf("thread_execute %ld in\n", syscall(__NR_gettid));
	g_printf("thread_execute %ld data is : %d\n", syscall(__NR_gettid), *((gint *)data));
	g_usleep (5000000);
	g_printf("thread_execute %ld out\n", syscall(__NR_gettid));
}
 
gint data[10];
int main(int argc, char **argv)
{
	g_printf ("main in\n");
	gint count;
	GError *gerr = NULL;
	GThreadPool *gpool = NULL;
	//最大同时执行2个线程，由于exclusive设置为FALSE，所以不会有错误发生，error被设置为NULL
	gpool = g_thread_pool_new (thread_execute, NULL, 2, FALSE, &gerr);
	if(gerr != NULL) {
		/* Something went wrong... */
		g_printf("Got error %d (%s) trying to launch the request pool task thread...\n",
			gerr->code, gerr->message ? gerr->message : "??");
		g_error_free(gerr);
		exit(1);
	}
	g_printf("thread pool max threads is %d\n", g_thread_pool_get_max_threads (gpool));
	g_printf("thread pool num threads is %d\n", g_thread_pool_get_num_threads (gpool));
	
	//线程池中最大允许线程数3个
	if(!g_thread_pool_set_max_threads (gpool, 3, &gerr))
		g_printf("g_thread_pool_set_max_threads is error: %s\n", gerr->message);
 
	g_printf("thread pool max threads is %d\n", g_thread_pool_get_max_threads (gpool));
	g_printf("thread pool num threads is %d\n", g_thread_pool_get_num_threads (gpool));
	
	/** 
	* 实际线程池中最多有3个线程同时运行 
	*/  
	for (count = 0; count < (sizeof(data)/sizeof(gint)); count++)
	{
		data[count] = count;
		g_thread_pool_push(gpool, (gpointer)(&(data[count])), &gerr);
		if(gerr != NULL)
		{
			g_printf("g_thread_pool_push is error: %s\n", gerr->message);
		}
	}
 
	g_usleep (100000);
	g_printf("thread pool num threads is %d\n", g_thread_pool_get_num_threads (gpool));
	g_printf("thread pool unprocessed num is %d\n", g_thread_pool_unprocessed (gpool));
	g_printf("thread pool num unused threads is %d\n", g_thread_pool_get_num_unused_threads ());
	
	//插入优先级高的新任务
	if(g_thread_pool_move_to_front (gpool, (gpointer)(&(data[9]))))
		g_printf("g_thread_pool_move_to_front is 9\n");
	
	//立即释放，不继续执行任务队列
	//g_thread_pool_free (gpool, TRUE, TRUE);
	//等待任务队列中的任务全部执行完成
	g_thread_pool_free (gpool, FALSE, TRUE);
	g_printf("thread pool num unused threads is %d\n", g_thread_pool_get_num_unused_threads ());
	
	//设置最大空闲时间2s
	g_thread_pool_set_max_idle_time (2000);
	
	g_usleep (5000000);
	g_printf("thread pool num unused threads is %d\n", g_thread_pool_get_num_unused_threads ());
	
	
	//最大同时执行1个线程，由于exclusive设置为FALSE，所以不会有错误发生，error被设置为NULL
	gpool = g_thread_pool_new (thread_execute, NULL, 1, FALSE, NULL);
	
	/** 
	* 实际线程池中最多有3个线程同时运行 
	*/  
	for (count = 0; count < (sizeof(data)/sizeof(gint)); count++)
	{
		data[count] = count;
		g_thread_pool_push(gpool, (gpointer)(&(data[count])), &gerr);
		if(gerr != NULL)
		{
			g_printf("g_thread_pool_push is error: %s\n", gerr->message);
		}
	}
	
	//从小到大
	//g_thread_pool_set_sort_function (gpool, compare_data, NULL);
	//从大到小
	g_thread_pool_set_sort_function (gpool, compare_data, (gpointer)(1));
	
	//等待任务队列中的任务全部执行完成
	g_thread_pool_free (gpool, FALSE, TRUE);
	//while(1);
	g_printf ("main out\n");
    	return 0;
}
