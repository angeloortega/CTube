#include <unistd.h> 
#include <stdlib.h> 
#include <stdio.h>
#include <sys/sem.h>
#include <time.h>
#include <sys/shm.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
//#include "semun.h"


static int set_semvalue(void); 
static void del_semvalue(void); 
static int semaphore_p(void); 
static int semaphore_v(void);
static int sem_idBola;
static int sem_idCanchaA;
static int sem_idCanchaB;
static time_t initTime;
static int verify_Time(time_t initTime);
static int obtenerCancha(char equipo);

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short  *array;
};

struct shared_use_st{
	int canchaA;
	int canchaB;
	int flag;
};

static int semaphoreBola_v(void) //This is the release operation for the ball
{ 
	struct sembuf sem_b;

	sem_b.sem_num = 0; 
	sem_b.sem_op = 1; 
	sem_b.sem_flg = SEM_UNDO; 
	if (semop(sem_idBola, &sem_b, 1) == -1) { 
		fprintf(stderr, "semaphore_v failed\n"); 
		return(0); 
	} 
	return(1);
}

static int semaphoreBola_p(void) //This is the wait operation for the ball
{ 
	struct sembuf sem_b;
	sem_b.sem_num = 0; 
	sem_b.sem_op = -1;
	sem_b.sem_flg = SEM_UNDO; 
	if (semop(sem_idBola, &sem_b, 1) == -1) { 
		fprintf(stderr, "semaphore_p failed\n"); 
		return(0); 
	} 
	return(1);
} 

static int semaphoreCanchaA_v(void)//This is the release operation for goalA
{
	struct sembuf sem_b;

	sem_b.sem_num = 0; 
	sem_b.sem_op = 1; /* V() */ 
	sem_b.sem_flg = IPC_NOWAIT; 
	if (semop(sem_idCanchaA, &sem_b, 1) == -1) { 
		return(0); 
	} 
	return(1);
}

static int semaphoreCanchaA_p(void) //This is the wait operation for goalA
{ 
	struct sembuf sem_b;
	sem_b.sem_num = 0; 
	sem_b.sem_op = -1; /* P() */ 
	sem_b.sem_flg = IPC_NOWAIT; 
	if (semop(sem_idCanchaA, &sem_b, 1) == -1) { 
		//fprintf(stderr, "semaphore_p failed\n"); 
		return(0); 
	} 
	return(1);
} 

static int semaphoreCanchaB_v(void)//This is the release operation for goalB
{
	struct sembuf sem_b;

	sem_b.sem_num = 0; 
	sem_b.sem_op = 1; /* V() */ 
	sem_b.sem_flg = IPC_NOWAIT; 
	if (semop(sem_idCanchaB, &sem_b, 1) == -1) { 
		return(0); 
	} 
	return(1);
}

static int semaphoreCanchaB_p(void) //This is the wait operation for goalB
{ 
	struct sembuf sem_b;
	sem_b.sem_num = 0; 
	sem_b.sem_op = -1; /* P() */ 
	sem_b.sem_flg = IPC_NOWAIT; 
	if (semop(sem_idCanchaB, &sem_b, 1) == -1) { 
		return(0); 
	} 
	return(1);
} 


static void del_semvalue(void) // This removes the semaphore from the system
{ 
	union semun sem_union;
	if (semctl(sem_idBola, 0, IPC_RMID, sem_union) == -1) 
		fprintf(stderr, "Failed to delete semaphore\n");
	if (semctl(sem_idCanchaA, 0, IPC_RMID, sem_union) == -1) 
		fprintf(stderr, "Failed to delete semaphore\n");
	if (semctl(sem_idCanchaB, 0, IPC_RMID, sem_union) == -1) 
		fprintf(stderr, "Failed to delete semaphore\n");

}

static int set_semvalue(void) // This initializes the semaphore
{ 
	union semun sem_union;
	int value = 1;
	sem_union.val = 1; 
	if (semctl(sem_idBola, 0, SETVAL, sem_union) == -1) 
		value = 0;
	if (semctl(sem_idCanchaA, 0, SETVAL, sem_union) == -1) 
		value = 0;
	if (semctl(sem_idCanchaB, 0, SETVAL, sem_union) == -1) 
		value = 0;
	return(value);
}

static int verify_Time(time_t initialTime) // Verifies time is under 
										//the 5 minutes of playgame
{
	time_t actualTime;
	actualTime = time(NULL);
	int dif = actualTime - initialTime;
	int n= dif; //in seconds
	if (n >= 30*1)
		return (0);
	return (1);
}	

static int obtenerCancha(char equipo){ //Requests the goal resource
	if (equipo == 'A'){				// a maximum of three times
		for (int p=0; p<3;p++){
			if (semaphoreCanchaB_p() == 0){
				sleep(1);
				continue;
			}
			return (0);
		}
		return(-1);
	}
	if (equipo == 'B'){
		for (int p=0; p<3;p++){
			if (semaphoreCanchaA_p() == 0){
				
				sleep(1);
				continue;
			}
			return (0);
		}
		return(-1);
	}	
}

static int soltarCancha(char equipo){ //Releases the goal resource
	if (equipo == 'A'){
		if (!semaphoreCanchaB_v())
			return(-1);
		return (0);
	}
	else if (equipo=='B'){
		if (!semaphoreCanchaA_v())
			return(-1);
		return (0);
	}
}

int main(int argc, char *argv[]) 
{
	void *shared_memory = (void *)0;
	struct shared_use_st *shared_stuff;
	int shmid;
	srand((unsigned int)getpid());
	shmid = shmget((key_t)1234, sizeof(struct shared_use_st), 0666 | IPC_CREAT);
	if (shmid == -1) {
		fprintf(stderr, "shmget failed\n");
		exit(EXIT_FAILURE);
	}
	shared_memory = shmat(shmid, (void *)0, 0);
	if (shared_memory == (void *)-1) {
		fprintf(stderr, "shmat failed\n");
		exit(EXIT_FAILURE);
	}

	shared_stuff = (struct shared_use_st *)shared_memory;
	shared_stuff->canchaA=0;
	shared_stuff->canchaB=0;
	shared_stuff->flag = 1;
	pid_t pid;
	int procesosHijos[12];
	sem_idBola = semget((key_t)1234, 1, 0666 | IPC_CREAT);
	sem_idCanchaA = semget((key_t)1134, 1, 0666 | IPC_CREAT);
	sem_idCanchaB = semget((key_t)2345, 1, 0666 | IPC_CREAT);
	char equipo='P';
	int porteroA=0;
	int porteroB=0;
	for (int x=0; x<12 ; x=x+1){
		pid = fork();
		
		if (pid == 0)
		{
			if (x==10) porteroA = getpid();		//sets up the goalie procs
			else if (x==11) porteroB = getpid(); // sets up the goales procs
			else if (x<5) equipo = 'A';
			else equipo = 'B';
			printf("Se creo el proceso %d con padre %d del equipo %c\n",getpid(),getppid(),equipo);
			procesosHijos[x] = getpid();
			sleep(2);
			break;
		}	
		else
			procesosHijos[x] = pid;
	}
	
	int i; int pause_time;
	srand((unsigned int)getpid());
	
	if (pid > 0) { //The father process initializes the semaphores
		if (!set_semvalue()) { 
			fprintf(stderr, "Failed to initialize semaphore\n"); 
			exit(EXIT_FAILURE); 
		}
		printf("Se inicializaron los semaforos\n");
		fflush(stdout); 
		
	}

	if (pid>0){ 
		sleep(1);
		initTime = time(NULL);
		printf("\nVAMOS AL FUTBOOOLLLLL\t COMIENZA EL PARTIDO\n");
	}

	while(shared_stuff->flag == 1) { 
		 if (pid == 0) {
			if (equipo == 'P' && porteroA!=0){
				int pause = rand() % 10;
				sleep(pause);
				if (semaphoreCanchaA_p()!=0){
					printf("\nEl portero del equipo A esta muy ATENTO\n");
					sleep(8);
					printf("\nEl portero del equipo A esta DISTRAIDO\n");
					if (semaphoreCanchaA_v()==-1)
						exit(EXIT_FAILURE);
					
				}
				continue;
			}
			if (equipo == 'P' && porteroB!=0){
				int pause = rand() % 10;
				sleep(pause);
				if (semaphoreCanchaB_p()!=0){
					printf("\nEl portero del equipo B esta muy ATENTO\n");
					sleep(8);
					printf("\nEl portero del equipo B esta DISTRAIDO\n");	
					if (semaphoreCanchaB_v()==-1)
						exit(EXIT_FAILURE);
					
				}
				continue;
			}
			pause_time = 5 + rand() % 15;
			sleep(pause_time);
			if (!semaphoreBola_p()) 
				exit(EXIT_FAILURE); 
			printf("\nEl proceso %d del equipo %c obtuvo la bola y quiere encarar al marco\n", getpid(), equipo);
			fflush(stdout);
			if (obtenerCancha(equipo) == 0){
				if (equipo == 'A') shared_stuff->canchaA++;
				else if (equipo == 'B') shared_stuff->canchaB++;
				printf("\nEl proceso %d del equipo %c metio un GOLAZO\n", getpid(),equipo);
				fflush(stdout);
				printf("\nAhora el marcador es Equipo A: %d y Equipo B: %d\n", shared_stuff->canchaA, shared_stuff->canchaB);
				fflush(stdout);
				if (soltarCancha(equipo) == -1)
					exit(EXIT_FAILURE);
			}
			else{
				printf("\nEl PORTERO le ha TAPADO un gol al PROCESO %d del EQUIPO %c\n", getpid(),equipo);
			}
		    	
			if (!semaphoreBola_v()) 
				exit(EXIT_FAILURE);
		}
		else {
			sleep(2);
			if (verify_Time(initTime) == 0){
				printf("\nPITO PITO PITO PITOOOOOO\nFINALIZA EL PARTIDO\n");
				for(int t=0;t<12;t++){
					kill(procesosHijos[t],SIGKILL);
					printf("El proceso hijo %d fue terminado\n",
						 procesosHijos[t]);	
				}				
				shared_stuff->flag=0;
				
			}
			sleep(3);			
		}			
	}
	if (pid == 0){
		printf("\nEl proceso hijo %d ha terminado su ejecucion\n", 
				getpid());
		exit(EXIT_SUCCESS);
	}
	else{
		printf("\nEL MARCADOR FINAL ES EQUIPO A: %d Y EQUIPO B: %d\n",
			 shared_stuff->canchaA, shared_stuff->canchaB);
		del_semvalue();
		exit(EXIT_SUCCESS);
	}

}

//cc pruebaSem.c -o pr
