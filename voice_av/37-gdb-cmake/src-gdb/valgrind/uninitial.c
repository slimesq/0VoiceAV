/*
 * @Descripttion: 
 * @version: 1.0
 * @Author: Darren
 * @Date: 2019-10-31 11:59:34
 * @LastEditors: Darren
 * @LastEditTime: 2019-10-31 14:27:11
 */
#include <stdio.h>
#include <stdlib.h>
void uninitial()
{
    int condition;
    if(condition)
    {
        printf("condition is true");
    }
    else
    {
        printf("condition is false");
    }
}
void overboundary()
{
    char *v = (char *)malloc(5);
    if(!v)
    {   
        printf("%s(%d)malloc failed\n", __FUNCTION__, __LINE__);
        return;
    }

    v[5] = 6; //Oop

    free(v);
}

void destroystack()
{
    char v[] = { 1, 2, 3, 4, 5 };

    v[5] = 6; 
}

// gcc -g -O0 uninitial.c -o uninitial
int main(void)
{
    // uninitial();
    overboundary();
    // destroystack();
    
    return 0;
}