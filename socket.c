#include "socket.h"

//����http��������request����
void build_request(const char *url)
{
	//��Ŷ˿ںŵ��м�����
	char tmp[10];
	//���url����������ʼ��λ��
	int i;

	memset(host, 0, MAXHOSTNAMELEN);
	memset(request, 0, REQUEST_SIZE);

	//�ж�Ӧ��ʹ�õ�httpЭ��

	//1.����ʹ����Ƕ���http/1.0�Ժ���е���
	if (force_reload && proxyhost != NULL && http10<1) 
		http10 = 1;

	//2.head������http/1.0����е�
	if (method == METHOD_HEAD && http10<1) 
		http10 = 1;
	//3.options�����trace������http/1.1����
	if (method == METHOD_OPTIONS && http10<2) 
		http10 = 2;
	if (method == METHOD_TRACE && http10<2) 
		http10 = 2;

	//��ʼ��дhttp����
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

	//�ж�url�ĺϷ���

	if (NULL == strstr(url, "://"))
	{
		fprintf(stderr, "\n%s: is not a valid URL.\n", url);
		exit(2);//url���Ϸ�
	}
	
	if (strlen(url)>1500)
	{
		fprintf(stderr, "URL is too long.\n");
		exit(2);//url����
	}

	//������ĸ��Сд�Ƚ�ǰ7λ
	if (0 != strncasecmp("http://", url, 7))
	{
		fprintf(stderr, "\nOnly HTTP protocol is directly supported, set --proxy for others.\n");
		exit(2);
	}

	//�ҵ���������ʼ�ĵط�
	i = strstr(url, "://") - url + 3;

	//����������ʼ�ĵط���ʼ�����ң�û�� '/' ��url�Ƿ�
	if (strchr(url + i, '/') == NULL) {
		fprintf(stderr, "\nInvalid URL syntax - hostname don't ends with '/'.\n");
		exit(2);
	}

	//�޴��������
	if (proxyhost == NULL)
	{
		//���ڶ˿ں�
		if (index(url + i, ':') != NULL && index(url + i, ':')<index(url + i, '/'))
		{
			//�����������host�ַ�����
			strncpy(host, url + i, strchr(url + i, ':') - url - i);

			//��ʼ����Ŷ˿ںŵ��м�����
			memset(tmp, 0, 10);

			//�и�õ��˿ں�
			strncpy(tmp, index(url + i, ':') + 1, strchr(url + i, '/') - index(url + i, ':') - 1);

			//���ö˿ں� atoi���ַ���ת����
			proxyport = atoi(tmp);

			//����д��';'ȴû��д�˿ںţ����������Ĭ�����ö˿ں�Ϊ80
			if (proxyport == 0)
				proxyport = 80;
		}
		else	//�����ڶ˿ں�
		{
			//�����������host�ַ����飬����www.baidu.com
			strncpy(host, url + i, strcspn(url + i, "/"));
		}

		//�����������Լ����ܴ��ڵĶ˿ں��Լ�����·����䵽��������
		//����urlΪhttp://www.baidu.com:80/one.jpg/
		//���ǽ�www.baidu.com:80/one.jpg��䵽��������
		strcat(request + strlen(request), url + i + strcspn(url + i, "/"));
	}
	else    //���ڴ��������
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

	//�����ڴ����������httpЭ��汾Ϊ1.0��1.1�����Host�ֶ�
	//�����ڴ������������httpЭ��汾Ϊ0.9ʱ������Ҫ���Host�ֶ�
	//��Ϊhttp0.9�汾û��Host�ֶΣ����������������ҪHost�ֶ�
	if (proxyhost == NULL && http10>0)
	{
		strcat(request, "Host: ");
		strcat(request, host);//Host�ֶ�����������������IP
		strcat(request, "\r\n");
	}

	/*pragma��http/1.1֮ǰ�汾����ʷ�������⣬����Ϊ��http�������ݶ�����
	�淶�����Ψһ��ʽ��
	Pragma:no-cache
	��ѡ��ǿ�����¼��أ���ѡ���޻���
	*/
	if (force_reload && proxyhost != NULL)
	{
		strcat(request, "Pragma: no-cache\r\n");
	}

	/*���ǵ�Ŀ���ǹ����������վ������Ҫ�����κ����ݣ����Բ����ó�����
	http/1.1Ĭ��Keep-alive(�����ӣ�
	������Ҫ��http�汾Ϊhttp/1.1ʱҪ�ֶ�����Ϊ Connection: close
	*/
	if (http10>1)
		strcat(request, "Connection: close\r\n");

	//��ĩβ�������
	if (http10>0) strcat(request, "\r\n");

	printf("\nRequest:\n%s\n", request);
}

int bench(void)
{
	int i, j, k;
	pid_t pid = 0;//���̺Ŷ���(int)
	FILE *f;

	//�ȼ��һ��Ŀ��������ǿ�����
	i = Socket(proxyhost == NULL ? host : proxyhost, proxyport);

	//Ŀ�������������,�˳�
	if (i<0) {
		fprintf(stderr, "\nConnect to server failed. Aborting benchmark.\n");
		return 1;
	}

	//��������������ӳɹ��ˣ��رո�����
	close(i);

	//�������ӽ���ͨ�ŵĹܵ�
	if (pipe(mypipe))
	{
		perror("pipe failed.");
		return 3;
	}

	
	//��������Ϊclients���ӽ��̽���ѹ������
	for (i = 0; i<clients; i++)
	{
		pid = fork();
		if (pid <= (pid_t)0)
		{
			/* child process or error*/
			sleep(1); //������ӽ��̹���1�룬��cpuʱ�佻���������Կ���fork�ӽ���
			break;	//��ֹ�ӽ��̼���fork
		}
	}

	//pid<0��ʾfork�ӽ��̳���
	if (pid < (pid_t)0)
	{
		fprintf(stderr, "problems forking worker no. %d\n", i);
		perror("fork failed.");
		return 3;
	}

	//pid=0��ʾ��ǰ�������ӽ���
	if (pid == (pid_t)0)
	{
		//���ӽ��̷���http������
		if (proxyhost == NULL)
			benchcore(host, proxyport, request);
		else
			benchcore(proxyhost, proxyport, request);

		//�ӽ��̻�ùܵ�д�˵��ļ�ָ�룬׼�����йܵ�д
		f = fdopen(mypipe[1], "w");

		//�ܵ�д�˴�ʧ��
		if (f == NULL)
		{
			perror("open pipe for writing failed.");
			return 3;
		}

		//��ܵ���д��ú��ӽ�����һ��ʱ��������ɹ��Ĵ�����ʧ�ܴ�������ȡ���������ظ������ֽ���
		fprintf(f, "%d %d %d\n", speed, failed, bytes);
		fclose(f);

		return 0;
	}
	//���Ǹ�����
	else
	{
		//�����̻�ùܵ����˵��ļ�ָ��
		f = fdopen(mypipe[0], "r");

		//�ܵ����˴�ʧ��
		if (f == NULL)
		{
			perror("open pipe for reading failed.");
			return 3;
		}

		/*
		fopen��׼IO�������Դ���������
		������������ݷǳ��̣���������Ҫ��ʱ
		����û�л���������ʵ�
		���ǲ���Ҫ������
		��˰ѻ�����������Ϊ_IONBF*/
		setvbuf(f, NULL, _IONBF, 0);

		speed = 0;  //http���������ӳɹ�����
		failed = 0; //http������ʧ�ܵ��������
		bytes = 0;  //�������ظ������ֽ���

		//�����̲�ͣ�Ķ�
		while (1)
		{ 
			//��������Լ��õ��ɹ��õ��Ĳ����ĸ���
			pid = fscanf(f, "%d %d %d", &i, &j, &k);
			if (pid<3)
			{
				fprintf(stderr, "Some of our childrens died.\n");
				break;
			}

			speed += i;
			failed += j;
			bytes += k;

			//��¼�Ѿ����˶��ٸ��ӽ��̵����ݣ������˳�
			if (--clients == 0)
				break;
		}

		//�رն���
		fclose(f);

		//ͳ�ƴ�����
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
	char buf[1500];//��¼��������Ӧ���󷵻ص�����
	int s, i;
	struct sigaction sa;//�źŴ���������

	//����alarm_handler����Ϊ�����źŴ�����
	sa.sa_handler = alarm_handler;
	sa.sa_flags = 0;

	//��ʱ������ź�SIGALRM����sa��ָ����������
	if (sigaction(SIGALRM, &sa, NULL))
		exit(3);

	alarm(benchtime); //�����źŴ�������ʼ��ʱ����ʱ��timerexpired��1

	rlen = strlen(req);//�õ������ĵĳ���
	while (1)
	{
			ischoked = 0;
			if (timerexpired)//��ʱ
			{
				if (failed>0)
				{
					failed--;
				}
				return;
			}

			//������Ŀ����վ��tcp����,����http����
			s = Socket(host, port);

			//����ʧ��
			if (s<0) 
			{
				failed++; 
				continue; 
			}
			//����������

			//ʵ��д����ֽ������������ֽ�������ͬ��дʧ�ܣ�����1ʧ�ܴ���+1
			if (rlen != write(s, req, rlen)) 
			{
				failed++; 
				close(s); 
				continue; 
			}

			//http/0.9�����⴦��
			/*
			*��Ϊhttp/0.9���ڷ������ظ����Զ��Ͽ�����
			*�ڴ˿�����ǰ�ȳ��׹ر��׽��ֵ�д��һ�룬���ʧ�����ǿ϶��Ǹ���������״̬
			*��ʵ�ϣ��ر�д�󣬷�����û��д������Ҳ������д�ˣ�����Ͳ�������
			*����رճɹ������������Ϊ���ܻ���Ҫ���շ������ظ�������
			*�����дһ���ǿ��Թرյģ���Ϊ�ͻ���Ҳ����Ҫд��ֻ��Ҫ��
			*��ˣ����������ƻ��׽��ֵ�д�����ⲻ�ǹر��׽��֣��رջ��ǵ���close
			*/
			if (http10 == 0 && shutdown(s, 1))
			{ 
				failed++; 
				close(s); 
				continue; 
			}

			//��Ҫ�ȴ��������ظ�
			if (force == 0)
			{
				//���׽��ֶ�ȡ���з������ظ�������
				while (1)
				{
					//��ʱ��־Ϊ1ʱ�����ٶ�ȡ�������ظ�������
					if (timerexpired)
						break;

					//��ȡ�׽�����1500���ֽ����ݵ�buf�����У�����׽���������С��Ҫ��ȡ���ֽ���1500���������� ����-1
					i = read(s, buf, 1500);
					//read����ֵ��

					//δ��ȡ�κ�����   ����   0
					//��ȡ�ɹ�         ����   �Ѿ���ȡ���ֽ���
					//����             ����   -1

					//��ȡ������
					if (i<0)
					{
						failed++;
						close(s);
						ischoked = 1;
						break;
					}
					else if (i == 0)	
						break;//û�ж�ȡ���κ��ֽ���
					else
						bytes += i;//�ӷ�������ȡ�������ֽ�������
				}
			}

			/*
			close���ط���ֵ
			�ɹ�   ���� 0
			ʧ��   ���� -1
			*/

			if (!ischoked)
			{
				if (close(s))//�׽��ֹر�ʧ��
				{
					failed++;
					continue;
				}
				//�׽��ֹرճɹ�,��������Ӧ���ӽ�������+1
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

sockaddr_in������

#include <netinet/in.h>��#include <arpa/inet.h>�����

struct sockaddr
{
__SOCKADDR_COMMON (sa_);  //Э����

char sa_data[14];         //��ַ+�˿ں�
};

sockaddrȱ�ݣ���Ŀ���ַ�Ͷ˿ںŻ���һ����
��sockaddr_in�ͽ������һȱ��
���˿ںź�IP��ַ�ֿ��洢

struct sockaddr_in
{
sa_family_t sin_family;     //��ַ��

uint16_t sin_port;          //16λTCP/UDP�˿ں�

struct in_addr sin_addr;    //32λIP��ַ

char sin_zero[8];           //��ʹ�ã�ֻΪ���ڴ����
};

*/

/*

hostent������
host entry����д
��¼������Ϣ��������������������ַ���ͣ���ַ���Ⱥ͵�ַ�б�

struct hostent
{

char *h_name;         //��ʽ������

char **h_aliases;     //��������

int h_addrtype;       //����IP��ַ���ͣ�IPV4-AF_INET

int h_length;		  //����IP��ַ�ֽڳ��ȣ�����IPv4�����ֽڣ���32λ

char **h_addr_list;	  //������IP��ַ�б�

};
#define h_addr h_addr_list[0]   //�������IP��ַ

�����ĵĵ�ַ���б���ʽ��ԭ��
��һ�������ֶ������ӿ�ʱ����Ȼ�ж����ַ

*/

//host:ip��ַ����������		clientPort:�˿�
int Socket(const char *host, int clientPort)
{
    int sock;
    unsigned long inaddr;
    struct sockaddr_in ad;//��ַ��Ϣ
    struct hostent *hp;//������Ϣ

	//��Ϊhost������ip��ַ���������������Ե�hostΪ��������ʱ����Ҫͨ���������õ�IP��ַ

    memset(&ad, 0, sizeof(ad));
    ad.sin_family = AF_INET;//����TCP/IPЭ����
	
    inaddr = inet_addr(host);//���ʮ����IPת��Ϊ������IP

	//����ΪIP��ַ
    if (inaddr != INADDR_NONE)
        memcpy(&ad.sin_addr, &inaddr, sizeof(inaddr));//��IP��ַ���Ƹ�ad��sin_addr����
	//���벻��IP��ַ����������
    else
    {
        hp = gethostbyname(host);//ͨ���������õ�������Ϣ
        if (hp == NULL)//��ȡ������Ϣʧ��
            return -1;
        memcpy(&ad.sin_addr, hp->h_addr, hp->h_length);//��IP��ַ���Ƹ�ad��sin_addr����
    }

	/*
	���˿ںŴ������ֽ�˳���������ֽ�˳��
	���������ڵ�ַ�ռ�洢��ʽ��Ϊ���ֽڴ�����ڴ���ֽڴ�

	�����ֽ�˳����TCP/IP�й涨�õ�һ�����ݱ�ʾ��ʽ����CPU�Ͳ���ϵͳ�޹�
	�Ӷ����Ա�֤�����ڲ�ͬ����֮�䴫��ʱ�ܹ�����ȷ����
	�����ֽ�˳����ô�β˳�򣺸��ֽڴ洢���ڴ���ֽڴ�
	*/
    ad.sin_port = htons(clientPort);

	/*
	AF_INET:     IPV4����Э��
	SOCK_STRAM:  �ṩ�������ӵ��ȶ����ݴ��䣬��TCPЭ��
	*/
	//����һ������IPV4��TCP��socket
    sock = socket(AF_INET, SOCK_STREAM, 0);

	//����socketʧ��
    if (sock < 0)
        return sock;

	//�������� ����ʧ�ܷ���-1
    if (connect(sock, (struct sockaddr *)&ad, sizeof(ad)) < 0)
        return -1;

	//�����ɹ�������socket
    return sock;
}
