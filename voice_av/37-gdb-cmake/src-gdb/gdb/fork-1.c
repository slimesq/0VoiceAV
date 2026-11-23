#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(void) {
	int pid;
	
	printf("main process pid:%u\n", getpid());
	pid = fork();
	/* fork another process */
	if (pid < 0) {
		/* error occurred */
		fprintf(stderr, "Fork Failed\n");
		exit(-1);
	} else if (pid == 0) {
		/* child process */
		printf("child process pid:%u\n", getpid());
		printf("excute Child process\n");
		int ret = execlp( "/bin/ls", "ls",NULL);// 成功：无返回；失败：-1
	} else {
		/* parent process */
		printf("Wait child process\n");
		wait(NULL);
		/* parent will wait for the child to complete */
		printf( "Main wait child Complete\n");
		exit(0);
	}
	return 0;
}
