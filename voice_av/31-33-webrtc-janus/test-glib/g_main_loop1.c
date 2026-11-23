/*
 * file: g_main_loop1.c
 * desc: 这个文件用于演示glib库里GMainLoop的用法
 * compile: gcc -o g_main_loop1 g_main_loop1.c `pkg-config --cflags --libs glib-2.0`
 */
#include<glib.h> 
GMainLoop* loop;
 
gint counter = 10;
gboolean callback(gpointer arg)
{
    g_print(".");
    if(--counter ==0){
        g_print("\n");
        //退出循环
        g_main_loop_quit(loop);
        //注销定时器
        return FALSE;
    }
    //定时器继续运行
    return TRUE;
}
 
int main(int argc, char* argv[])
{
    g_print("g_main_loop_new\n");
    loop = g_main_loop_new(NULL, FALSE);
    //增加一个定时器，100毫秒运行一次callback
    g_timeout_add(100,callback,NULL);
    g_print("g_main_loop_run\n");
    g_main_loop_run(loop);
    g_print("g_main_loop_unref\n");
    g_main_loop_unref(loop);
    return 0;
}
