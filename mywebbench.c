#include "socket.h"

//Ĭ�ϲ�������
int http10 = 1;				//Ĭ��HTTP�汾Ϊhttp/1.0 ��0 - http/0.9, 1 - http/1.0, 2 - http/1.1��
int method = METHOD_GET;	//Ĭ�����󷽷�ΪGET
int clients = 1;			//Ĭ��ֻģ��һ���ͻ���
int force = 0;				//Ĭ����Ҫ�ȴ���������Ӧ
int force_reload = 0;		//ʧ��ʱ��������
int proxyport = 80;			//Ĭ�Ϸ��ʷ������˿�Ϊ80
char *proxyhost = NULL;		//Ĭ���޴��������
int benchtime = 10;			//Ĭ������ʱ��Ϊ10s

/*
volatile:
�������η�����Ϊָ��ؼ��֣�
ȷ����ָ�����Ϊ�������Ż���ʡ��
��ÿ��Ҫ�����¶�ֵ��
���������õ����������ʱ�򶼱���С�ĵ����¶�ȡ���������ֵ��
������ʹ�ñ����ڼĴ�����ı��ݣ���֤ÿ�ζ����Ķ������µ�
*/
volatile int timerexpired = 0;

//���Խ��
int speed = 0;  //�ɹ��õ���������Ӧ���ӽ�������
int failed = 0; //û�гɹ��õ���������Ӧ���ӽ�������
int bytes = 0;  //�����ӽ��̶�ȡ���������ظ������ֽ���

// �ڲ� 
int mypipe[2];                //�ܵ����ڸ��ӽ���ͨ��
char host[MAXHOSTNAMELEN];    //�洢�����������ַ
char request[REQUEST_SIZE];   //���http��������Ϣ����

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
	//argc��ʾ��������
	//argv[0]��ʾ�������е�·���ͳ�����
	//argv[1]ָ���1������
	//argv[n]ָ���n������

    int opt=0;
    int options_index=0;
    char *tmp=NULL;

	//1.������û���������
    if(argc==1)
    {
        usage();
        return 2;
    } 

	//�����������������һ��������
	//"GHOT912Vfrt:c:p:?h"��һ���ַ������һ��ð�Ŵ������������һ������
	//����t,p,c������涼Ҫ��һ������
	//��������ð�����ʾ�������п���
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
			case 'f': force = 1; break;		//���ȴ���������Ӧ
			case 'r': force_reload = 1; break;	//�����������(�޻���)
			case 't': benchtime = atoi(optarg); break;
			case 'c': clients = atoi(optarg); break;
            case 'p':	//ʹ�ô�������������������������źͶ˿ںţ���ʽ��-p server:port
						//server:port��һ�����������������ַ��������ɷ�������ַ�Ͷ˿���������
            tmp=strrchr(optarg,':');	//��optagr���ҵ�':'�����ֵ�λ��
            proxyhost=optarg;
            if(tmp==NULL)	//û�ж˿ں�
            {
                break;
            }
            if(tmp==optarg)	//�˿ں���optarg�ͷ��˵��ȱʧ������ַ
            {
                fprintf(stderr,"Error in option --proxy %s: Missing hostname.\n",optarg);
                return 2;
            }
            if(tmp==optarg+strlen(optarg)-1)	//':'����ĩβ��˵��ȱʧ�˿ں�
            {
                fprintf(stderr,"Error in option --proxy %s Port number is missing.\n",optarg);
                return 2;
            }
            *tmp='\0';	//��optarg��':'��ʼ�ضϣ�ǰ������������������Ƕ˿ں�
            proxyport=atoi(tmp+1);break;	//���ô���������˿ں�
            case ':':
            case 'h':
            case '?': usage(); return 2; break;
			default : usage(); return 2; break;
        }
    }

	//��������������֮�󣬸պ��Ƕ���URL����ʱargv[optind]ָ��URL
	//URL����Ϊ��
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

	//����������
	build_request(argv[optind]);//����ΪURL

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
	*���в����٣��⺯����Ĭ���л��壬�ӽ��̻Ḵ������������
	*��������ˢ�»�����,�ӽ��̻�ѻ�������Ҳ�����
	*�����к󻺳�����ˢ����
	*�ӽ��̵ı�׼�⺯�����ǿ黺�����Ͳ�����ǰ����Щ��
	*/
    printf(".\n");

	//��ʼѹ������
    return bench();
}
