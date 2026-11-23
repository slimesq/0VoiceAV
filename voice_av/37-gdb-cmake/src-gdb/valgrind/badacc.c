#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    int len = 4;
    int *pt = (int *)malloc((len+1) * sizeof(int));
    int *p = pt;
    for (int i = 0; i < len; i++)
    {
        p++;
    }
    *p = 5;
    printf("the value of p equal:%d\n", *p);
    free(pt);
    
    return 0;
}