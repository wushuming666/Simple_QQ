#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{	
	// ./procct1 5 /bin/ls -lt /tmp/tmp.txt
	if (argc < 3)
	{
		printf("Using: ./procct1 timetv1 program argv ...\n");
		printf("Example:/project/tools1/bin/procct1 5 /bin/tar zcvf /tmp/tmp.tgz /usr/include\n\n");

		printf("本程序是服务程序的调度程序,周期性启动服务程序或shell脚本\n");
		printf("timetv1 运行周期,单位:秒  被调度的程序运行结束后，在timetv1秒后会被procct1重新启动\n");
		printf("program 被调度的程序名，必须使用完整路径\n");
		printf("argvs   被调度的程序的参数\n");
		printf("注意，本程序不会被kill杀死，但可以用kill -9强行杀死\n\n\n");
		return -1;
	}

	// 关闭所有信号和io
	for (int ii=0; ii<64; ii++)
	{
		signal(ii,SIG_IGN); 
		close(ii);
	}

	// 生成子进程，父进程退出，让程序运行在后台，由一号进程接管
	if (fork() != 0) exit(0);

	// 启用SIGCHLD信号，让父进程可以wait子进程退出的状态（僵尸进程）
	signal(SIGCHLD,SIG_DFL);

	char *pargv[argc-1];
	for (int ii=2;ii<argc;ii++)
		pargv[ii-2] = argv[ii];

	pargv[argc-2] = NULL;

	while (true)
	{
		if (fork() == 0)
		{
			execv(argv[2], pargv);
			exit(0);	//execv调用失败被执行，不加死循环	
		}
		else
		{
			int status;
			wait(&status);
			sleep(atoi(argv[1]));
		}
	}	
	return 0;
}