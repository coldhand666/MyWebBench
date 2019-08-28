#ifndef _SOCKET_H_
#define _SOCKET_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <rpc/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>

//http请求方法
#define METHOD_GET 0
#define METHOD_HEAD 1
#define METHOD_OPTIONS 2
#define METHOD_TRACE 3

#define PROGRAM_VERSION "1"
#define REQUEST_SIZE 2048

extern int http10;
extern int method;
extern int clients;
extern int force;
extern int force_reload;
extern int proxyport;
extern char *proxyhost;
extern int benchtime;

extern volatile int timerexpired;

extern int speed;
extern int failed;
extern int bytes;

extern int mypipe[2];
extern char host[MAXHOSTNAMELEN];
extern char request[REQUEST_SIZE];

extern const struct option long_options[15];


//构造http请求报文
void build_request(const char *url);

//父进程创建子进程，读取子进程测试得到的数据，然后统计处理
int bench(void);

//子进程真正相服务器发出请求报文并以其得到此期间的相关数据
void benchcore(const char* host, const int port, const char *request);

//闹钟信号处理函数
void alarm_handler(int signal);

//用法和各参数的详细意义
void usage(void);

//host:ip地址或者主机名		clientPort:端口
int Socket(const char *host, int clientPort);

#endif