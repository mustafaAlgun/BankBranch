/*
        Bank Branch
        Mustafa Algun
        2165777
        07.05.2021
*/

/*
                        REFERENCES

Queue Implementation        -> https://stackoverflow.com/questions/62161356/c-programming-linked-list-and-queue-with-an-array

POSIX Multithread functions -> https://pubs.opengroup.org/onlinepubs/9699919799/

Exp. dist.                  -> https://stackoverflow.com/questions/2106503/pseudorandom-number-generator-exponential-distribution

Command Line Arg            -> https://stackoverflow.com/questions/28129098/using-getopt-in-c-for-command-line-arguments

*/

/*
Console command for compilation --> gcc -o office main.c -pthread -lm

Console command to run          --> ./office
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <getopt.h>
#include <pthread.h>
#include <unistd.h>  // to use POSIX 'sleep(int second)' function

pthread_mutex_t mutex;
int BusySupervisor = 0;
int finish = 0; // to kill threads
int PendingClientNumber = 0; // # waiting clients
pthread_cond_t CV_Supervisor; // condition variable
pthread_cond_t CV_SBusy; // condition variable

///////////////////     QUEUE CLASS IMPLEMENTATION START      ///////////////////

typedef struct Node
{
  void *data;
  struct Node *next;
}node;

typedef struct QueueList
{
    int sizeOfQueue;
    size_t memSize;
    node *head;
    node *tail;
}Queue;


void queueInit(Queue *q, size_t memSize)
{
   q->sizeOfQueue = 0;
   q->memSize = memSize;
   q->head = q->tail = NULL;
}

int enqueue(Queue *q, const void *data)
{
    node *newNode = (node *)malloc(sizeof(node));

    if(newNode == NULL)
    {
        return -1;
    }

    newNode->data = malloc(q->memSize);

    if(newNode->data == NULL)
    {
        free(newNode);
        return -1;
    }

    newNode->next = NULL;

    memcpy(newNode->data, data, q->memSize);

    if(q->sizeOfQueue == 0)
    {
        q->head = q->tail = newNode;
    }
    else
    {
        q->tail->next = newNode;
        q->tail = newNode;
    }

    q->sizeOfQueue++;
    return 0;
}

void dequeue(Queue *q, void *data)
{
    if(q->sizeOfQueue > 0)
    {
        node *temp = q->head;
        memcpy(data, temp->data, q->memSize);

        if(q->sizeOfQueue > 1)
        {
            q->head = q->head->next;
        }
        else
        {
            q->head = NULL;
            q->tail = NULL;
        }

        q->sizeOfQueue--;
        free(temp->data);
        free(temp);
    }
}


int getQueueSize(Queue *q)
{
    return q->sizeOfQueue;
}


///////////////////     QUEUE CLASS IMPLEMENTATION FINISH      ///////////////////

// prints console outputs
void printStart(int clientNumber ,int paydeskThreadNumber ,int maximumQSize ,float generationRateTime ,float durationRateTime ) {

    printf("NUM_CLIENTS: %d\n",clientNumber);
    printf("NUM_DESKS: %d\n",paydeskThreadNumber);
    printf("QUEUE_SIZE: %d\n",maximumQSize);
    printf("DURATION_RATE: %f\n",durationRateTime);
    printf("GENERATION_RATE: %f\n",generationRateTime);
}

struct BankClient { // struct 1
    int id; // ClientID
    float duration;
};

struct Information {	// struct 2
    int paydeskNo;
    struct BankClient client;
} INFO;


float ExpDistRandomGenerator (float lambda) {    // exp. dist. number generator with lambda
    float u;
    u = rand() / (RAND_MAX + 1.0);
    return (-log(1-u) / lambda);
}

struct PayDeskInfo {    // Structure for desk info
    int deskId;
    pthread_cond_t desk_cv; //  desk condition variable
    Queue deskQueue;
};


void* PaydeskFunc (void* arg) {	// paydesk thread function

    struct PayDeskInfo * payDeskReferance = (struct PayDeskInfo *) arg;
    struct BankClient front_client;
    while(1) {
        pthread_mutex_lock(&mutex);

        while (BusySupervisor) { pthread_cond_wait (&CV_SBusy, &mutex);}    // supervisor is working, wait!
        while ( getQueueSize(&payDeskReferance -> deskQueue) == 0 && finish == 0) { pthread_cond_wait (&payDeskReferance -> desk_cv, &mutex);}// no client left

        if (getQueueSize(&payDeskReferance -> deskQueue) == 0 && finish==1) {pthread_mutex_unlock(&mutex); break;} // nobody left, finish
        dequeue(&payDeskReferance->deskQueue,&front_client);    // client leaves
        INFO.paydeskNo = payDeskReferance -> deskId;
        INFO.client = front_client; // next client
        BusySupervisor = 1; // Set BusySupervisor
        sleep(front_client.duration); // Sleep exp.random wrt to the front client
        pthread_cond_signal(&CV_Supervisor); // Wake up supervisor

        pthread_mutex_unlock(&mutex);
    }

    pthread_exit(0);
}

// client thread function
int GoToDesk(struct BankClient* ClientReferance, int MAXDeskNumber, int TotalQueueSize, struct PayDeskInfo** payDeskReferance) {  //clients are sent to desks with this function
    pthread_mutex_lock(&mutex);

    int suitable=0,i=0;
    while(i < MAXDeskNumber) {  // find the most sutable queue
        if (getQueueSize(&(*payDeskReferance)[suitable].deskQueue) < getQueueSize(&(*payDeskReferance)[i].deskQueue)) suitable = i;
        i++;
    }
    if (getQueueSize(&(*payDeskReferance)[suitable].deskQueue) >= TotalQueueSize){
        printf("All queues are full. BankClient %d discarded.\n",ClientReferance->id);
        return 1;   // queue full
    }

    enqueue(&(*payDeskReferance)[suitable].deskQueue, ClientReferance);    // Send client to the queue

    if(getQueueSize(&(*payDeskReferance)[suitable].deskQueue)==1) {
        pthread_cond_signal(&(*payDeskReferance)[suitable].desk_cv);   // queue empty, wake up
    }
    PendingClientNumber++;

    pthread_mutex_unlock(&mutex);
    return 0;
}

void* SupervisorFunc (void* arg) {	// supervisor thread function

    while(1) {
        pthread_mutex_lock(&mutex);
        if (finish && PendingClientNumber==0) {  pthread_mutex_unlock(&mutex); break;}  // no client left
        while( BusySupervisor==0 ) { pthread_cond_wait (&CV_Supervisor, &mutex);}   // wait until supervisor wakes up
        if (finish && PendingClientNumber==0) {  pthread_mutex_unlock(&mutex); break;}  // no client left

        printf("Desk %d served Client %d in %f seconds.\n",INFO.paydeskNo,INFO.client.id,INFO.client.duration );
        PendingClientNumber=PendingClientNumber-1;
        BusySupervisor = 0; // supervioser not busy anymore
        pthread_cond_broadcast(&CV_SBusy); // Wake up the waiters

        pthread_mutex_unlock(&mutex);
    }

    pthread_exit(0);
}


int main(int argc, char *argv[]) {

    srand(time(NULL));

    int CommandChoice = 0;
    int client_Num = 20;
    int paydesk_Thread_Num = 4;
    int maximum_Q_Size = 3;
    float generation_rate_time = 100.0;
    float duration_rate_time = 20.0;


    while (( CommandChoice = getopt(argc, argv, "c:n:q:g:d:")) != -1){
        switch (CommandChoice) {
          case 'c':
                 client_Num = atoi(optarg);
                         break;
          case 'g':
                 generation_rate_time = atof(optarg);
                         break;

          case 'd':
                 duration_rate_time = atof(optarg);
                         break;

          case 'n':
                 paydesk_Thread_Num = atoi(optarg);
                         break;

          case 'q':
                 maximum_Q_Size = atoi(optarg);
                         break;

          default:
            printf("\n");
        }
    }

    printStart( client_Num ,paydesk_Thread_Num , maximum_Q_Size , generation_rate_time ,duration_rate_time );   // first console output
    sleep(1);

    struct PayDeskInfo* DeskInfos = malloc(paydesk_Thread_Num * sizeof(struct PayDeskInfo));    // Allocate memory
    for (int i= 0; i < paydesk_Thread_Num; i++) {       // Desk Informations are created & assigned
        queueInit(&DeskInfos[i].deskQueue, sizeof(struct BankClient));
        DeskInfos[i].deskId = i;
        pthread_cond_init(&DeskInfos[i].desk_cv,0);
    }

    pthread_t* Paydesk_Thread = malloc( paydesk_Thread_Num * sizeof(pthread_t) );   // Allocate memory
    for (int i= 0; i < paydesk_Thread_Num; i++) {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_create(&Paydesk_Thread[i], &attr, PaydeskFunc, &DeskInfos[i]);  // paydesk threads are created with PaydeskFunc
    }


    pthread_mutex_init(&mutex,0);
    pthread_cond_init(&CV_Supervisor,0);
    pthread_cond_init(&CV_SBusy,0);

    pthread_t Supervisor_Thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&Supervisor_Thread, &attr, SupervisorFunc, NULL);    // supervisor thread is created with SupervisorFunc
    sleep(1);

    struct BankClient* Clients = malloc ( client_Num * sizeof(struct BankClient) ); // Allocate memory

    for (int i = 0; i < client_Num; i++) {  // each client is created by main thread
        Clients[i].duration=ExpDistRandomGenerator(duration_rate_time);
        Clients[i].id=i;
        GoToDesk(&Clients[i], paydesk_Thread_Num, maximum_Q_Size, &DeskInfos);
        printf("Client %d arrived\n",i);
        sleep( ExpDistRandomGenerator(generation_rate_time) );  // sleep rand time after each creation
    }


    pthread_mutex_lock(&mutex);
    pthread_cond_broadcast(&CV_SBusy);
    pthread_cond_signal(&CV_Supervisor);
    finish = 1;
    for(int i=0; i<paydesk_Thread_Num; i++) {
        pthread_cond_signal(&DeskInfos[i].desk_cv);
        sleep(0.1);
    }
    pthread_mutex_unlock(&mutex);

    for (int i= 0; i < paydesk_Thread_Num; i++) {
        pthread_join(Paydesk_Thread[i],NULL);   // paydesks joined
        sleep(0.1);
    }

    pthread_mutex_lock(&mutex);
    pthread_cond_signal(&CV_Supervisor);    // supervisor starts
    BusySupervisor = 1;
    sleep(0.1);
    pthread_mutex_unlock(&mutex);
    pthread_join(Supervisor_Thread,NULL);   // supervisor joined

    for(int i=0; i<paydesk_Thread_Num; i++) pthread_cond_destroy(&DeskInfos[i].desk_cv);   // each cv is terminated
    pthread_cond_destroy(&CV_Supervisor);
    pthread_cond_destroy(&CV_SBusy);
    pthread_mutex_destroy(&mutex);

    free(DeskInfos);        // memory deallocation
    free(Paydesk_Thread);   // memory deallocation
    free(Clients);          // memory deallocation


    return 0;
}
