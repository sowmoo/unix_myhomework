/*
	유닉스 실습 소스파일 
            
        각 #include 파일은 함수가 필요할때마다 쉘스크립트에서 man을 사용 
 
        구현못한 함수들은 외부에서 끌어오는 식으로 사용 
        (실제 구현안된 명령어드래도 유닉스 쉘스크립트에 구현되있다면 사용가능) 
 
        cmd 실행파일을 사용하여 외부 파일도 실행이 가능하나... 뻑날 위험 있음 (책임못짐)
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <errno.h>
#include <utime.h>
#include <fcntl.h>

#define SZ_STR_BUF		256		// 일반 문자열 배열 길이
#define SZ_FILE_BUF 1024

char *cmd;
char *argv[100];
char *optv[10];
int  argc, optc;
char cur_work_dir[SZ_STR_BUF]; // dirtory name save buffer

#define PRINT_ERR_RET() do { perror(cmd);  return; }while(0)

#define EQUAL(_s1, _s2) 	(strcmp(_s1, _s2) == 0) // 두개 문자열 같으면 true
#define NOT_EQUAL(_s1, _s2)	(strcmp(_s1, _s2) != 0)	// 두개 문자열 다르면 true

static void
print_usage(char *msg, char *cmd, char *opt, char *arg)
{
	printf("%s%s", msg, cmd);	// "사용법: ls"
	if (NOT_EQUAL(opt, "")) 	// 옵션이 있으면
		printf("  %s", opt);	// " -l"
	printf("  %s\n", arg);		// " [디렉토리 이름]"
}

static int
check_arg(int count)
{
	if(count < 0)
	{
		count = -count;
		if(argc <= count)
		{
			return 0;
		}
	}
	if(argc == count)
	{
		return 0;
	}
	if(argc > count)
	{
		printf("Too many arguments. \n");
	}
	else
	{
		printf("Insufficient arguments.\n");
	}
	return -1;
}

static int
check_opt(char *opt)
{
	int i, err = 0;

	for(i = 0; i<optc; ++i)
	{
		if(NOT_EQUAL(opt, optv[i]))
		{
			printf("Not supported option(%s).\n", optv[i]);
			err = -1;
		}
	}
	return(err);
}


static char *
get_argv_optv(char *cmd_line) //strtok에 의해 공백을 기준으로 쪼개서 argv, optv에 저장 argc, optc는 카운팅증가 
{
	char *tok;
	argc = optc = 0;
	if((cmd = strtok(cmd_line, " \t\n")) == NULL)
	{
		return(NULL);
	}

	for( ; (tok = strtok(NULL, " \t\n")) != NULL; )
	{
		if(tok[0] == '-')
		{
			optv[optc++] = tok;
		}
		else
		{
			argv[argc++] = tok;
		}
	}
	return(cmd);
}

///////////// here implemented ls() function ///////////////
static void
print_attr(char *path, char *fn)
{
	struct passwd *pwp;
	struct group *grp;
	struct stat st_buf;
	char full_path[SZ_STR_BUF], buf[SZ_STR_BUF], c;
	char time_buf[13];
	struct tm *tmp;

	sprintf(full_path, "%s/%s", path, fn);

	if(lstat(full_path, &st_buf) < 0)
	{
		PRINT_ERR_RET();
	}

	if(S_ISREG(st_buf.st_mode))
	{
		c = '-';
	}
	else if(S_ISDIR(st_buf.st_mode))
	{
		c = 'd';
	}
	else if(S_ISCHR(st_buf.st_mode))
	{
		c = 'c';
	}
	else if(S_ISBLK(st_buf.st_mode))
	{
		c = 'b';
	}
	else if(S_ISFIFO(st_buf.st_mode))
	{
		c = 'f';
	}
	else if(S_ISLNK(st_buf.st_mode))
	{
		c = 'l';
	}
	else if(S_ISSOCK(st_buf.st_mode))
	{
		c = 's';
	}

	buf[0] = c;
	buf[1] = (st_buf.st_mode & S_IRUSR) ? 'r' : '-';
	buf[2] = (st_buf.st_mode & S_IWUSR) ? 'w' : '-';
	buf[3] = (st_buf.st_mode & S_IXUSR) ? 'x' : '-';
	buf[4] = (st_buf.st_mode & S_IRGRP) ? 'r' : '-';
	buf[5] = (st_buf.st_mode & S_IWGRP) ? 'w' : '-';
	buf[6] = (st_buf.st_mode & S_IXGRP) ? 'x' : '-';
	buf[7] = (st_buf.st_mode & S_IROTH) ? 'r' : '-';
	buf[8] = (st_buf.st_mode & S_IWOTH) ? 'w' : '-';
	buf[9] = (st_buf.st_mode & S_IXOTH) ? 'x' : '-';
	buf[10] = '\0';

	pwp = getpwuid(st_buf.st_uid);
	grp = getgrgid(st_buf.st_gid);
	tmp = localtime(&st_buf.st_mtime);

	strftime(time_buf, 13, "%b %d %H:%M", tmp);
	
	sprintf(buf+10, " %3ld %-8s %-8s %8ld %s %s", st_buf.st_nlink, pwp->pw_name, grp->gr_name,st_buf.st_size, time_buf, fn);

	if(S_ISLNK(st_buf.st_mode))
	{
		int len, bytes;
		strcat(buf, " -> ");
		len = strlen(buf);
		bytes = readlink(full_path, buf+len, SZ_STR_BUF-len);
		buf[len+bytes] = '\0';
	}
	printf("%s\n",buf);
}

static void 
print_detail(DIR *dp, char *path)
{
	struct dirent *dirp;

	while ((dirp = readdir(dp)) != NULL)
	{
		print_attr(path, dirp->d_name);
	}
}

static void 
get_max_name_len(DIR *dp, int *p_max_name_len, int *p_num_per_line)
{
	struct dirent *dirp;
	int max_name_len = 0;
		
	while((dirp = readdir(dp)) != NULL)
	{
		int name_len = strlen(dirp->d_name);
		if(name_len > max_name_len)
		{
			max_name_len = name_len;
		}
	}
	
	readdir(dp);
	max_name_len += 4;

	*p_num_per_line = 80 / max_name_len;
	*p_max_name_len = max_name_len;
}

static void 
print_name(DIR *dp)
{
	struct dirent *dirp;
	int max_name_len, num_per_line, cnt = 0;

	get_max_name_len(dp, &max_name_len, &num_per_line);

	while((dirp = readdir(dp)) != NULL)
	{
		printf("%-*s", max_name_len, dirp->d_name);

		if((++cnt % num_per_line) == 0)
		{
			printf("\n");
		}
	}

	if((cnt % num_per_line) != 0)
	{	
		printf("\n");
	}
}


void
cat(void)
{
	int fd, len;
	char buf[SZ_FILE_BUF];

	if((fd=open(argv[0], O_RDONLY)) < 0)
	{
		PRINT_ERR_RET();
	}
	
	while( (len = read(fd, buf, SZ_FILE_BUF)) > 0)
	{
		if(write(STDOUT_FILENO, buf, len) != len)
		{
			len = -1;
			break;
		}
	}

	if(len < 0)
	{
		perror(cmd);
	}

	close(fd);

}

void
cp(void)
{
	int rfd, wfd, len;
	char buf[SZ_FILE_BUF];
	struct stat st_buf;

	if((stat(argv[0], &st_buf) < 0 ) || ((rfd = open(argv[0], O_RDONLY)) < 0))
	{
		PRINT_ERR_RET();
	}

	if((wfd = creat(argv[1], st_buf.st_mode)) < 0)
	{
		perror(cmd);
		close(rfd);
		return;
	}

	while((len = read(rfd, buf, SZ_FILE_BUF)) > 0)
	{
		if(write(wfd, buf, len) != len)
		{
			len = -1;
			break;
		}
	}


	if(len < 0)
	{
		perror(cmd);
	}
	
	close(rfd);
}

void 
cd(void)
{
	struct passwd *pwp;

	pwp = getpwuid(getuid());

	if(argc == 0)
	{
		argv[0] = pwp->pw_dir;
	}

	if(chdir(argv[0]) < 0)
	{
		PRINT_ERR_RET();
	}
	else
	{	
		getcwd(cur_work_dir,SZ_STR_BUF);	
	}

}

void 
changemod()
{
	int mode; // 8 ginsou
	sscanf(argv[0], "%o", &mode);
	
	chmod(argv[1],mode);

	if(chmod(argv[1], mode) < 0)
	{
		PRINT_ERR_RET();
	}
}

void 
date()
{
	char stm[128];
	time_t ttm = time(NULL);
	struct tm *ltm = localtime(&ttm);

	printf("atm: %s", asctime(ltm));
	printf("ctm: %s", ctime(&ttm));
	strftime(stm, 128, "%a %b %d %H:%M:%S %Y",ltm);
	printf("stm: %s\n",stm);
}

void
echo(void)
{
	int i;
	for(i=0; i<argc; i++)
	{
		printf("%s ",argv[i]);
	}
	printf("\n");
}

void
hostname(void)
{
	char hostname[SZ_STR_BUF];
	gethostname(hostname,MAXHOSTNAMELEN);
	
	if(hostname != NULL)
	{
		printf("%s\n",hostname);
	}
	else
	{
		printf("NULL\n");
	}
}

void
ln(void)
{
	int optc;  //trash code
	if(( (argc!=0) ? symlink(argv[0],argv[1]) : link(argv[0],argv[1])) < 0)
	{
		PRINT_ERR_RET();
	}
}

void
id(void)
{
	struct passwd *pwp;
	struct group *grp;

	pwp = (argc)? getpwnam(argv[0]):getpwuid(getuid());
	if((pwp==NULL) || ((grp = getgrgid(pwp->pw_gid)) == NULL))
	{
		printf("id: 잘못된 사용자 이름: \"%s\"\n", argv[0]);
	}
	else
	{
		printf("uid = %d(%s) gid = %d(%s)\n",pwp->pw_uid, pwp->pw_name, grp->gr_gid,grp->gr_name);
	}



	/* 
	더미데이터 
	if(argc == 1)
	{
		pwp = getpwnam(*argv);
	}
	else
	{
		pwp = getpwuid(getuid());
	}
	
	if(pwp == NULL)
	{
		printf("id: 잘못된 사용자 이름: "\%s\"\n",*argv);
	}

	grp = getgrgid();
	
	if(grp == NULL)
	{
		PRINT_ERR_RET();
	}
	*/

}


void 
ls(void)
{
	char *path;
	DIR	*dp;

	path = (argc == 0)? ".":argv[0];

	if((dp = opendir(path)) == NULL)
	{
		PRINT_ERR_RET();
	}

	if(optc == 0)
	{
		print_name(dp);
	}
	else
	{
		print_detail(dp, path);
	}
	closedir(dp);

}

void
makedir(void) //mkdir함수 
{
	if(mkdir(argv[0], 0755) <0)
	{
		PRINT_ERR_RET();
	}
	//0755 : rwxr-xr-x	
}

void 
mv(void)
{
	if( link(argv[0],argv[1]) < 0 ||  unlink(argv[0]) < 0)
	{
		PRINT_ERR_RET();
	}
}


void
pwd(void)
{
	printf("%s\n",cur_work_dir);
}

void
rm(void)
{
	int ret;
	struct stat buf;

	if((lstat(argv[0], &buf))<0 || (S_ISDIR(buf.st_mode)) ? rmdir(argv[0]):unlink(argv[0])<0)
	{
		PRINT_ERR_RET();
	}


}

void
removedir(void) //rmdir 함수
{
	if(rmdir(argv[0]) < 0)
	{
		PRINT_ERR_RET();
	}
}

void 
run_cmd(void)
{
	pid_t pid;

	if((pid = fork()) < 0)
	{
		PRINT_ERR_RET();
	}
	else if(pid == 0)
	{
		int i, cnt = 0;
		char *nargv[100];

		nargv[cnt++] = cmd;

		for(i =0; i<optc; ++i)
		{
			nargv[cnt++] = optv[i];
		}
		for(i=0; i< argc; ++i)
		{
			nargv[cnt++] = argv[i];
		}
		nargv[cnt++] = NULL;

		if(execvp(cmd, nargv) < 0)
		{
			perror(cmd);
			exit(1);
		}
	}
	else
	{
		if(waitpid(pid, NULL, 0) < 0)
		{
			PRINT_ERR_RET();
		}
	}

}

void 
Sleep(void) 
{
	int sec;
	sscanf(argv[0], "%d", &sec);

	int s = sec;
	sleep(s);
}

void
touch(void) //해당파일을 현재시간으로 바꿈 (만약 파일이 없으면 크기 0 파일생성) 
{
	int fd;

	if(utime(argv[0], NULL) == 0)
	{
		return;
	}

	if((errno != ENOENT) || ((fd = creat(argv[0],0644))<0))
	{
		PRINT_ERR_RET();
	}

	close(fd);
}

void
unixname(void)
{
	struct utsname un;
	uname(&un);
	
	printf("%s ",un.sysname);
	
	if(optc == 1)
	{
		printf("%s %s %s %s",un.nodename, un.release, un.version, un.machine);
	}
	printf("\n");

}

void
whoami(void) 
{
	struct passwd *pwp;

	pwp = getpwuid(getuid());

	if(pwp != NULL)
	{
		printf("%s\n", pwp->pw_name);
	}
	else
	{
		printf("NULL\n");
	}

    /*  더미코드
        
	char *username = getpwuid();
	
	if(username != NULL)
	{
		printf("%s\n",username);
	}
	else
	{
		printf("NULL\n");
	}
   */

}

void
quit(void) // 리눅스상에서 제공해주는 인터럽트 종료함수 exit 호출 
{
	exit(0);
}


void help(void);


#define AC_LESS_1		-1		// 명령어 인자 개수가 0 또는 1인 경우
#define AC_ANY			-100	// 명령어 인자 개수가 제한 없는 경우(echo)

typedef struct {
	char *cmd;			// 명령어 문자열
	void (*func)(void);	// 명령어를 처리하는 함수 시작 주소(함수 이름)
	int  argc;			// 명령어 인자의 개수(명령어와 옵션은 제외)
	char *opt;			// 옵션 문자열: 예) "-a", 옵션 없으면 ""
	char *arg;			// 명령어 사용법 출력할 때 사용할 명령어 인자: 
} cmd_tbl_t;			//		예) cp인 경우 "원본파일 복사파일""

/*
	  cmd_tbl_t[] 설명 
	  
	  사용자 입력          실행함수		     명령어인자개수      옵션               설명 		
	                      (포인터)             (명령어, 옵션제외)    
*/
	cmd_tbl_t cmd_tbl[] = { 
	{ "cat",		cat,		1,			"",		"파일이름"},
	{ "cp",			cp,			2,			"",		"원본파일  복사된파일" },
	{ "cd",			cd,			AC_LESS_1,	"",		"[디렉토리이름]"},
	{ "chmod",		changemod,	2,			"",		"8진수모드값 파일이름"},
	{ "date",		date,		0,			"",		""},
	{ "echo",		echo,		AC_ANY,		"",		"[에코할 문장]" },
	{ "help",		help,		0,			"",		"" },
	{ "hostname",	hostname,	0,			"",		"" },
	{ "id",			id,			AC_LESS_1,	"",		"[계정이름]"},
	{ "ln",			ln,			2,			"-s",	"원본파일 링크파일"},
	{ "ls",			ls,			AC_LESS_1,	"-l",	"[디렉토리이름]" },
	{ "exit",		quit,		0,			"",		"" },
	{ "mv",			mv,			2,			"",		"원본파일 바뀐이름"},
	{ "pwd",		pwd,		0,			"",		""},
	{ "rm",			rm,			1,			"",		"[파일이름]" },
	{ "whoami",		whoami,		0,			"",		""},
	{ "uname",		unixname,	0,			"-a",	""},
	{ "mkdir",		makedir,	1,			"",		"디렉토리이름"},
	{ "rmdir",		removedir,	1,			"",		"디렉토리이름"},
	{ "touch",		touch,		1,			"",		"파일이름"},
	{ "sleep",		Sleep,		1,			"",		"초단위시간"},
};
int num_cmd = sizeof(cmd_tbl) / sizeof(cmd_tbl[0]);

void
help(void)
{
	int  k;

	printf("현재 지원되는 명령어 종류입니다.\n");
	for (k = 0; k < num_cmd; ++k)
		print_usage("  ", cmd_tbl[k].cmd, cmd_tbl[k].opt, cmd_tbl[k].arg);
	printf("\n");
}

void
proc_cmd(void) //명령어 숫자 혹은 옵션이 잘된됬는지 체크하는 함수 
{
	int  k;

	for (k = 0; k < num_cmd; ++k) 
		{	
		if (EQUAL(cmd, cmd_tbl[k].cmd)) //EQUAL은 매크로상수로 지정되어있어 전처리기에 의해 컴파일시 가장먼저 변환됨 
			{

			if ((check_arg(cmd_tbl[k].argc) < 0) || 	
				(check_opt(cmd_tbl[k].opt)  < 0))		
				
				print_usage("사용법: ", cmd_tbl[k].cmd,
									    cmd_tbl[k].opt, cmd_tbl[k].arg);
				 
			else
				cmd_tbl[k].func();		

			return;
		}
	}
	run_cmd();
}

int 
main(int argc, char *argv[])
{
	int cmd_count = 1;

	char cmd_line[SZ_STR_BUF]; 	// 입력된 명령어 라인 전체를 저장 공간

	setbuf(stdout, NULL);	// 표준 출력 버퍼 제거: printf() 즉시 화면 출력
	setbuf(stderr, NULL);	// 표준 에러 출력 버퍼 제거

	help();						// 명령어 전체 리스트 출력
	
	getcwd(cur_work_dir, SZ_STR_BUF);

	for (;;) {
		printf("<%s> %d: ",cur_work_dir,cmd_count); //현재위치 출력과 쉘스크립트 입력된 카운팅 개수 (기준은 '\n')

		fgets(cmd_line, SZ_STR_BUF, stdin); // << help를 출력후 여기서 대기 

		if (get_argv_optv(cmd_line) != NULL) { //아무것도 입력안될경우 통과 
			proc_cmd();	// 한 명령어 처리 (strtok에 의해 잘 쪼개져서 argv argc optv optc 에 값 변경) 
			cmd_count++; //쉘 스크립트 카운터 증가 
		}
	}
}
