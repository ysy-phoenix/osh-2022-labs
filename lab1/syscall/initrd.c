 #define _GNU_SOURCE
 #include <unistd.h>
 #include <sys/syscall.h>
 #include <sys/types.h>
 #include <signal.h>
 #include<stdio.h>

#define LENGTH1 5
#define LENGTH2 20
int main(int argc, char *argv[]) {
	char buf1[LENGTH1];
	char buf2[LENGTH2];
    
    int ret1 = syscall(548, buf1, LENGTH1);
    printf("length1: %d, return of syscall %d\n", LENGTH1, ret1);
    printf("buf1: %s\n", buf1);
    
    int ret2 = syscall(548, buf2, LENGTH2);
    printf("length2: %d, return of syscall %d\n", LENGTH2, ret2);
    printf("buf2: %s\n", buf2);
    
    while (1) {}
}
