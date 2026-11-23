/*
 * file: g_main_loop0.c
 * desc: 这个文件用于演示glib库里GMainLoop的用法
 * compile: gcc -o g_main_loop0 g_main_loop0.c `pkg-config --cflags --libs glib-2.0`
 */
#include<glib.h> 
GMainLoop* loop;
int main(int argc, char* argv[])
{
    //创建一个循环体，先不管参数的意思。
    g_print("g_main_loop_new/n");
    loop = g_main_loop_new(NULL, FALSE);
    //让这个循环体跑起来
    g_print("g_main_loop_run/n");
    g_main_loop_run(loop);
    //循环运行完成后，计数器减一
    //glib的很多结构类型和c++的智能指针相似，拥有一个计数器
    //当计数器为0时，自动释放资源。
    g_print("g_main_loop_unref/n");
    g_main_loop_unref(loop);
    return 0;
}
