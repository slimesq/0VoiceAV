#include <stdio.h>
#include <stdlib.h>
void fun( )
{
    int *p = (int *)malloc(10*sizeof(int));
    //p[10] = 0;
    free(p);
}

int main(void)
{
    fun(); 
    return 0;
}
