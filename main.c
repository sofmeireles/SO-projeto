#include <stdlib.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/msg.h>
#define PIPE_NAME "/tmp/input_pipe"


//structs
struct departure{
    //departure = 1, arrival = 2
    char code[20];
    int init;
    int takeoff;
    int holding;
    struct departure * next;
};

struct arrival{
	  char code[20];
	  int init;
	  int eta;
    int fuel;
    int holding;
    struct arrival * next;
};

//variaveis globais
int unidade;
int duracao_descolagem;
int duracao_aterragem;
int int_descolagem;
int int_aterragem;
int hold_max;
int hold_min;
int qtd_max_partidas;
int qtd_max_chegadas;
struct arrival* header_arrivals;
struct departure* header_departures;
pthread_mutex_t mutex;


void print_struct(){
    printf("ua: %d\n",unidade);
    printf("dd: %d, id: %d\n",duracao_descolagem,int_descolagem);
    printf("da: %d, ia: %d\n",duracao_aterragem,int_aterragem);
    printf("hmin: %d, holmax: %d\n",hold_min,hold_max);
    printf("qtdp: %d\n",qtd_max_partidas);
    printf("qtdc: %d\n",qtd_max_chegadas);
}

void print_arrivals(struct arrival* header){
    struct arrival* atual= header;
    if(atual->next == NULL) printf("lista vazia\n");
    while(atual->next != NULL){
        printf("ARRIVAL %s init:%d eta:%d fuel:%d\n",atual->next->code,atual->next->init,atual->next->eta,atual->next->fuel);
        atual=atual->next;
    }
}

void print_departures(struct departure* header){
    struct departure* atual= header;
    if(atual->next == NULL) printf("lista vazia\n");
    while(atual->next != NULL){
         printf("DEPARTURE %s init:%d takeoff:%d\n",atual->next->code,atual->next->init,atual->next->takeoff);
         atual=atual->next;
    }
}


void add_departure(struct departure* header,struct departure* node){
    node->next=header->next;
    header->next=node;
    printf("added departure\n");
}

void add_arrival(struct arrival* header, struct arrival* node){
		node->next=header->next;
    header->next=node;
    printf("added arrival\n");
}

void ve_inits(){
		struct departure* dep=header_departures;
	  struct arrival* arr=header_arrivals;
	  
	  //ver nos departures
    while(dep->next != NULL){
        printf("init: %d\n",dep->next->init);
        dep=dep->next;
    }
    //ver nos arrivals    
    while(arr->next != NULL){
        printf("init: %d\n",arr->next->init);
        arr=arr->next;
    }
}

void ficheiro_log(char* mensagem);

void read_config(){
    FILE*f=fopen("config.txt","r");
    fscanf(f,"%d\n%d, %d\n%d, %d\n%d, %d\n%d\n%d",&unidade,&duracao_descolagem,&int_descolagem,&duracao_aterragem,&int_aterragem,&hold_min,&hold_max,&qtd_max_partidas,&qtd_max_chegadas);
}

bool verifica_numero(char* str, int fim, int flag){
    int i;
    if(flag==0){
        for(i=0;i<fim;i++){
            if(isdigit(str[i])==0)
                return false;
        }
    }
    return true;
}

bool verifica_code(char* token){
		//ver nos departures
	  struct departure* dep=header_departures;
	  struct arrival* arr=header_arrivals;
    while(dep->next != NULL){
        if(strcmp(dep->next->code,token)==0){
            return false;
        }
        else{
            dep=dep->next;
        }
    }
    //ver nos arrivals    
    while(arr->next != NULL){
        if(strcmp(arr->next->code,token)==0){
            return false;
        }
        else{
            arr=arr->next;
        }
    }
    return true;
}

bool validacao(char * mensagem){
    struct arrival* arr;
    struct departure* dep;
    char* token;
    char* dem="\t";
    int i=1;
    int type;


    //DEPARTURE or ARRIVAL
    token=strtok(mensagem,dem);
    if (strcmp(token,"ARRIVAL")==0){
        arr=malloc(sizeof(struct arrival));
        type=2;
        //printf("[%d] Arrival\n",voo->type);
    }
    else if(strcmp(token,"DEPARTURE")==0){
        dep=malloc(sizeof(struct departure));
        type=1;
        //printf("[%d] Departure\n",voo->type);
    }
    else return false;
    
    while(token !=NULL){
        token=strtok(NULL,dem);
        //printf("token [%d]: %s\n",i,token);
        if(token == NULL)
        	break;
        if (i==1){ //flight_code
        	  if(verifica_code(token)== false) return false;
        	  else{
        	  	if(type==1){
        	  		strcpy(dep->code,token);
        	  	}
        	  	else{
        	  		strcpy(arr->code,token);
        	  	}
            }
        }
        else if(i==2){
            if(strcmp(token,"init:")!=0) return false;
        }
        else if(i==3 /*&& verifica_numero(token,strlen(token),0)==true*/){
        		if(type==1){
        	  		dep->init=atoi(token);
        	  }
        	  else{
        	  		arr->init=atoi(token);
        	  }
        }
        else if(i==4){
            if(strcmp("takeoff:",token)!=0 && strcmp("eta:",token)!=0) return false;
        }
        
        else if(i==6){
            if(strcmp("fuel:",token)!=0) return false;
        }
        else if(i==5 || i==7){

            

            if(type==1){
            		token[strlen(token)-1]='\0';
            		if(verifica_numero(token,strlen(token),0)==false) return false;
                dep->takeoff=atoi(token);
                add_departure(header_departures,dep);
                return true;

            }
            else{
                if(i==5){
                    arr->eta=atoi(token);
                }
                else{
                		token[strlen(token)-1]='\0';
                		if(verifica_numero(token,strlen(token),0)==false) return false;
                    arr->fuel = atoi(token);
                    add_arrival(header_arrivals,arr);
                    return true;
                }
            }
        }
        i++;
    }
    return false;
}


void le_comandos(){
    int fd;
    char comando[1000], str[1000], cmd[1000];
    if ((fd = open(PIPE_NAME, O_RDONLY, O_WRONLY)) < 0) { //ler do pipe
        perror("Erro ao ler o pipe: ");
        exit(0);
    }
    else{
        read(fd,comando,1000);
        strcpy(cmd,comando);
        if (validacao(comando)==true){
        		sprintf(str,"NEW COMMAND => %s\n",cmd);
            ficheiro_log(str);
        }
        else{
        		sprintf(str,"WRONG COMMAND => %s\n",cmd);
            ficheiro_log(str);
        }
    }
    printf("\n######### Departures #########\n");
    print_departures(header_departures);
    printf("######### ARRIVALS #########\n");
    print_arrivals(header_arrivals);
    //ve_inits();
    
}

void cria_pipe(){
    if ((mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0600)<0) && (errno!= EEXIST)) {
        perror("Erro ao criar o pipe: ");
        exit(0);
    }else printf("Pipe criado!\n");
}

void* thread_leitura(void* idp){
    while(1){
        le_comandos();
    }

    pthread_exit(NULL);
    return NULL;
}

int inicia(){
    int message_queue;
    pid_t processo;
    pthread_t pipe_thread;
    int pipe_thread_id;
    
    header_arrivals=malloc(sizeof(struct arrival));
    header_arrivals->next=NULL;
    header_departures=malloc(sizeof(struct departure));
    header_departures->next=NULL;
    
    read_config();
    //print_struct();


    //PIPE
    cria_pipe();
    //para escrever no pipe abrir outro terminal e escrever echo "cena">input_pipe
		
    //THREAD que lê o pipe
    pthread_create(&pipe_thread,NULL,thread_leitura,&pipe_thread_id);

    //Inicia mutex
    if (pthread_mutex_init(&mutex, NULL) != 0){
        printf("Erro ao inicializar o mutex\n");
        return -1;
    }

    //MQ
    if ((message_queue = msgget(IPC_PRIVATE, IPC_CREAT | 0700))==-1){
        printf("Erro ao criar a message queue!\n");
        return -1;
    }//else printf("Message queue criada!\n");

    processo=fork();
    
    if(processo==0){
        printf("PID da torre de controlo: %d\n",getpid());
        //ficheiro_log(strcat("PID da torre de controlo:",(char*)getpid()));
        //execl("torre","torre");
    }

    else{
        printf("PID do gestor de simulacao: %d\n",getpid());
    }
    pthread_join(pipe_thread,NULL);
    return 0;
}

void ficheiro_log(char* mensagem){
    pthread_mutex_lock(&mutex);
    FILE *f=fopen("log.txt","a");
    time_t horas;
    struct tm* time_struct;
		mensagem[strlen(mensagem)-1]='\0';
    time(&horas);
    time_struct = localtime(&horas);
    fprintf(f,"%d:%d:%d %s",time_struct->tm_hour,time_struct->tm_min,time_struct->tm_sec,mensagem);
    printf("%d:%d:%d %s",time_struct->tm_hour,time_struct->tm_min,time_struct->tm_sec,mensagem);
 		fclose(f);
    pthread_mutex_unlock(&mutex);
}


int main() {
    inicia();
    wait(NULL);
    return 0;
}