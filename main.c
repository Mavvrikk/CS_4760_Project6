#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/msg.h>
#include <errno.h>

int shmkey = 112369;
int msgkey = 963211;
bool signalReceived = false;
bool stillChildrenRunning = true;

typedef struct messageQ
{
	long mtype;
	int quantum;
	long myPID;
	int rwBit;

} messageQ;

typedef struct FT
{
    int occupied; // either occupied or empty
    int dirtyBit; // dirty bit to track if its written too
    pid_t pid; // pid of page number
    int pNum; // page number held
} FT;

typedef struct PCB
{
	int occupied;		// either true or false
	pid_t pid;		// hold PID of child process
	int pageTable[32];		// PageTable 
} PCB;

void helpFunction ()
{
	printf
		("The function call should go as follows: ./oss [-h] [-n proc] [-s simu] [-t timeLimit] ");
	printf
		("where -h displays this help function and terminates, proc is the number of processes you want to run, simu is the number of simultaneous");
	printf
		("processes that run and timeLimit is approximately the time limit each child process runs.");
}

void incrementClock (int *shmSec, int *shmNano, int addNano)
{
	*shmNano += addNano;
	if (*shmNano >= 1000000)
	{
		*shmNano = 0;
		*shmSec += 1;
	}
}

void forker (int *totalLaunched, int *PCB_INDEX, PCB * processTable)
{
	pid_t pid;

	if (*totalLaunched == 100)
	{
		return;
	}
	else
	{
		if ((pid = fork ()) < 0)	// FORK HERE
		{
			perror ("fork");
		}
		else if (pid == 0)
		{
			/*char* args[]={"./worker",0};
			execlp(args[0],args[0],args[1]);*/
	int* shmSec;
	int* shmNano;
	int shmid = shmget (shmkey, 2 * sizeof(int), 0777 | IPC_CREAT);
	if (shmid == -1)
	{
		perror("ERROR IN SHMGET");
		exit(0);
	}
	shmSec = (int*)(shmat(shmid,0,0));
	shmNano = shmSec + 1;
	int message_queue = msgget (msgkey, 0777 | IPC_CREAT);	
	messageQ msg;
	int Terminate = 0;
	messageQ buff;
	buff.mtype = getppid ();
	buff.myPID = getpid ();
	srand (time (NULL) * getpid ());
	int TimeLastRequestNano = 0;
	int TimeLastRequestSec = 0;
	int R_HELD[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	while (Terminate == 0)
	{
		int TaskToDo = rand () % 100;	// RANDOMIZER TO DETERMINE CHILD FUNCTION
		int pNum = rand () % 32;
		int offSet = rand () % 1024;
		int Addy = pNum + offSet;
		if (TimeLastRequestNano < *shmNano  && TimeLastRequestSec < *shmSec)	// If the right amount of time has passed, request again
		{
			TimeLastRequestSec = *shmSec;	//reset time since last request
			TimeLastRequestNano = *shmNano;
			
			if (TaskToDo < 8)
			{		// Process terminates
				buff.quantum = -1;
				Terminate = 1;
			}
			else if (TaskToDo > 85)	// Process writes to a address
			{
				buff.quantum = Addy;
				buff.rwBit = 1; // WRITING TO ADDRESS
			}
			else
			{		// Process reads from an address 10-85
				buff.quantum = Addy;
				buff.rwBit = 0; // READING FROM ADDRESS
			}
			
			if (msgsnd(message_queue, &buff, sizeof (buff) - sizeof (long), 0) == -1)
			{
				perror ("MESSAGE SND (WORKER) FAILED");
				exit (1);
			}
			if(buff.quantum != -1)
			{
				if (msgrcv (message_queue, &msg, sizeof (msg) - sizeof (long), getpid(), 0) == -1)
				{
					perror ("MESSAGE RCV (WORKER) FAILED");
					exit (1);
				}
				if (buff.quantum >= 0) // if we page faulted
				{
					while (TimeLastRequestNano <= *shmNano + 14000); // wait for 14 ms
				}
			}
		}
	}
			
		}
		else if (pid > 0)
		{
			// record child into PCB
			processTable[*PCB_INDEX].occupied = 1;
			processTable[*PCB_INDEX].pid = pid;
			// Up Launch Count & PCB_INDEX Count
			*totalLaunched += 1;
			return;
		}
	}
}

bool PCB_isEmpty (PCB * processTable)
{
	int i = 0;
	for (i; i < 17; i++)
	{
		if (processTable[i].occupied != 0)
			return false;
	}
	return true;
}

void initializeStruct (struct PCB *processTable)
{
	int i = 0;
	for (i; i < 17; i++)
	{
		processTable[i].occupied = 0;
		processTable[i].pid = 0;
		int j = 0;
		for (j; j < 32; j++)
		{
			processTable[i].pageTable[j] = 0;
		}
	}
return;
}

void initializeFrameTable(struct FT *frameTable, int SIZE)
{
    int i = 0;
    for (i; i<SIZE; i++)
    {
        frameTable[i].occupied = 0;
        frameTable[i].dirtyBit = 0;
        frameTable[i].pid = 0;
        frameTable[i].pNum = -1; // has to be a value that couldnt naturally appear hear
    }
}

int PCB_Has_Room (struct PCB *processTable)
{
	int i = 0;
	for (i; i < 17; i++)
	{
		if (processTable[i].occupied == 0)	// if our PCB has a non occupied slot give it to a needy process
			return i;
	}
	return -1;
}

void sig_handler (int signal)
{
	printf ("\n\nSIGNAL RECEIVED, TERMINATING PROGRAM\n\n");
	stillChildrenRunning = false;
	signalReceived = true;
}

void sig_alarmHandler (int sigAlarm)
{
	printf ("TIMEOUT ACHIEVED. PROGRAM TERMINATING\n");
	stillChildrenRunning = false;
	signalReceived = true;
}

char *x = NULL;
char *y = NULL;
char *z = NULL;
char *LogFile = NULL;


int  main (int argc, char **argv)
{
	int option;
	while ((option = getopt (argc, argv, "f:h")) != -1)
	{
		switch (option)
		{
			case 'h':
				helpFunction ();
				return 0;
			case 'f':
				LogFile = optarg;
				break;
		}
	}


	//INITIALIZE ALL VARIABLES
	int totalLaunched = 0;
	int PCB_INDEX = 0;
	int status;
	int *shmSec;
	int *shmNano;
	PCB processTable[17];
	messageQ msg;
	messageQ rcvbuf;
    FT frameTable [256]; 
	//create file for LOGFILE
	FILE *fLog;

	// signal handlers
	signal (SIGINT, sig_handler);
	signal (SIGALRM, sig_alarmHandler);
	// alarm (60);			// break at 60 seconds
	
	// initialize PCB and Frame Table 
	initializeStruct(processTable);
	initializeFrameTable(frameTable, 256);
	
	// Set up message queue
	int message_queue;
	if ((message_queue = msgget (msgkey, 0777 | IPC_CREAT)) == -1)
	{
		perror ("MSGET");
		exit (1);
	}

	if ((fLog = fopen ("LOGFILE","w")) == NULL)
	{
		perror("error in file");
		exit(0);
	}

	//set up shared memory
	int shmid = shmget (shmkey, 2 * sizeof (int), 0777 | IPC_CREAT);	// create shared memory
	if (shmid == -1)
	{
		perror("ERROR IN SHMGET\n");
		exit (0);
	}

	//Clock
	shmSec = (int *) (shmat (shmid, 0, 0));	// create clock variables
	shmNano = shmSec + 1;
	*shmSec = 0;			// initialize clock to zero
	*shmNano = 0;
	int timeLastChildwasLaunchedNano = 0;
	int timeLastChildwasLaunchedSec = 0;

	//LAUNCH INITIAL CHILD 
	forker (&totalLaunched, &PCB_INDEX, processTable);
	PCB_INDEX = 0;
	incrementClock (shmSec, shmNano, 20000);
	while (stillChildrenRunning)
	{

		if (*shmNano >= timeLastChildwasLaunchedNano + 20000 && *shmSec >= timeLastChildwasLaunchedSec)
		{			// if enough time has passed to launch a new child, do it
			timeLastChildwasLaunchedNano = *shmNano;	// reset counter variables
			timeLastChildwasLaunchedSec = *shmSec;
			incrementClock (shmSec, shmNano, 20000);
			PCB_INDEX = PCB_Has_Room (processTable);
			if (PCB_INDEX >= 0)
			{			// If theres an unoccupied slot in PCB, Launch a child
				forker (&totalLaunched, &PCB_INDEX, processTable);
			}
		}
		// NOW THAT CHILDREN HAVE BEEN CREATED WAIT FOR A MEMORY REQUEST
		if (msgrcv (message_queue, &rcvbuf, sizeof (rcvbuf) - sizeof (long), getpid (), IPC_NOWAIT) == -1)
		{
			if (errno == ENOMSG)
			{
				// do nothing, -1 because no msg exists
			}
			else
			{
				perror ("MESSAGE RECEIVED FAILED (PARENT)");
				printf ("%d  %d\n", errno, ENOMSG);
				exit (1);
			}
		}
		else
		{

			if (rcvbuf.quantum >= 0)	// if it Didnt terminate
			{
				fprintf(fLog, "OSS: PROCESS %ld REQUESTING READ OF ADDRESS %d AT TIME %d:%d \n", rcvbuf.myPID, rcvbuf.quantum,*shmSec,*shmNano);
                // IF IT ISNT IN THE FRAM TABLE, BLOCK IT FOR 14 ms
				else
				{
					fprintf (fLog, "OSS: ADDRESS %d is in frame PERCENT d. READ/WRITING at time %d:%d",  rcvbuf.quantum, *shmSec, *shmNano);
					msg.quantum = rcvbuf.quantum;	// give the message the resource requested
					msg.mtype = rcvbuf.myPID;
					if (msgsnd (message_queue, &msg, sizeof (msg) - sizeof (long), 0) == -1)	//msg that proc to know it was granted
					{
						perror ("MESSAGE SEND FAILED (PARENT)");
						printf("%d", errno);
						exit (1);
					}
				}
			}

			else if (rcvbuf.quantum == -1)  // this should equal -1,  meaning proc terminated
			{
				printf("	RECIEVED %d FROM PROC %ld; TERMINATED \n", rcvbuf.quantum,rcvbuf.myPID);
				int i = 0;
				for (i; i < 17; i++) 
				{
					if (processTable[i].pid == (pid_t)rcvbuf.myPID)
						processTable[i].occupied = 0;
				}
			}
		}

		incrementClock (shmSec, shmNano, 2000);

		if (PCB_isEmpty (processTable) == true)
			stillChildrenRunning = false;
	}

	if (signalReceived == true)
	{				// KILL ALL CHILDREN IF SIGNAL
		int i = 0;
		pid_t childPid;
		for (i; i < 17; i++)
		{
			if (processTable[i].pid > 0 && processTable[i].occupied == 1)
			{			// IF there is a process who is still runnning
				childPid = processTable[i].pid;
				kill (childPid, SIGKILL);
			}
		}
	}

	//DETACH SHARED MEMORY
	shmdt (shmSec);
	shmctl (shmid, IPC_RMID, NULL);
	// DETACH MESSAGE QUEUE
	if (msgctl (message_queue, IPC_RMID, NULL) == -1)
	{
		perror ("MSGCTL");
		exit (1);
	}
	return (0);
}

