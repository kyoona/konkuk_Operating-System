#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<signal.h>
#include<sys/time.h>
#include<time.h>
#include<stddef.h>
#include<string.h>
struct PCB;
void pushQueue(struct PCB pcb, int priority);
struct PCB popQueue(int priority);
void mlfq();

struct PCB{
	pid_t pid;
	int timeAllotment;
	int priority;
	int isNull;
};
typedef struct NODE{
	struct NODE *next;
	struct PCB pcb;
}Node;

//각 priority에 따른 ready queue의 시작 node
Node *q3=NULL;
Node *q2=NULL;
Node *q1=NULL;
//각 priority에 따른 ready queue의 끝 node
Node *q3end=NULL;
Node *q2end=NULL;
Node *q1end=NULL;

//running process의 PCB
struct PCB runningPCB;

//시간경과를 check할 변수들
int exitTime; //user가 입력한 Main process가 실행될 시간
int totalTime=0; //총 경과시간
int boostTime=0; //priority boost를 위한 시간 경과값(10초가 될때마다 0으로 초기화)

int main(int argc, char* argv[])
{
	//시작셋팅
	int pcNum=(*argv[1]-'0');
	if(pcNum<=0&&pcNum>26){
		return -1;
	}
	exitTime=atoi(argv[2]);
	char *charg[]={"A","B","C","D","F","G","H","I","J","K","L","N","M","O","P","Q","R","S","T","U","V","W","X","Y","Z"};
	int charNum=0;
	pid_t pid;
	for(int i=0;i<pcNum;i++){
		pid=fork();
		if(pid<0){
			return -1;
		}
		else if(pid==0){
			break;
		}
		struct PCB pcb;
		pcb.pid=pid;
		pcb.timeAllotment=0;
		pcb.priority=3;
		pcb.isNull=0;
		pushQueue(pcb,3);
		charNum++;
	}
	int success=0;
	if(pid==0){
		success=execl("./ku_app","./ku_app",charg[charNum],NULL);
	}
	mlfq();
}

void timeSliceHandler()
{
	runningPCB.timeAllotment++;
	//해당 process의 cpu할당 시간이 2초를 넘어갈때 priority 감소
	if(runningPCB.priority>1 && runningPCB.timeAllotment>=2){ 
		runningPCB.priority--;
		runningPCB.timeAllotment=0;
	}

	//context switch를 위한 코드
	kill(runningPCB.pid,SIGSTOP);
	pushQueue(runningPCB,runningPCB.priority);

	struct PCB nextPCB;
	
	if((nextPCB=popQueue(3)).isNull!=1){
		runningPCB=nextPCB;
	}
	else if(runningPCB.priority-1>0 && (nextPCB=popQueue(2)).isNull!=1){
		runningPCB=nextPCB;
	}
	else if(runningPCB.priority-2>0 && (nextPCB=popQueue(1)).isNull!=1){
		runningPCB=nextPCB;
	}
	else{
		return;
	}
	kill(runningPCB.pid,SIGCONT);
}

/*process priority를 boost시킬때 실행*/
void priorityBoostHandler()
{
	struct PCB pcb;
	while((pcb=popQueue(2)).isNull!=1){
		pcb.priority=3;
	}
	while((pcb=popQueue(1)).isNull!=1){
		pcb.priority=3;
	}
}

/*Main process가 종료될때 실행*/
void exitTimeHandler()
{
	//ready queue에 있는 process와 실행중인 process kill해주기
	//ready queue의 동적할당 free시켜주기
	struct PCB pcb;
	while((pcb=popQueue(3)).isNull!=1){
		kill(pcb.pid, SIGKILL);
	}
	while((pcb=popQueue(2)).isNull!=1){
		kill(pcb.pid, SIGKILL);
	}
	while((pcb=popQueue(1)).isNull!=1){
		kill(pcb.pid, SIGKILL);
	}
	kill(runningPCB.pid, SIGKILL);

	//main 함수도 종료시켜주기
	exit(0);
}

/* SIGALRM이 도착했을때 호출되는 handler(1초마다 timer에 의해 호출)*/
void handler(int sigNum)
{
	totalTime++; //총시간경과값 1초 증가
	boostTime++; //boost 경과값 1초 증가
	//총시간경과가 user가 입력한 exitTime이 되었을때 종료
	if(totalTime>=exitTime){
		exitTimeHandler();
	}
	//boost 경과값이 10초가 되었을때 boost시키고 다시 0으로 초기화
	if(boostTime==10){
		boostTime=0;
		priorityBoostHandler();
	}
	//timeslice에 따른 context switch
	timeSliceHandler();
}

/*Timer 생성하고 handler부착*/
void mlfq()
{
	sleep(5);
	signal(SIGALRM,handler);

	struct itimerval timeSlice;
	timeSlice.it_value.tv_usec=0;
	timeSlice.it_value.tv_sec=1;
	timeSlice.it_interval.tv_usec=0;
	timeSlice.it_interval.tv_sec=1;
	setitimer(ITIMER_REAL, &timeSlice, NULL);

	runningPCB=popQueue(3);
	kill(runningPCB.pid,SIGCONT);
	while(1){}
}

/*ready queue에 PCB추가*/
void pushQueue(struct PCB pcb, int priority)
{
	Node *newNode=(Node*)malloc(sizeof(Node));
	newNode->pcb=pcb;
	newNode->next=NULL;

	Node **front=NULL;
	Node **end=NULL;
	if(priority==3){
		front=&q3;
		end=&q3end;
	}
	else if(priority==2){
		front=&q2;
		end=&q2end;
	}
	else if(priority==1){
		front=&q1;
		end=&q1end;
	}

	if(*front==NULL&& *end==NULL){
		*front=newNode;
		*end=newNode;
		return;
	}
	(*end)->next=newNode;
	*end=newNode;
}

/*ready queue에서 pop */
struct PCB popQueue(int priority)
{
	struct PCB re;
	re.isNull=1;
	Node **front=NULL;
	Node **end=NULL;
	if(priority==3){
		front=&q3;
		end=&q3end;
	}
	else if(priority==2){
		front=&q2;
		end=&q2end;
	}
	else if(priority==1){
		front=&q1;
		end=&q1end;
	}

	if(*front==NULL){
		return re;
	}
	Node *pop=*front;
	*front=(*front)->next;
	re=pop->pcb;
	free(pop);

	return re;
}