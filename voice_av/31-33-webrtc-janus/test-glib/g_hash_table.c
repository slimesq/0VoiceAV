/*
 * file: g_hash_table.c
 * desc: 这个文件用于演示glib库里GHashTable的用法
 * compile: gcc -o g_hash_table g_hash_table.c `pkg-config --cflags --libs glib-2.0`
 */
#include <stdio.h>
#include <glib.h>
#include <glib/gprintf.h>
 
struct map {
    int key;
    char *value;
} m[10] = {
    {1,"one"},
    {2,"two"},
    {3,"three"},
    {4,"four"},
    {5,"five"},
    {6,"six"},
    {7,"seven"},
    {8,"eight"},
    {9,"nine"},
    {10,"ten"}
};
 
typedef struct map map;
 
static gboolean myHRFunc(gpointer key, gpointer value, gpointer user_data)
{
    gint a = *(gint *)key;
    gint b = *(gint *)user_data;
 
    return a == b ? TRUE : FALSE;
}
 
static void myIterator(gpointer key, gpointer value, gpointer user_data)
{
    printf(user_data, *(gint*)key, value);
}
 
 
static void printKey(gpointer p1, gpointer p2)
{
    printf(p2, *(gint*)p1);
}
 
static void printValue(gpointer p1, gpointer p2)
{
    printf(p2, p1);
}
 
 
static void test_hash_1(void)
{
// GHashTable* g_hash_table_new(GHashFunc hash_func, GEqualFunc key_equal_func);    创建一个1个引用的哈希表
// hash_func : 从一个关键字创建哈希值的函数。主要有g_int_hash() 和 g_str_hash() 如果这个函数是null，使用g_direct_hash()
// key_equal_func : 这个函数去检查两个键值是否相等。这个函数在哈希表查找键值时使用。对应上一个参数有对应的函数g_direct_equal(), g_int_equal() and g_str_equal()
// g_int_hash()这个函数是在使用int型变量作为键时使用的函数，参数的一个指针，返回int类型。
// g_str_hash()这个函数是在使用string型为键时使用的函数，参数是指针，返回int类型。
    GHashTable *hash = g_hash_table_new(g_int_hash, g_int_equal);
    gint i;
 
// void g_hash_table_insert(GHashTable *hash_table, gpointer key, gpointer value);    把key键value值插入到hash_table
    for (i = 0; i < sizeof(m)/sizeof(m[0]); i++)
        g_hash_table_insert(hash, &m[i].key, m[i].value);
 
// guint g_hash_table_size(GHashTable *hash_table);         返回表中的键值个数
    g_printf("It should has '%d' keys in the hash now.\t\tResult: %d.\n", 10, g_hash_table_size(hash));
// gpointer g_hash_table_lookup(GHashTable *hash_table, gconstpointer key);     
// 通过key查找值，注意，这个函数不能分辨key值是否存在，或者值是否存在(也就是值是否为null)，如果需要可以使用函数g_hash_table_lookup_extended()。换句话说，用这个函数的时候需要确定键值是存在的
    g_printf("The value of the second key should be '%s' now.\t\tResult: %s.\n", m[1].value, (gchar *)g_hash_table_lookup(hash, &m[1].key));
// gboolean g_hash_table_remove(GHashTable *hash_table, gconstpointer key);    
// 删除key键值，如果哈希表使用g_hash_table_new_full()创建，这个键值将会被销毁功能释放掉内存，如果不是用_full()函数创建需要程序员自己释放内存
    gboolean found = g_hash_table_remove(hash, &m[8].key);
    g_printf("The key '%d' was %sfound and removed now.\n", m[8].key, found ? "" : "not ");
    found = g_hash_table_remove(hash, &m[8].key);
    g_printf("The key '%d' was %sfound and removed now.\n", m[8].key, found ? "" : "not ");
    g_hash_table_insert(hash, &m[8].key, m[8].value);
// gpointer g_hash_table_find(GHashTable *hash_table, GHRFunc predicate, gpointer user_data);       调用predicate函数，判断键值对，直到函数返回值为true表示找到。使用该函数会遍历哈希表，但是不能增删改表
    g_printf("The key '%d' should be there now.\t\tResult: %s.\n", m[8].key, g_hash_table_find(hash, myHRFunc, &m[8].key) == NULL ? "NO" : "YES");
 
// void g_hash_table_replace(GHashTable *hash_table, gpointer key, gpointer value);         和g_hash_table_insert()函数一样插入键值，不同的是如果键已经存在则替换值。如果你实现了value_destroy_func 或者key_destroy_func函数，在替换后旧的数据会被释放掉
    g_hash_table_replace(hash, &m[2].key, "2222");
    g_printf("The value of the third key should be '%s' now.\t\tResult: %s.\n", "2222", (gchar *)g_hash_table_lookup(hash, &m[2].key));
    g_printf("The all items in hash table is :\n");
// void g_hash_table_foreach(GHashTable *hash_table, GHFunc func, gpointer user_data);      遍历哈希表，然后将键值传递给func函数
    g_hash_table_foreach(hash, myIterator, "Key:\t%d\t\tValue:\t%s\n");
 
 
// GList* g_hash_table_get_keys(GHashTable *hash_table);    获得哈希表中的所有key
    GList *lkey = g_hash_table_get_keys(hash);
    g_list_foreach(lkey, printKey, "%d\t");
    g_printf("\n");
 
// GList* g_hash_table_get_values(GHashTable *hash_table);      获得哈希表中所有的value
    GList *lvalue = g_hash_table_get_values(hash);
    g_list_foreach(lvalue, printValue, "%s\t");
    g_printf("\n");
 
// void g_hash_table_remove_all(GHashTable *hash_table);    删除所有在GHashTable中的键及其关联值。是否释放资源同样受到创建函数的影响。
    g_hash_table_remove_all(hash);
    g_printf("Now all items in hash table is :\n");
    g_hash_table_foreach(hash, myIterator, "Key:\t%d\t\tValue:\t%s\n");
 
 
// void g_hash_table_destroy(GHashTable *hash_table);       释放所有键值，并将哈希表的引用计数置位1.如果你使用的是new()函数创建表需要自己释放键值，如果使用new_full()会调用释放函数自动释放
    g_hash_table_destroy(hash);
}
 
int main(void)
{
    printf("BEGIN:\n************************************************************\n");
    test_hash_1();
    printf("\n************************************************************\nDONE\n");
    
    return 0;
}
