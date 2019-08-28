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

//http���󷽷�
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


//����http������
void build_request(const char *url);

//�����̴����ӽ��̣���ȡ�ӽ��̲��Եõ������ݣ�Ȼ��ͳ�ƴ���
int bench(void);

//�ӽ�����������������������Ĳ�����õ����ڼ���������
void benchcore(const char* host, const int port, const char *request);

//�����źŴ�����
void alarm_handler(int signal);

//�÷��͸���������ϸ����
void usage(void);

//host:ip��ַ����������		clientPort:�˿�
int Socket(const char *host, int clientPort);

#endif