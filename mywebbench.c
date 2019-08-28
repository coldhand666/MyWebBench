#include "socket.h"

//默认参数设置
int http10 = 1;				//默认HTTP版本为http/1.0 （0 - http/0.9, 1 - http/1.0, 2 - http/1.1）
int method = METHOD_GET;	//默认请求方法为GET
int clients = 1;			//默认只模拟一个客户端
int force = 0;				//默认需要等待服务器响应
int force_reload = 0;		//失败时重新请求
int proxyport = 80;			//默认访问服务器端口为80
char *proxyhost = NULL;		//默认无代理服务器
int benchtime = 10;			//默认请求时间为10s

/*
volatile:
类型修饰符，作为指令关键字，
确保本指令不会因为编译器优化而省略
且每次要求重新读值，
编译器在用到这个变量的时候都必须小心的重新读取这个变量的值，
而不是使用保存在寄存器里的备份，保证每次读到的都是最新的
*/
volatile int timerexpired = 0;

//测试结果
int speed = 0;  //成功得到服务器响应的子进程数量
int failed = 0; //没有成功得到服务器响应的子进程数量
int bytes = 0;  //所有子进程读取到服务器回复的总字节数

// 内部 
int mypipe[2];                //管道用于父子进程通信
char host[MAXHOSTNAMELEN];    //存储服务器网络地址
char request[REQUEST_SIZE];   //存放http请求报文信息数组

const struct option long_options[] =
{
	{ "force", no_argument, &force, 1 },
	{ "reload", no_argument, &force_reload, 1 },
	{ "time", required_argument, NULL, 't' },
	{ "help", no_argument, NULL, '?' },
	{ "http09", no_argument, NULL, '9' },
	{ "http10", no_argument, NULL, '1' },
	{ "http11", no_argument, NULL, '2' },
	{ "get", no_argument, &method, METHOD_GET },
	{ "head", no_argument, &method, METHOD_HEAD },
	{ "options", no_argument, &method, METHOD_OPTIONS },
	{ "trace", no_argument, &method, METHOD_TRACE },
	{ "version", no_argument, NULL, 'V' },
	{ "proxy", required_argument, NULL, 'p' },
	{ "clients", required_argument, NULL, 'c' },
	{ NULL, 0, NULL, 0 }
};

int main(int argc, char *argv[])
{
	//argc表示参数个数
	//argv[0]表示自身运行的路径和程序名
	//argv[1]指向第1个参数
	//argv[n]指向第n个参数

    int opt=0;
    int options_index=0;
    char *tmp=NULL;

	//1.命令行没有输入参数
    if(argc==1)
    {
        usage();
        return 2;
    } 

	//命令行有输入参数则一个个解析
	//"GHOT912Vfrt:c:p:?h"中一个字符后面加一个冒号代表该命令后面接一个参数
	//比如t,p,c命令，后面都要接一个参数
	//连续两个冒号则表示参数可有可无
    while((opt=getopt_long(argc,argv,"GHOT912Vfrt:c:p:?h",long_options,&options_index))!=EOF )
    {
        switch(opt)
        {
            case  0 : break;
			case 'G': method = METHOD_GET; break;
			case 'H': method = METHOD_HEAD; break;
			case 'O': method = METHOD_OPTIONS; break;
			case 'T': method = METHOD_TRACE; break;
            case '9': http10=0;break;
            case '1': http10=1;break;
            case '2': http10=2;break;
			case 'V': printf(PROGRAM_VERSION"\n"); exit(0);
			case 'f': force = 1; break;		//不等待服务器响应
			case 'r': force_reload = 1; break;	//重新请求加载(无缓存)
			case 't': benchtime = atoi(optarg); break;
			case 'c': clients = atoi(optarg); break;
            case 'p':	//使用代理服务器，则设置其代理网络号和端口号，格式：-p server:port
						//server:port是一个参数，下面把这个字符串解析成服务器地址和端口两个参数
            tmp=strrchr(optarg,':');	//在optagr中找到':'最后出现的位置
            proxyhost=optarg;
            if(tmp==NULL)	//没有端口号
            {
                break;
            }
            if(tmp==optarg)	//端口号在optarg最开头，说明缺失主机地址
            {
                fprintf(stderr,"Error in option --proxy %s: Missing hostname.\n",optarg);
                return 2;
            }
            if(tmp==optarg+strlen(optarg)-1)	//':'在最末尾，说明缺失端口号
            {
                fprintf(stderr,"Error in option --proxy %s Port number is missing.\n",optarg);
                return 2;
            }
            *tmp='\0';	//将optarg从':'开始截断，前面就是主机名，后面是端口号
            proxyport=atoi(tmp+1);break;	//设置代理服务器端口号
            case ':':
            case 'h':
            case '?': usage(); return 2; break;
			default : usage(); return 2; break;
        }
    }

	//命令参数解析完毕之后，刚好是读到URL，此时argv[optind]指向URL
	//URL参数为空
    if(optind==argc) {
        fprintf(stderr,"webbench: Missing URL!\n");
        usage();
        return 2;
    }

    if(clients<=0) 
		clients=1;
    if(benchtime<=0) 
		benchtime=10;
 
    /* Copyright */
    fprintf(stderr,"MyWebBench - A Simple Web Benchmark "PROGRAM_VERSION", GPL Open Source Software.\n");

	//构造请求报文
	build_request(argv[optind]);//参数为URL

    printf("Runing info: ");

    if(clients==1) 
        printf("1 client");
    else
        printf("%d clients",clients);
    printf(", running %d sec", benchtime);
    
    if(force) 
		printf(", early socket close");
    if(proxyhost!=NULL) 
		printf(", via proxy server %s:%d",proxyhost,proxyport);
    if(force_reload) 
		printf(", forcing reload,no cache");

	/*
	*换行不能少！库函数是默认行缓冲，子进程会复制整个缓冲区
	*若不换行刷新缓冲区,子进程会把缓冲区的也打出来
	*而换行后缓冲区就刷新了
	*子进程的标准库函数的那块缓冲区就不会有前面这些了
	*/
    printf(".\n");

	//开始压力测试
    return bench();
}
