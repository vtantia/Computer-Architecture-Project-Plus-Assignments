#include <stdio.h>

int main() {
    int a = 4;
    long long b = 0x01234567;
    b <<= 32;
    b += 0x89abcdef;
    memcpy(&a, ((void *)&b) + sizeof(int) / 2, sizeof(int));
    printf("%x\n", *(int*)((void*)&b+4));
    printf("%x\n", *(int*)(void*)(&b));
    printf("%x\n", a);
}
