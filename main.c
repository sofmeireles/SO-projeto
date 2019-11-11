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
struct flight{
    int type; //departure = 1, arrival = 2
    char code[20];
    int init;
    int takeoff;
    int eta;
    int fuel;
    int holding;
    struct flight * next;
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
struct flight* header_voos;


void print_struct(){
    printf("ua: %d\n",unidade);
    printf("dd: %d, id: %d\n",duracao_descolagem,int_descolagem);
    printf("da: %d, ia: %d\n",duracao_aterragem,int_aterragem);
    printf("hmin: %d, holmax: %d\n",hold_min,hold_max);
    printf("qtdp: %d\n",qtd_max_partidas);
    printf("qtdc: %d\n",qtd_max_chegadas);
}

void print_voos(struct flight* header){
    int i=1;
    struct flight* atual= header;
    if(atual->next == NULL) printf("lista vazia\n");
    while(atual->next != NULL){
        if(atual->next->type==1){
            printf("#%d DEPARTURE %s init:%d takeoff:%d\n",i,atual->next->code,atual->next->init,atual->next->takeoff);
        }
        else{
            printf("#%d ARRIVAL %s init:%d eta:%d fuel:%d\n",i,atual->next->code,atual->next->init,atual->next->eta,atual->next->fuel);
        }
        atual=atual->next;
        i++;
    }
}

struct flight* cria_header_voos(){
    struct flight* header;
    header=malloc(sizeof(struct flight));
    if(header != NULL){
        
        strcpy(header->code,"\0");
        header->eta=0;
        header->fuel=0;
        header->holding=0;
        header->init=0;
        header->takeoff=0;
        header->type=0;
        header->next=NULL;
    }
    return header;
}

void add_voo(struct flight* header,struct flight* node){
    node->next=header->next;
    header->next=node;
    printf("added voo\n");
}


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
    /* Preciso resolver
    else{
    	str[strlen(str)-1]='\t';
    	for(i=0;i<fim;i++){
    	printf("boas : %s/",str[i]);
		if(isdigit(str[i])==0)
		    return false;
    	}
    }*/
    return true;
}

bool verifica_code(char* token){
	  struct flight* atual=header_voos;
	  if(atual->next == NULL) return true;
    while(atual->next != NULL){
        if(strcmp(atual->next->code,token)==0){
            return false;
        }
        else{
            atual=atual->next;
        }
    }
    return true;
}

bool validacao(char * mensagem){
    struct flight* voo=malloc(sizeof(struct flight));
    char* token;
    char* dem="\t";
    int i=1;

    voo->holding=0;

    //DEPARTURE or ARRIVAL
    token=strtok(mensagem,dem);
    if (strcmp(token,"ARRIVAL")==0){
        voo->type=2;
        printf("[%d] Arrival\n",voo->type);
    }
    else if(strcmp(token,"DEPARTURE")==0){
        voo->type=1;
        printf("[%d] Departure\n",voo->type);
    }
    else return false;

    while(token !=NULL){
        token=strtok(NULL,dem);
        //printf("token [%d]: %s\n",i,token);
        if (i==1){ //flight_code
        	  if(verifica_code(token)== false) return false;
        	  else{
            	strcpy(voo->code,token);
            	printf("%s\n",voo->code);
            }
        }
        else if(i==2){
            if(strcmp(token,"init:")!=0) return false;
        }
        else if(i==3 && verifica_numero(token,strlen(token),0)==true){
            voo->init=atoi(token);
            //printf("init:%d\n",voo->init);
        }
        else if(i==4){
            if(strcmp("takeoff:",token)!=0 && strcmp("eta:",token)!=0) return false;
        }
        else if(i==6){
            if(strcmp("fuel:",token)!=0) return false;
        }
        else if(i==5 || i==7){

            /* Ainda não funciona para este último caso
            if(verifica_numero(token,strlen(token),1)==false){
                printf("token: %s%d\n",token,(int)strlen(token));
                printf("fodeu\n");
                   return false;
            }*/

            if(voo->type==1){
                voo->eta=0;
                voo->fuel=0;
                voo->takeoff=atoi(token);
                //printf("takeoff:%d\n",voo->takeoff);
                add_voo(header_voos,voo);
                return true;

            }
            else{
                voo->init=0;
                if(i==5){
                    voo->eta=atoi(token);
                    //printf("eta:%d\n",voo->eta);
                }
                else{
                    voo->fuel = atoi(token);
                    //printf("fuel:%d\n", voo->fuel);
                    add_voo(header_voos,voo);
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
    char comando[1000];
    if ((fd = open(PIPE_NAME, O_RDONLY, O_WRONLY)) < 0) { //ler do pipe
        perror("Erro ao ler o pipe: ");
        exit(0);
    }
    else{
        read(fd,comando,1000);
        if (validacao(comando)==true)
            printf("pipe lido com sucesso\n");
        else
            printf("erro ao ler o pipe\n");
    }
    print_voos(header_voos);
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
    header_voos=cria_header_voos();
    read_config();
    //print_struct();


    //PIPE
    cria_pipe();
    //para escrever no pipe abrir outro terminal e escrever echo "cena">input_pipe

    //THREAD que lê o pipe
    pthread_create(&pipe_thread,NULL,thread_leitura,&pipe_thread_id);


    //MQ
    if ((message_queue = msgget(IPC_PRIVATE, IPC_CREAT | 0700))==-1){
        printf("Erro ao criar a message queue!\n");
        return -1;
    }//else printf("Message queue criada!\n");

    //processo=fork();
    /*
    if(processo==0){
        printf("PID da torre de controlo: %d\n",getpid());
        execl("torre","torre");
    }

    else{
        printf("PID do gestor de simulacao: %d\n",getpid());
    }*/
    pthread_join(pipe_thread,NULL);
    return 0;
}

void ficheiro_log(char* mensagem){
    //mutex
    FILE *f=fopen("log.txt","a");
    time_t horas;
    struct tm* time_struct;

    time(&horas);
    time_struct = localtime(&horas);
    fprintf(f,"%d:%d:%d %s\n",time_struct->tm_hour,time_struct->tm_min,time_struct->tm_sec,mensagem);
    printf("%d:%d:%d %s\n",time_struct->tm_hour,time_struct->tm_min,time_struct->tm_sec,mensagem);
    //mutex
}


int main() {
    inicia();
    wait(NULL);
    return 0;
}