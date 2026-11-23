#include <stdio.h>
#include <string.h>

int main(void)
{
    char x[50] = {0};
    
    strncpy(x+20, x, 20);   //ok
    strncpy(x+20, x, 21); // verlap
    strncpy(x, x+20, 20);   //ok
    strncpy(x, x+20, 21); // verlap

    return 0;
}