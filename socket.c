#include "socket.h"

//构造http报文请求到request数组
void build_request(const char *url)
{
	//存放端口号的中间数组
	char tmp[10];
	//存放url中主机名开始的位置
	int i;

	memset(host, 0, MAXHOSTNAMELEN);
	memset(request, 0, REQUEST_SIZE);

	//判断应该使用的http协议

	//1.缓存和代理都是都是http/1.0以后才有到的
	if (force_reload && proxyhost != NULL && http10<1) 
		http10 = 1;

	//2.head请求是http/1.0后才有的
	if (method == METHOD_HEAD && http10<1) 
		http10 = 1;
	//3.options请求和trace请求都是http/1.1才有
	if (method == METHOD_OPTIONS && http10<2) 
		http10 = 2;
	if (method == METHOD_TRACE && http10<2) 
		http10 = 2;

	//开始填写http请求
	switch (method)
	{
	default:
		case METHOD_GET: 
			strcpy(request, "GET"); 
			break;
		case METHOD_HEAD: 
			strcpy(request, "HEAD"); 
			break;
		case METHOD_OPTIONS: 
			strcpy(request, "OPTIONS"); 
			break;
		case METHOD_TRACE: 
			strcpy(request, "TRACE"); 
			break;
	}

	strcat(request, " ");

	//判断url的合法性

	if (NULL == strstr(url, "://"))
	{
		fprintf(stderr, "\n%s: is not a valid URL.\n", url);
		exit(2);//url不合法
	}
	
	if (strlen(url)>1500)
	{
		fprintf(stderr, "URL is too long.\n");
		exit(2);//url过长
	}

	//忽略字母大小写比较前7位
	if (0 != strncasecmp("http://", url, 7))
	{
		fprintf(stderr, "\nOnly HTTP protocol is directly supported, set --proxy for others.\n");
		exit(2);
	}

	//找到主机名开始的地方
	i = strstr(url, "://") - url + 3;

	//从主机名开始的地方开始往后找，没有 '/' 则url非法
	if (strchr(url + i, '/') == NULL) {
		fprintf(stderr, "\nInvalid URL syntax - hostname don't ends with '/'.\n");
		exit(2);
	}

	//无代理服务器
	if (proxyhost == NULL)
	{
		//存在端口号
		if (index(url + i, ':') != NULL && index(url + i, ':')<index(url + i, '/'))
		{
			//填充主机名到host字符数组
			strncpy(host, url + i, strchr(url + i, ':') - url - i);

			//初始化存放端口号的中间数组
			memset(tmp, 0, 10);

			//切割得到端口号
			strncpy(tmp, index(url + i, ':') + 1, strchr(url + i, '/') - index(url + i, ':') - 1);

			//设置端口号 atoi将字符串转整型
			proxyport = atoi(tmp);

			//避免写了';'却没有写端口号，这种情况下默认设置端口号为80
			if (proxyport == 0)
				proxyport = 80;
		}
		else	//不存在端口号
		{
			//填充主机名到host字符数组，比如www.baidu.com
			strncpy(host, url + i, strcspn(url + i, "/"));
		}

		//将主机名，以及可能存在的端口号以及请求路径填充到请求报文中
		//比如url为http://www.baidu.com:80/one.jpg/
		//就是将www.baidu.com:80/one.jpg填充到请求报文中
		strcat(request + strlen(request), url + i + strcspn(url + i, "/"));
	}
	else    //存在代理服务器
	{
		strcat(request, url);
	}

	//	if (http10 == 0)
	//		strcat(request," HTTP/0.9");
	//   else 
	if (http10 == 1)
		strcat(request, " HTTP/1.0");
	else if (http10 == 2)
		strcat(request, " HTTP/1.1");

	strcat(request, "\r\n");

	if (http10>0)
		strcat(request, "User-Agent: MyWebBench "PROGRAM_VERSION"\r\n");

	//不存在代理服务器且http协议版本为1.0或1.1，填充Host字段
	//当存在代理服务器或者http协议版本为0.9时，不需要填充Host字段
	//因为http0.9版本没有Host字段，而代理服务器不需要Host字段
	if (proxyhost == NULL && http10>0)
	{
		strcat(request, "Host: ");
		strcat(request, host);//Host字段填充的是主机名或者IP
		strcat(request, "\r\n");
	}

	/*pragma是http/1.1之前版本的历史遗留问题，仅作为与http的向后兼容而定义
	规范定义的唯一形式：
	Pragma:no-cache
	若选择强制重新加载，则选择无缓存
	*/
	if (force_reload && proxyhost != NULL)
	{
		strcat(request, "Pragma: no-cache\r\n");
	}

	/*我们的目的是构造请求给网站，不需要传输任何内容，所以不必用长连接
	http/1.1默认Keep-alive(长连接）
	所以需要当http版本为http/1.1时要手动设置为 Connection: close
	*/
	if (http10>1)
		strcat(request, "Connection: close\r\n");

	//在末尾填入空行
	if (http10>0) strcat(request, "\r\n");

	printf("\nRequest:\n%s\n", request);
}

int bench(void)
{
	int i, j, k;
	pid_t pid = 0;//进程号定义(int)
	FILE *f;

	//先检查一下目标服务器是可用性
	i = Socket(proxyhost == NULL ? host : proxyhost, proxyport);

	//目标服务器不可用,退出
	if (i<0) {
		fprintf(stderr, "\nConnect to server failed. Aborting benchmark.\n");
		return 1;
	}

	//否则表明尝试连接成功了，关闭该连接
	close(i);

	//建立父子进程通信的管道
	if (pipe(mypipe))
	{
		perror("pipe failed.");
		return 3;
	}

	
	//创建数量为clients的子进程进行压力测试
	for (i = 0; i<clients; i++)
	{
		pid = fork();
		if (pid <= (pid_t)0)
		{
			/* child process or error*/
			sleep(1); //出错或子进程挂起1秒，将cpu时间交给父进程以快速fork子进程
			break;	//防止子进程继续fork
		}
	}

	//pid<0表示fork子进程出错
	if (pid < (pid_t)0)
	{
		fprintf(stderr, "problems forking worker no. %d\n", i);
		perror("fork failed.");
		return 3;
	}

	//pid=0表示当前进程是子进程
	if (pid == (pid_t)0)
	{
		//由子进程发出http请求报文
		if (proxyhost == NULL)
			benchcore(host, proxyport, request);
		else
			benchcore(proxyhost, proxyport, request);

		//子进程获得管道写端的文件指针，准备进行管道写
		f = fdopen(mypipe[1], "w");

		//管道写端打开失败
		if (f == NULL)
		{
			perror("open pipe for writing failed.");
			return 3;
		}

		//向管道中写入该孩子进程在一定时间内请求成功的次数、失败次数、读取到服务器回复的总字节数
		fprintf(f, "%d %d %d\n", speed, failed, bytes);
		fclose(f);

		return 0;
	}
	//我是父进程
	else
	{
		//父进程获得管道读端的文件指针
		f = fdopen(mypipe[0], "r");

		//管道读端打开失败
		if (f == NULL)
		{
			perror("open pipe for reading failed.");
			return 3;
		}

		/*
		fopen标准IO函数是自带缓冲区的
		我们输入的数据非常短，并且数据要及时
		所以没有缓冲是最合适的
		我们不需要缓冲区
		因此把缓冲类型设置为_IONBF*/
		setvbuf(f, NULL, _IONBF, 0);

		speed = 0;  //http请求报文连接成功次数
		failed = 0; //http请求报文失败的请求次数
		bytes = 0;  //服务器回复的总字节数

		//父进程不停的读
		while (1)
		{ 
			//读入参数以及得到成功得到的参数的个数
			pid = fscanf(f, "%d %d %d", &i, &j, &k);
			if (pid<3)
			{
				fprintf(stderr, "Some of our childrens died.\n");
				break;
			}

			speed += i;
			failed += j;
			bytes += k;

			//记录已经读了多少个子进程的数据，读完退出
			if (--clients == 0)
				break;
		}

		//关闭读端
		fclose(f);

		//统计处理结果
		printf("\nSpeed=%d pages/min, %d bytes/sec.\nRequests: %d succeed, %d failed.\n",
			(int)((speed + failed) / (benchtime / 60.0f)),
			(int)(bytes / (float)benchtime),
			speed,failed);
	}

	return i;
}

void benchcore(const char *host, const int port, const char *req)
{
	int rlen,ischoked=0;
	char buf[1500];//记录服务器响应请求返回的数据
	int s, i;
	struct sigaction sa;//信号处理函数定义

	//设置alarm_handler函数为闹钟信号处理函数
	sa.sa_handler = alarm_handler;
	sa.sa_flags = 0;

	//超时会产生信号SIGALRM，用sa中指定函数处理
	if (sigaction(SIGALRM, &sa, NULL))
		exit(3);

	alarm(benchtime); //闹钟信号处理函数开始计时，超时后timerexpired置1

	rlen = strlen(req);//得到请求报文的长度
	while (1)
	{
			ischoked = 0;
			if (timerexpired)//超时
			{
				if (failed>0)
				{
					failed--;
				}
				return;
			}

			//建立到目的网站的tcp连接,发送http请求
			s = Socket(host, port);

			//连接失败
			if (s<0) 
			{
				failed++; 
				continue; 
			}
			//发出请求报文

			//实际写入的字节数和请求报文字节数不相同，写失败，发送1失败次数+1
			if (rlen != write(s, req, rlen)) 
			{
				failed++; 
				close(s); 
				continue; 
			}

			//http/0.9的特殊处理
			/*
			*因为http/0.9是在服务器回复后自动断开连接
			*在此可以提前先彻底关闭套接字的写的一半，如果失败了那肯定是个不正常的状态
			*事实上，关闭写后，服务器没有写完数据也不会再写了，这个就不考虑了
			*如果关闭成功则继续往后，因为可能还需要接收服务器回复的内容
			*当这个写一定是可以关闭的，因为客户端也不需要写，只需要读
			*因此，我们主动破坏套接字的写，但这不是关闭套接字，关闭还是得用close
			*/
			if (http10 == 0 && shutdown(s, 1))
			{ 
				failed++; 
				close(s); 
				continue; 
			}

			//需要等待服务器回复
			if (force == 0)
			{
				//从套接字读取所有服务器回复的数据
				while (1)
				{
					//超时标志为1时，不再读取服务器回复的数据
					if (timerexpired)
						break;

					//读取套接字中1500个字节数据到buf数组中，如果套接字中数据小于要读取的字节数1500会引起阻塞 返回-1
					i = read(s, buf, 1500);
					//read返回值：

					//未读取任何数据   返回   0
					//读取成功         返回   已经读取的字节数
					//阻塞             返回   -1

					//读取阻塞了
					if (i<0)
					{
						failed++;
						close(s);
						ischoked = 1;
						break;
					}
					else if (i == 0)	
						break;//没有读取到任何字节数
					else
						bytes += i;//从服务器读取到的总字节数增加
				}
			}

			/*
			close返回返回值
			成功   返回 0
			失败   返回 -1
			*/

			if (!ischoked)
			{
				if (close(s))//套接字关闭失败
				{
					failed++;
					continue;
				}
				//套接字关闭成功,服务器响应的子进程数量+1
				speed++;
			}
	}
}


void alarm_handler(int signal)
{
	timerexpired = 1;
}

void usage(void)
{
	fprintf(stderr,
		"webbench [option]... URL\n"
		"  -f|--force               Don't wait for reply from server.\n"
		"  -r|--reload              Send reload request - Pragma: no-cache.\n"
		"  -t|--time <sec>          Run benchmark for <sec> seconds. Default 10.\n"
		"  -p|--proxy <server:port> Use proxy server for request.\n"
		"  -c|--clients <n>         Run <n> HTTP clients at once. Default one.\n"
		"  -9|--http09              Use HTTP/0.9 style requests.\n"
		"  -1|--http10              Use HTTP/1.0 protocol.\n"
		"  -2|--http11              Use HTTP/1.1 protocol.\n"
		"  -G|--get                 Use GET request method.\n"
		"  -H|--head                Use HEAD request method.\n"
		"  -O|--options             Use OPTIONS request method.\n"
		"  -T|--trace               Use TRACE request method.\n"
		"  -?|-h|--help             This information.\n"
		"  -V|--version             Display program version.\n"
		);
}

/*

sockaddr_in分析：

#include <netinet/in.h>和#include <arpa/inet.h>定义的

struct sockaddr
{
__SOCKADDR_COMMON (sa_);  //协议族

char sa_data[14];         //地址+端口号
};

sockaddr缺陷：把目标地址和端口号混在一起了
而sockaddr_in就解决了这一缺陷
将端口号和IP地址分开存储

struct sockaddr_in
{
sa_family_t sin_family;     //地址族

uint16_t sin_port;          //16位TCP/UDP端口号

struct in_addr sin_addr;    //32位IP地址

char sin_zero[8];           //不使用，只为了内存对齐
};

*/

/*

hostent分析：
host entry的缩写
记录主机信息包括主机名，别名，地址类型，地址长度和地址列表

struct hostent
{

char *h_name;         //正式主机名

char **h_aliases;     //主机别名

int h_addrtype;       //主机IP地址类型：IPV4-AF_INET

int h_length;		  //主机IP地址字节长度，对于IPv4是四字节，即32位

char **h_addr_list;	  //主机的IP地址列表

};
#define h_addr h_addr_list[0]   //保存的是IP地址

主机的的地址是列表形式的原因：
当一个主机又多个网络接口时，自然有多个地址

*/

//host:ip地址或者主机名		clientPort:端口
int Socket(const char *host, int clientPort)
{
    int sock;
    unsigned long inaddr;
    struct sockaddr_in ad;//地址信息
    struct hostent *hp;//主机信息

	//因为host可能是ip地址或者主机名，所以当host为主机名的时候需要通过主机名得到IP地址

    memset(&ad, 0, sizeof(ad));
    ad.sin_family = AF_INET;//采用TCP/IP协议族
	
    inaddr = inet_addr(host);//点分十进制IP转化为二进制IP

	//输入为IP地址
    if (inaddr != INADDR_NONE)
        memcpy(&ad.sin_addr, &inaddr, sizeof(inaddr));//将IP地址复制给ad的sin_addr属性
	//输入不是IP地址，是主机名
    else
    {
        hp = gethostbyname(host);//通过主机名得到主机信息
        if (hp == NULL)//获取主机信息失败
            return -1;
        memcpy(&ad.sin_addr, hp->h_addr, hp->h_length);//将IP地址复制给ad的sin_addr属性
    }

	/*
	将端口号从主机字节顺序变成网络字节顺序
	就是整数在地址空间存储方式变为高字节存放在内存低字节处

	网络字节顺序是TCP/IP中规定好的一种数据表示格式，与CPU和操作系统无关
	从而可以保证数据在不同主机之间传输时能够被正确解释
	网络字节顺序采用大尾顺序：高字节存储在内存低字节处
	*/
    ad.sin_port = htons(clientPort);

	/*
	AF_INET:     IPV4网络协议
	SOCK_STRAM:  提供面向连接的稳定数据传输，即TCP协议
	*/
	//创建一个采用IPV4和TCP的socket
    sock = socket(AF_INET, SOCK_STREAM, 0);

	//创建socket失败
    if (sock < 0)
        return sock;

	//建立连接 连接失败返回-1
    if (connect(sock, (struct sockaddr *)&ad, sizeof(ad)) < 0)
        return -1;

	//创建成功，返回socket
    return sock;
}
