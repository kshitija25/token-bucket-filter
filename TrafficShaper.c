/*
 * Description : The file implements TrafficShaper (Token Bucket Filter)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include "util.h"
#include "list.h"

/*==================== DEFINES ====================*/
#define MAX_LENGTH 100
#define DETERMINISTIC 10
#define TRACE_DRIVEN 11

/*==================== Globals ====================*/
double Parameter[6];
char *filename;
FILE *file;
int num;
int mode = DETERMINISTIC;

int tokens_added;
int tokens_dropped;
int Bucket_Tokens;
int sigint;

struct timeval tv;
List Q1, Q2;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t Q2Q = PTHREAD_COND_INITIALIZER;
pthread_t Packet_arrival_thread, Token_Arrival_thread, Server_thread, SigInt_interrupt_thread;

int  count_processed;
int  Packets_dropped;
int  Packets_arrived;
int  Packets_serviced;
int  signal_waiting;

unsigned long  Reftime;
double   average_interarrival_time;
double   average_service_time;
unsigned long long total_time_in_Q1;
unsigned long long total_time_in_Q2;
unsigned long long total_service_time;
double   average_time_in_system;
double   average_of_the_square_time_in_system;

/* --- SIGINT handling (portable) --- */
#ifdef __MINGW32__
volatile sig_atomic_t SigInt = 0;   /* handle Ctrl+C via signal() on Windows */
#else
sigset_t new;                       /* POSIX: block + sigwait in a thread */
int SigInt;
#endif

/*==================== Types ====================*/
typedef struct tagMyPacketList {
    int Packet_num;
    int InterArrivalTime;
    int Tokens;
    int ServiceTime;
    unsigned long MeasArrivalTime;
    unsigned long EntryTimeQ1;
    unsigned long EntryTimeQ2;
} MyPacketList;

typedef struct tagMyParseData {
    int InterArrivalTime;
    int Tokens;
    int ServiceTime;
} MyParsedata;

/*==================== Decls ====================*/
void StopEmulation(void);
void interrupt(void);
void* Packet_Arrival(void *unused);
void* Token_Arrival(void *unused);
void* Server(void *unused);
void* SigInt_interrupt(void *unused);

#ifdef __MINGW32__
void SigIntHandler(int signum)
{
    (void)signum;
    SigInt = 1;
    printf("\n");
    interrupt();
}
#endif

/*==================== Helpers ====================*/
void CallErr(int errn, void* arg)
{
    switch(errn)
    {
        case 0 :   fprintf(stderr, "Error: (malformed command). Usage : warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n"); exit(0); break;
        case 1:    fprintf(stderr, "Error : No such file or directory or file cannot be accessed.\n"); exit(0); break;
        case 2:    fprintf(stderr, "(malformed command) invalid parameter value %s in command\n", (char*)arg); exit(0); break;
        case 3:    fprintf(stderr, "(input file %s does not exist)\n", (char*)arg); exit(0); break;
        case 4:    fprintf(stderr, "(input file %s cannot be opened - access denies)\n", (char*)arg); exit(0); break;
        case 5:    fprintf(stderr, "(input file %s is a directory)\n", (char*)arg); exit(0); break;
        case 6:    fprintf(stderr, "(input file is not in the right format) Error Line: %d\n", *((int*)arg)); interrupt(); exit(0); break;
        case 7:    fprintf(stderr, "Error : Additional packets greater than n=%d present in testfile\n", *((int*)arg)); interrupt(); exit(0); break;
        case 8:    fprintf(stderr, "Error : The input for the %s command option should be a positive integer\n", (char*)arg); interrupt(); exit(0); break;
        case 9:    fprintf(stderr, "Error : The input for the %s command option should be positive\n", (char*)arg); interrupt(); exit(0); break;
    }
}

void CheckAndGetValue(char* argv, double *var )
{
    if (!argv) { CallErr(0,0); }
    else {
        int count = (int)strlen(argv), i = 0;
        char s[20];
        strncpy(s, argv, count);
        while(count) {
            if ((int)s[i]==45) CallErr(0,0);
            else if (!((((int)s[i]>=48)&&((int)s[i]<=57)) || ((int)s[i]==46))) CallErr(2,(void*)argv);
            i++; count--;
        }
        *var = atof(argv);
    }
}

void CheckAndGetData(char* buf, int line_number, int* data)
{
    int count = (int)strlen(buf), i = 0;
    char s[1];
    while(count) {
        strncpy(s, &buf[i], 1);
        if (!((((int)s[0]>=48)&&((int)s[0]<=57)) || ((int)s[0]==43))) CallErr(6, (void*)&line_number);
        i++; count--;
    }
    *data = atoi(buf);
}

MyParsedata* ParseTraceFile()
{
    char  buf[MAX_LENGTH];
    char * start_p = NULL;
    int len = 0;
    char  *next_p = NULL;
    static int line_number = 0;
    static int flag = 0;
    MyParsedata *Parsed_data = (MyParsedata *)malloc(sizeof(MyParsedata));

    if (fgets(buf, sizeof(buf), file) != NULL)
    {
        line_number++;
        start_p = strtok(buf, "\n");
        start_p = strcat(buf, "\0");
        next_p = start_p;
        len = (int)strlen(start_p);
        int *ptr = &Parsed_data->InterArrivalTime;

        if (next_p[0] == '\t' || next_p[0] == ' ' || next_p[len-1] == '\t' || next_p[len-1] == ' ')
            CallErr(6, (void*)&line_number);

        if (flag == 0)
        {
            CheckAndGetData(start_p, line_number, &num);
            flag=1;
            free(Parsed_data);
            return NULL;
        }

        while(*next_p != '\0')
        {
            if (*next_p == '\t' || *next_p == ' ')
            {
                *next_p++ = '\0';
                CheckAndGetData(start_p, line_number, ptr);
                ptr++;
                while(*next_p == '\t' || *next_p == ' ')
                    next_p++;
                start_p = next_p;
            }
            next_p++;

            if (line_number > num+1) CallErr(7, (void*)&num);
        }
        CheckAndGetData(start_p, line_number, ptr);
    }
    return Parsed_data;
}

void CheckCommandAndGetMode(int argc, char** argv)
{
    char* s[8] = {"-lambda","-mu","-r","-B","-P","-n","-t", 0};
    double Default_Params[6] = {0.5, 0.35, 1.5, 10, 3, 20};
    int j=1,k=0,flag=0;
    if(argc < 1) CallErr(0,0);

    if(argc > 1)
    {
        while(argv[j])
        {
            while(s[k])
            {
                if(!strcmp((const char*)argv[j], (const char*)s[k++]))
                {
                    if (k==7) { mode = TRACE_DRIVEN; filename = argv[++j]; }
                    else CheckAndGetValue(argv[++j], &Parameter[k-1]);
                    flag = 1; break;
                }
            }
            if (mode == TRACE_DRIVEN)
            {
                struct stat statbuf;
                file = fopen(filename, "r");
                if(file == NULL) {
                    if(errno == ENOENT) CallErr(3,(void *)filename);
                    else if(errno == EACCES) CallErr(4,(void *)filename);
                }
                stat((const char*) filename, &statbuf);
                if(S_ISDIR(statbuf.st_mode)) CallErr(5,(void *)filename);
            }
            if(!flag && argv[j+1]) {
                k=0; while(s[k]) { if(!strcmp((const char*)argv[j+1], (const char*)s[k++])) CallErr(0,0); }
            } else if(!flag && !argv[j+1]) {
                CallErr(0,0);
            }
            j++; k=0; flag=0;
        }
    }

    for(k=0;k<6;k++)
    {
        if (k>=3 && k <= 5 && (Parameter[k] != (double)((int)Parameter[k]))) CallErr(8,(void*)s[k]);
        if(!Parameter[k]) Parameter[k] = Default_Params[k];
    }
}

double get_time(unsigned long value) { return ((double)value/1000); }

/*==================== Threads ====================*/
void* Packet_Arrival(void *unused) { (void)unused;
    int Tokens =0, Packet_num = 0, count = 0, i = 0;
    long InterArrivalTime=0;
    unsigned long  difftime=0, x=0, EntryTime_Q1=0, EntryTime_Q2=0;
    unsigned long  t1=0, t2=0;
    unsigned long  difftime_ref = Reftime;
    MyParsedata *Parsed_data = NULL;
    Packets_arrived = 0;
    Packets_dropped = 0;

    gettimeofday(&tv,0);
    t1 = (tv.tv_sec *1000000) + (unsigned long)tv.tv_usec;

    while(count < num)
    {
        pthread_testcancel();

        if  (mode == DETERMINISTIC) {
            x = (double)(1/Parameter[0])*1000000;
            if(x>10000000) InterArrivalTime = 10000000;
            else InterArrivalTime = round(x);
            Tokens = (int)Parameter[4];
        } else {
            Parsed_data = ParseTraceFile();
            Tokens = Parsed_data->Tokens;
            InterArrivalTime = Parsed_data->InterArrivalTime*1000;
        }

        gettimeofday(&tv,0);
        t2= (tv.tv_sec *1000000) + (unsigned long)tv.tv_usec - t1;
        if((int)(InterArrivalTime-t2)>0) usleep(InterArrivalTime-t2);

        Packet_num++;

        gettimeofday(&tv,0);
        t1= (tv.tv_sec) *1000000 + (unsigned long)tv.tv_usec;
        difftime = t1 - difftime_ref;
        difftime_ref= difftime_ref + difftime;

        if (Tokens > Parameter[3])
        {
            printf("%012.03fms: p%d arrives, needs %d tokens, inter-arrival time = %.3fms, dropped\n",
                   get_time(t1 - Reftime), Packet_num, Tokens, get_time(difftime));
            count++; count_processed++; Packets_dropped++; Packets_arrived++;
            average_interarrival_time =  ((average_interarrival_time*i)+ get_time(difftime))/(i+1);
            i++; continue;
        } else {
            printf("%012.03fms: p%d arrives, needs %d tokens, inter-arrival time = %.3fms\n",
                   get_time(t1-Reftime), Packet_num, Tokens, get_time(difftime));
        }

        Packets_arrived++;
        average_interarrival_time =  ((average_interarrival_time*i)+ get_time(difftime))/(i+1);
        i++;

        MyPacketList *Packet_p = (MyPacketList*)malloc(sizeof(MyPacketList));
        Packet_p->Packet_num = Packet_num;
        Packet_p->MeasArrivalTime = t1;

        if  (mode == DETERMINISTIC) {
            Packet_p->InterArrivalTime = InterArrivalTime;
            Packet_p->Tokens = Tokens;
            x = (1/Parameter[1])*1000000;
            if(x>10000000) Packet_p->ServiceTime = 10000000;
            else Packet_p->ServiceTime = round(x);
        } else {
            Packet_p->InterArrivalTime = InterArrivalTime;
            Packet_p->Tokens = Tokens;
            Packet_p->ServiceTime = Parsed_data->ServiceTime*1000;
        }

        pthread_mutex_lock(&mutex);

        gettimeofday(&tv,0);
        unsigned long EntryTime_Q1 = tv.tv_sec * 1000000 + tv.tv_usec ;
        Packet_p->EntryTimeQ1 = EntryTime_Q1;

        ListAppend(&Q1, (void*)Packet_p);
        printf("%012.03fms: p%d enters Q1\n", get_time(EntryTime_Q1- Reftime), Packet_p->Packet_num);

        ListNode *Packet =  ListFirst(&Q1);

        if(((MyPacketList*)Packet->obj)->Tokens <= Bucket_Tokens)
        {
            Bucket_Tokens -= ((MyPacketList*)Packet->obj)->Tokens;
            MyPacketList *Packet_t = (MyPacketList*)Packet->obj;

            gettimeofday(&tv,0);
            ListUnlink(&Q1, Packet);
            t2 = tv.tv_sec *1000000 + tv.tv_usec ;
            difftime = t2 - Packet_t->EntryTimeQ1;
            printf("%012.03fms: p%d leaves Q1, time in Q1 = %.3fms, token bucket now has %d token\n",
                   get_time(t2-Reftime), Packet_t->Packet_num, get_time(difftime), Bucket_Tokens );

            total_time_in_Q1 += difftime;

            gettimeofday(&tv,0);
            unsigned long EntryTime_Q2 = tv.tv_sec * 1000000 + tv.tv_usec ;
            Packet_t->EntryTimeQ2 = EntryTime_Q2;
            ListAppend(&Q2,  (void*)Packet_t);
            printf("%012.03fms: p%d enters Q2\n", get_time(EntryTime_Q2-Reftime), Packet_t->Packet_num);
            count_processed++;

            if (ListLength(&Q2)) pthread_cond_broadcast(&Q2Q);
        }
        pthread_mutex_unlock(&mutex);

        count++;
        free(Parsed_data);
    }

    pthread_mutex_lock(&mutex);
    if (ListEmpty(&Q2) &&  ListEmpty(&Q1)) signal_waiting=1;
    pthread_mutex_unlock(&mutex);

    return (void*)1;
}

void* Token_Arrival(void *unused) { (void)unused;
    int i=1;
    unsigned long r=0,t1=0,t2=0;
    unsigned long time_in_mill=0,difftime=0;
    double x = 1/Parameter[2] *1000000;

    tokens_added = 0;
    tokens_dropped = 0;

    if(x> 10000000) r = 10000000; else r = round(x);
    t1= (tv.tv_sec *1000000) + (unsigned long)tv.tv_usec;

    while(count_processed < num)
    {
        pthread_testcancel();

        gettimeofday(&tv,0);
        t2= (tv.tv_sec *1000000) + (unsigned long)tv.tv_usec - t1;
        if((int)(r-t2) > 0) usleep(r-t2);

        gettimeofday(&tv,0);
        t1= (tv.tv_sec *1000000) + (unsigned long)tv.tv_usec;

        pthread_mutex_lock(&mutex);
        if(Bucket_Tokens < (int)Parameter[3])
        {
            Bucket_Tokens += 1;
            tokens_added++;

            gettimeofday(&tv,0);
            time_in_mill = (tv.tv_sec) * 1000000 + tv.tv_usec;
            difftime = time_in_mill - Reftime;
            printf("%012.03fms: token t%d arrives, token bucket now has %d token\n",
                   get_time(difftime), i++, Bucket_Tokens);
        }
        else
        {
            tokens_dropped++;
            gettimeofday(&tv,0);
            time_in_mill = tv.tv_sec * 1000000 + tv.tv_usec;
            difftime = time_in_mill - Reftime;
            printf("%012.03fms: token t%d arrives, dropped\n", get_time(difftime), i++);
        }

        if(!ListEmpty(&Q1))
        {
            ListNode *Packet =  ListFirst(&Q1);

            if(((MyPacketList*)Packet->obj)->Tokens <= Bucket_Tokens)
            {
                Bucket_Tokens -= ((MyPacketList*)Packet->obj)->Tokens;

                MyPacketList *Packet_t = (MyPacketList*)Packet->obj;

                gettimeofday(&tv,0);
                ListUnlink(&Q1, Packet);
                time_in_mill = tv.tv_sec *1000000 + tv.tv_usec ;
                difftime = time_in_mill - Packet_t->EntryTimeQ1;

                printf("%012.03fms: p%d leaves Q1, time in Q1 = %.3fms, token bucket now has %d token\n",
                       get_time(time_in_mill-Reftime), Packet_t->Packet_num, get_time(difftime), Bucket_Tokens );

                total_time_in_Q1 += difftime;

                gettimeofday(&tv,0);
                time_in_mill = tv.tv_sec * 1000000 + tv.tv_usec ;
                Packet_t->EntryTimeQ2 = time_in_mill;
                ListAppend(&Q2,  (void*)Packet_t);
                printf("%012.03fms: p%d enters Q2\n", get_time(time_in_mill-Reftime), Packet_t->Packet_num);
                count_processed++;

                if (ListLength(&Q2)) pthread_cond_broadcast(&Q2Q);
            }
        }
        pthread_mutex_unlock(&mutex);
    }
    return (void*)1;
}

void* Server(void *unused) { (void)unused;
    unsigned long time_in_mill  =0;
    unsigned long difftime =0;
    int i=0;

    while(1)
    {
        pthread_mutex_lock(&mutex);

        if(ListEmpty(&Q2) && ListEmpty(&Q1) && count_processed >= num)
        { pthread_mutex_unlock(&mutex); break; }

        while(ListEmpty(&Q2))
        {
            pthread_cond_wait(&Q2Q,&mutex);
            if(signal_waiting) pthread_exit(0);
        }

        ListNode *Packet =  ListFirst(&Q2);
        MyPacketList *Packet_t = (MyPacketList*)Packet->obj;

        ListUnlink(&Q2,Packet);
        gettimeofday(&tv,0);
        time_in_mill = (tv.tv_sec) * 1000000 + tv.tv_usec;
        difftime = time_in_mill - Packet_t->EntryTimeQ2;
        printf("%012.03fms: p%d leaves Q2, time in Q2 = %.3fms\n",
               get_time(time_in_mill-Reftime), Packet_t->Packet_num, get_time(difftime) );

        total_time_in_Q2+= difftime;
        pthread_mutex_unlock(&mutex);

        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

        gettimeofday(&tv,0);
        unsigned long Start_Service_time_in_mill = (tv.tv_sec) * 1000000 + tv.tv_usec;
        printf("%012.03fms: p%d begins service at S, requesting %.3fms of service\n",
               get_time(Start_Service_time_in_mill-Reftime), Packet_t->Packet_num, get_time(Packet_t->ServiceTime));

        usleep(Packet_t->ServiceTime);
        Packets_serviced++;

        gettimeofday(&tv,0);
        time_in_mill = tv.tv_sec * 1000000 + tv.tv_usec ;
        difftime = time_in_mill - Start_Service_time_in_mill;
        printf("%012.03fms: p%d departs from S, service time = %.3fms, time in system = %.3fms\n",
               get_time(time_in_mill - Reftime), Packet_t->Packet_num, get_time(difftime), get_time(time_in_mill - Packet_t->MeasArrivalTime) );

        average_service_time = ((average_service_time* i)+ get_time(difftime))/(i+1);
        total_service_time += difftime;
        average_of_the_square_time_in_system=
            ((average_of_the_square_time_in_system*i)+
             (get_time(time_in_mill - Packet_t->MeasArrivalTime)*get_time(time_in_mill - Packet_t->MeasArrivalTime)))/(i+1);
        average_time_in_system = ((average_time_in_system*i) + get_time(time_in_mill - Packet_t->MeasArrivalTime ))/(i+1);
        i++;

        free(Packet_t);

        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        pthread_testcancel();
    }
    return (void*)1;
}

void interrupt(void)
{
    unsigned long time_in_mill;
    int num_local;

    pthread_cancel(Packet_arrival_thread);
    pthread_cancel(Token_Arrival_thread);
    pthread_cancel(Server_thread);

    pthread_mutex_lock(&mutex);
    while(!ListEmpty(&Q1))
    {
        ListNode *Packet =  ListFirst(&Q1);
        num_local =  ((MyPacketList*)Packet->obj)->Packet_num;
        ListUnlink(&Q1, Packet);
        gettimeofday(&tv,0);
        time_in_mill = tv.tv_sec * 1000000 + tv.tv_usec ;
        printf("%012.03fms: p%d removed from Q1\n", get_time(time_in_mill - Reftime), num_local);
    }

    while(!ListEmpty(&Q2))
    {
        ListNode *Packet =  ListFirst(&Q2);
        num_local =  ((MyPacketList*)Packet->obj)->Packet_num;
        ListUnlink(&Q2, Packet);
        gettimeofday(&tv,0);
        time_in_mill = tv.tv_sec * 1000000 + tv.tv_usec ;
        printf("%012.03fms: p%d removed from Q2\n", get_time(time_in_mill - Reftime), num_local);
    }
    pthread_mutex_unlock(&mutex);
}

void* SigInt_interrupt(void *unused) { (void)unused;
#ifndef __MINGW32__
    int sig;
    sigwait(&new, &sig);
    SigInt = 1;
    printf("\n");
    interrupt();
#endif
    return (void*)1;
}

/*==================== Stats & Orchestration ====================*/
void StopEmulation(void)
{
    unsigned long emulation_time=0;
    gettimeofday(&tv,0);
    emulation_time = (tv.tv_sec) * 1000000 + tv.tv_usec;
    printf("%012.03fms: emulation ends\n", get_time(emulation_time - Reftime));

    printf("\nStatistics:\n");
    printf("\n    average packet inter-arrival time = %.6g\n", average_interarrival_time/(1000.0));

    if( average_service_time == 0) printf("    average packet service time = N/A   no packet at Server\n");
    else                           printf("    average packet service time = %.6g\n",average_service_time/1000.0);

    printf("\n    average number of packets in Q1 = %.6g \n", (double)total_time_in_Q1/(emulation_time - Reftime));
    printf("    average number of packets in Q2 = %.6g\n",(double)total_time_in_Q2/(emulation_time - Reftime));
    printf("    average number of packets at S = %.6g\n", (double)total_service_time/(emulation_time - Reftime) );

    if(Packets_serviced == 0)
        printf("\n    average time a packet spent in system = N/A  no packet arrived at Server\n");
    printf("\n    average time a packet spent in system = %.6g\n",average_time_in_system/1000.0);

    if(Packets_serviced == 0)
        printf("    standard deviation for time spent in system = N/A  no packet arrived at Server\n" );
    else
        printf("    standard deviation for time spent in system = %.6g\n",
               (double)sqrt((average_of_the_square_time_in_system) -  (average_time_in_system*average_time_in_system))/1000.0);

    if(tokens_added == 0)  printf("\n    token drop probability = N/A  no token arrived at the Token Bucket\n" );
    else                   printf("\n    token drop probability = %.6g\n",(double)((double)tokens_dropped/(tokens_added+tokens_dropped)) );

    if(Packets_arrived == 0) printf("    packet drop probability = N/A  no packet arrived at the System\n" );
    else                     printf("    packet drop probability = %.6g\n", (double)((double)Packets_dropped/Packets_arrived) );

    if(mode == TRACE_DRIVEN) fclose(file);
}

void StartEmulation(void)
{
    ListInit(&Q1);
    ListInit(&Q2);
    Bucket_Tokens = 0;

#ifdef __MINGW32__
    signal(SIGINT, SigIntHandler);
#else
    sigemptyset(&new);
    sigaddset(&new, SIGINT);
    pthread_sigmask(SIG_BLOCK, &new, NULL);
#endif

    printf("\nEmulation Parameters:\n");
    printf("    number to arrive = %d\n",num);
    if(mode == DETERMINISTIC) {
        printf("    lambda = %g\n",Parameter[0]);
        printf("    mu = %g\n",Parameter[1]);
    }
    printf("    r = %g\n", Parameter[2]);
    printf("    B = %d\n",(int)Parameter[3]);
    if(mode == DETERMINISTIC) printf("    P = %d\n", (int)Parameter[4]);
    if(mode == TRACE_DRIVEN)  printf("    tsfile = %s\n",filename);
    printf("\n");

    gettimeofday(&tv,0);
    Reftime = tv.tv_sec * 1000000 + tv.tv_usec ;
    printf("00000000.000ms: emulation begins\n");

    void *result_Packet_arrival, *result_Token_Arrival, *result_Server;
#ifndef __MINGW32__
    void *result_SigInt_interrupt;
#endif

    pthread_create(&Packet_arrival_thread, 0, Packet_Arrival, NULL);
    pthread_create(&Token_Arrival_thread, 0, Token_Arrival, NULL);
    pthread_create(&Server_thread, 0, Server, NULL);
#ifndef __MINGW32__
    pthread_create(&SigInt_interrupt_thread,0,SigInt_interrupt,NULL);
#endif

    pthread_join(Packet_arrival_thread, &result_Packet_arrival );
    pthread_join(Token_Arrival_thread, &result_Token_Arrival );

    if(signal_waiting) {
        pthread_mutex_lock(&mutex);
        pthread_cond_signal(&Q2Q);
        pthread_mutex_unlock(&mutex);
    }

    pthread_join(Server_thread, &result_Server );

#ifndef __MINGW32__
    if(SigInt) pthread_join(SigInt_interrupt_thread, &result_SigInt_interrupt );
    else       pthread_cancel(SigInt_interrupt_thread);
#endif

    StopEmulation();
}

void Initialize_num(void)
{
    if(mode == DETERMINISTIC) num = (int)Parameter[5];
    else                      ParseTraceFile();
}

int main(int argc, char* argv[])
{
    CheckCommandAndGetMode(argc, argv);
    Initialize_num();
    StartEmulation();
    return 0;
}
