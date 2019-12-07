//run with gcc -pthread -D_REENTRANT -Wall main2.c -o main2

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
#include <signal.h>
#include <time.h>
#define PIPE_NAME "/tmp/input_pipe"
#define MAX_THREADS 20
#define MAX_ARRIVALS 10
#define MAX_DEPARTURES 10

//structs
struct departure{
    char code[20];
    int init;
    int takeoff;
    int holding;
    int slot;
    struct departure * next;
};

struct arrival{
    char code[20];
    int init;
    int eta;
    int fuel;
    int holding;
    int slot;
    int prioridade;
    struct arrival * next;
};

struct voo{
    struct arrival* arr;
    struct departure* dep;
    struct voo* next;
    int slot;
};

typedef struct{
  int num_voos_cria;
  int num_voos_atr;
  int temp_de_esp_atr;
  int num_voos_desc;
  int temp_de_esp_desc;
  int m_mh_atr;
  int m_mh_hurg;
  int num_voos_red;
  int voos_rejeitados;
}mem_structure;
mem_structure *data;

typedef struct{
  long mtype;
  char code[20];
  int takeoff;
  int eta;
  int fuel;
  int ids;
}voos_send_msg;

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
int qtd_max_voos;
int message_queue;
int shmid;
int shmid_arr;
int shmid_dep;
int j;
time_t time_init;
pthread_t thread_voos[MAX_THREADS];
int ids[MAX_THREADS];
struct arrival* header_arrivals;
struct departure* header_departures;
struct arrival* shm_arrivals;
struct departure* shm_departures;
struct arrival* fila_arrivals;
struct departure* fila_departures;
struct voo* header_voos;
pthread_mutex_t mutex;
pthread_mutex_t mutex2;
pthread_mutex_t mutex_fuel;
pthread_t time_thread;
pthread_t tempo_atual_thread;
pthread_t pipe_thread;
FILE * f_log;
sem_t *mutexx;
sem_t *mutexlog;
sem_t *mutexmsg;


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
        printf("ARRIVAL %s init:%d eta:%d fuel_inicial:%d\n",atual->next->code,atual->next->init,atual->next->eta,atual->next->fuel);
        atual=atual->next;
    }
}

void print_arr_shm(struct arrival* shm_arrivals){
    printf("\nARRIVALS NA SHM:\n");
    for(int i =0; i<MAX_ARRIVALS;i++){
        if((shm_arrivals+i)->slot != 0){
            printf("\tARRIVAL %s init:%d eta:%d fuel:%d prioridade: %d\n",(shm_arrivals+i)->code,(shm_arrivals+i)->init,(shm_arrivals+i)->eta,(shm_arrivals+i)->fuel,(shm_arrivals+i)->prioridade);
        }
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

void print_voos(){
    struct voo* atual= header_voos;
    printf("TODOS OS VOOS\n");
    if(atual->next == NULL) printf("lista vazia\n");
    while(atual->next != NULL){
        if(atual->next->arr==NULL){
            printf("DEPARTURE %s init:%d takeoff:%d\n",atual->next->dep->code,atual->next->dep->init,atual->next->dep->takeoff);
        }
        else{
            printf("ARRIVAL %s init:%d eta:%d fuel:%d\n",atual->next->arr->code,atual->next->arr->init,atual->next->arr->eta,atual->next->arr->fuel);

        }
        atual=atual->next; 
    }
}

void add_voo(struct voo* node){
    struct voo* atual = header_voos;
    
    while(atual->next != NULL){
        atual=atual->next;
    }
    atual->next=node;
    printf("added flight\n");
}

/*
void remove_arrival();
void remove_departure();

void remove_voo(char* nome){
    struct voo* atual=header_voos->next;
    struct voo* anterior=header_voos;
    while(atual->next != NULL){ 
        if(atual->arr != NULL){
            if(strcmp(atual->arr->code,nome)==0){
                anterior->next=atual->next;
                free(atual);
                remove_arrival(nome);
                break;
            }
        }
        else if(atual->dep != NULL){
            if(strcmp(atual->dep->code,nome)==0){
                anterior->next=atual->next;
                free(atual);
                remove_departure(nome);
                break;
            }
        }
        atual=atual->next;
        anterior=anterior->next;
    }
    if(atual->next==NULL){
        if(atual->arr != NULL){
            if(strcmp(atual->arr->code,nome)==0){
                anterior->next=NULL;
                free(atual);
                remove_arrival(nome);
            }
        }
        else if(atual->dep != NULL){
            if(strcmp(atual->dep->code,nome)==0){
                anterior->next=NULL;
                free(atual);
                remove_departure(nome);
            }
        }
    }
}
*/

void add_departure(struct departure* node){
    struct departure* atual = header_departures;
    struct voo* novo_voo=malloc(sizeof(struct voo));
    while(atual->next !=NULL && atual->next->init < node->init){
            atual=atual->next;
    }
    if(atual->next==NULL){
        atual->next=node;
    }
    else{
        node->next=atual->next;
        atual->next=node;
    }
    novo_voo->arr=NULL;
    novo_voo->dep=node;
    novo_voo->next=NULL;
    add_voo(novo_voo);
    //printf("added departure\n");
}

void add_arrival(struct arrival* node){
    struct arrival* atual = header_arrivals;
    struct voo* novo_voo=malloc(sizeof(struct voo));
    sem_wait(mutexx);
    while(atual->next !=NULL && atual->next->init < node->init){
            atual=atual->next;
    }
    if(atual->next==NULL){
        atual->next=node;
    }
    else{
        node->next=atual->next;
        atual->next=node;
    }
    novo_voo->arr=node;
    novo_voo->dep=NULL;
    novo_voo->next=NULL;
    add_voo(novo_voo);
    sem_post(mutexx);
    //printf("added arrival\n");
}

void remove_arrival(char* nome){
    struct arrival* atual=header_arrivals->next;
    struct arrival* anterior=header_arrivals;

    while(atual->next != NULL){ 
        if(strcmp(atual->code,nome)==0){
            anterior->next=atual->next;
            free(atual);
            break;
        }
        else{
            anterior=anterior->next;
            atual=atual->next;
        }
    }
    if(atual->next == NULL){
            if(strcmp(atual->code,nome)==0){
                anterior->next=NULL;
                free(atual);
            }
    }
}

void remove_departure(char* nome){
    struct departure* atual=header_departures->next;
    struct departure* anterior=header_departures;

    while(atual->next != NULL){ 
        if(strcmp(atual->code,nome)==0){
            anterior->next=atual->next;
            free(atual);
            break;
        }
        else{
            anterior=anterior->next;
            atual=atual->next;
        }
    }
    if(atual->next == NULL){
            if(strcmp(atual->code,nome)==0){
                anterior->next=NULL;
                free(atual);
            }
    }
}

void add_fila_arrivals(struct arrival* node){
    struct arrival* atual = fila_arrivals;
    printf("\n");
    while(atual->next !=NULL){
        if(atual->next->prioridade > node->prioridade){
            atual=atual->next;
        }
        else if(atual->next->prioridade == node->prioridade) {
            if (atual->next->eta < node->eta) {
                atual = atual->next;
            }
            node->next = atual->next;
            atual->next = node;
            break;
        }
        else{
            node->next = atual->next;
            atual->next = node;
            break;
        }
    }
    if(atual->next==NULL){
        atual->next=node;
    }
    //print_arrivals(fila_arrivals);
}

void add_fila_departures(struct departure* node){
    struct departure* atual = fila_departures;
    while(atual->next !=NULL && atual->next->takeoff < node->takeoff){
            atual=atual->next;
    }
    if(atual->next==NULL){
        atual->next=node;
    }
    else{
        node->next=atual->next;
        atual->next=node;
    }
    //printf("added departure à fila\n");
}


void ficheiro_log(char* mensagem);

void read_config(){
    FILE* configs=fopen("config.txt","r");
    char linha[30];
    char* token;
    int i=0;

    //falta meter proteção para se a linha das virgulas estiver
    //vazia, mas ainda não sei como fazer

    if(configs==NULL){
        perror("Erro a ler o ficheiro config.txt");
    }

    while(fgets(linha,30,configs)){
        if(i==0){ 
            unidade=atoi(linha);
            if (unidade==0)
                unidade=500;
        }

        else if(i==1){
            token=strtok(linha,",");
            duracao_descolagem=atoi(token);
            if (duracao_descolagem==0)
                duracao_descolagem=30;
            token=strtok(NULL,",");
            int_descolagem=atoi(token);
            if (int_descolagem==0)
                int_descolagem=5;
        }

        else if(i==2){
            token=strtok(linha,",");
            duracao_aterragem=atoi(token);
            if (duracao_aterragem==0)
                duracao_aterragem=20;
            token=strtok(NULL,",");
            int_aterragem=atoi(token);
            if (int_aterragem==0)
                int_aterragem=10;
        }

        else if(i==3){
            token=strtok(linha,",");
            hold_min=atoi(token);
            if (hold_min==0)
                hold_min=75;
            token=strtok(NULL,",");
            hold_max=atoi(token);
            if (hold_max==0)
                hold_max=100;
        }

        else if(i==4){
            qtd_max_partidas=atoi(linha);
            if (qtd_max_partidas==0)
                qtd_max_partidas=100;
        }
        else if(i==5){
            qtd_max_chegadas=atoi(linha);
            if (qtd_max_chegadas==0)
                qtd_max_chegadas=1000;
        }
        i++;
    }
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

void cria_threads_voo();

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
        arr->next=NULL;
        type=2;
        //printf("[%d] Arrival\n",voo->type);
    }
    else if(strcmp(token,"DEPARTURE")==0){
        dep=malloc(sizeof(struct departure));
        dep->next=NULL;
        type=1;
        //printf("[%d] Departure\n",voo->type);
    }
    else return false;

    while(token !=NULL){
        token=strtok(NULL,dem);
        //printf("token [%d]: %s\n",i,token);
        if(token == NULL){
            break;
        }
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
        else if(i==3 && verifica_numero(token,strlen(token),0)==true){
       
            if(atoi(token) < (int)(time(NULL)-time_init)){
               return false;
            }
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
                add_departure(dep);
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
                    add_arrival(arr);
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
	memset(comando,0,1000);
	
        read(fd,comando,1000);
        strcpy(cmd,comando);
        if (validacao(comando)==true){
        	sprintf(str,"NEW COMMAND => %s",cmd);
            ficheiro_log(str);
        }
        else{
        	sprintf(str,"WRONG COMMAND => %s",cmd);
            ficheiro_log(str);
        }
       
    }
    
}

//função de operação das threads
void *gere_voos(void* arg){
    voos_send_msg msg;
    printf("criou um voo\n");
    while(1){
      if((msgrcv(message_queue, &msg, sizeof(msg)-sizeof(long), 2, 0)) != -1){
        printf("Recebi de volta %d\n", msg.ids);
        printf("Recebi de volta %d, %d, %d \n", msg.fuel, msg.eta, msg.takeoff);
        pthread_exit(NULL);
      }
    }
}

void cria_pipe(){
    if ((mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0600)<0) && (errno!= EEXIST)) {
        perror("Erro ao criar o pipe\n ");
        exit(0);
    }else printf("Pipe criado!\n");
}

void ve_inits();


void cria_threads_voo(){
  ids[j] = j;
  if((pthread_create(&thread_voos[j], NULL, gere_voos,(void*) &ids[j])) != 0){
    printf("ERRO a criar thread\n");
  }
  printf("criou a thread[%d]\n", ids[j]);
  j++;
}

void* thread_leitura(void* idp){
    while(1){
    	pthread_mutex_lock(&mutex2);
        le_comandos();
        pthread_mutex_unlock(&mutex2);
    }

    pthread_exit(NULL);
    return NULL;
}

void* thread_controlo(void* idp){
    int num;
    //char nome[100];
    while(1){
        scanf("%d",&num);
        if(num==1){
            int tempo_atual = (int)(time(NULL)-time_init);
            printf("tempo_atual: %d\n",tempo_atual);
        }
        else if(num==2){
            print_arrivals(header_arrivals);
            print_departures(header_departures);
            print_voos();

        }
        else if(num==3){
            printf("n. threads = %d\n",j);
        }
        else if(num==4){
            print_arr_shm(shm_arrivals);
            print_departures(fila_departures);
            print_arrivals(fila_arrivals);
        }
    }
    pthread_exit(NULL);
    return NULL;
}

void* thread_cria_voos(void* idp){
    struct arrival* atual_arrival=header_arrivals;
    struct departure* atual_departure=header_departures;
    voos_send_msg msg;
    while(1){
        int tempo_atual = (int)(time(NULL)-time_init);
        if (atual_arrival->next!=NULL){
            int tempo_prox_arr =atual_arrival->next->init;
            if (tempo_atual == atual_arrival->next->init){
                //MQ
                msg.eta = atual_arrival->next->eta;
                msg.fuel  = atual_arrival->next->fuel;
                strcpy(msg.code,atual_arrival->next->code);
                msg.mtype = 1;
                printf("sending(%d and %d and %s)\n", msg.eta, msg.fuel, msg.code);
                if( (msgsnd(message_queue, &msg, sizeof(msg)-sizeof(long), 0)) == -1){
                  printf("erro a enviar a mensagem\n");
                  perror(0);
                }
                if (atual_arrival->next->next!=NULL){
                    atual_arrival=atual_arrival->next;
                    printf("tempo espera: %d\n",atual_arrival->next->init - tempo_atual);
                    sleep(tempo_prox_arr - tempo_atual);
                }
                else{
                    sleep(1);
                }
            }
        }
        if (atual_departure->next != NULL){
            int tempo_prox_dep = atual_departure->next->init;
            if (tempo_atual == atual_departure->next->init){
                //printf("init_dep: %d\n", atual_departure->next->init);
                //MQ
                msg.takeoff = atual_departure->next->takeoff;
                msg.mtype = 1;
                strcpy(msg.code,atual_departure->next->code);
                msg.fuel=-1;
                printf("sending(%d)\n", msg.takeoff);
                if( (msgsnd(message_queue, &msg, sizeof(msg)-sizeof(long), 0)) == -1){
                  printf("erro a enviar a mensagem\n");
                  perror(0);
                }
                cria_threads_voo();
                if (atual_departure->next->next!=NULL){
                    atual_departure=atual_departure->next;
                    printf("tempo espera: %d\n",atual_departure->next->init - tempo_atual);
                    sleep(tempo_prox_dep - tempo_atual);
                }
                else{
                    sleep(1);
                }
            }
        }

    }
    pthread_exit(NULL);
    return NULL;
}

void cria_memoria(){
    shmid = shmget(IPC_PRIVATE, sizeof(mem_structure), IPC_CREAT | 0766);
    if(shmid < 0){
        printf("ERRO na criacao da memoria\n");
        exit(-1);
    }

    data = (mem_structure*)shmat(shmid, NULL, 0);
    if(data == (mem_structure*)-1){
        printf("ERRO no mapeamento da memoria\n");
        exit(-1);
    }
    else{
        printf("Memoria mapeada\n");
    }
    printf("qtd max: %d\n",qtd_max_voos);
    shmid_arr = shmget(IPC_PRIVATE, sizeof(struct arrival)*MAX_THREADS, IPC_CREAT | 0766);
    if(shmid_arr < 0){
        printf("ERRO na criacao da memoria2\n");
        exit(-1);
    }

    shm_arrivals = (struct arrival*)shmat(shmid, NULL, 0);
    if(shm_arrivals == NULL){
        printf("ERRO no mapeamento da memoria\n");
        exit(-1);
    }
    else{
        printf("Memoria mapeada\n");
    }

    for(int i=0;i<MAX_ARRIVALS;i++){
        (shm_arrivals+i)->init=-1; //inicializar o init a -1 (ficar vazio)
    }

    shmid_dep = shmget(IPC_PRIVATE, sizeof(struct departure)*MAX_THREADS, IPC_CREAT | 0766);
    if(shmid_dep < 0){
        printf("ERRO na criacao da memoria2\n");
        exit(-1);
    }

    shm_departures = (struct departure*)shmat(shmid, NULL, 0);
    if(shm_departures == NULL){
        printf("ERRO no mapeamento da memoria\n");
        exit(-1);
    }
    else{
        printf("Memoria mapeada\n");
    }
}

void sigint(int signum){
    wait(NULL);
    printf("\n######### Departures #########\n");
    print_departures(header_departures);
    printf("######### ARRIVALS #########\n");
    print_arrivals(header_arrivals);
    //limpar memoria partilhada
    shmctl(shmid, IPC_RMID, NULL);
    shmctl(shmid_arr, IPC_RMID, NULL);
    shmctl(shmid_dep,IPC_RMID, NULL);
    printf("Limpou a memoria\n");

    //limpar mutexx
    sem_close(mutexx);
    sem_unlink("MUTEXX");
    sem_close(mutexlog);
    sem_unlink("MUTEXLOG");
    sem_close(mutexmsg);
    sem_unlink("MUTEXMSG");

    //limpar threads
    /*
    for(int i=0; i<(qtd_max_partidas+qtd_max_chegadas); i++){
        pthread_cancel(thread_voos[i]);
    }*/
    exit(0);
}

void cria_mensage_queue(){
  if ((message_queue = msgget(IPC_PRIVATE, IPC_CREAT | 0700))==-1){
      printf("Erro ao criar a message queue!\n");
      exit(0);
  }else printf("Message queue criada!\n");
}

void* msgq(void* arg){
    voos_send_msg msg;
    int i;
    while(1){
        if(msgrcv(message_queue, &msg, sizeof(msg)-sizeof(long), 1, 0) != -1){
            printf("lido da MQ(%d takeoff, %d eta, %d fuel)\n", msg.takeoff, msg.eta, msg.fuel);
        }
        if(msg.fuel != -1){
            for(i=0;i<MAX_ARRIVALS;i++){
                if((shm_arrivals+i)->slot == 0){
                    (shm_arrivals+i)->slot = 1;//"ocupar" o slot da shm
                    (shm_arrivals+i)->fuel=msg.fuel;
                    (shm_arrivals+i)->prioridade=0;
                    msg.mtype = 1;
                    msg.ids=(int)(shm_arrivals+i);
                    strcpy((shm_arrivals+i)->code,msg.code);
                    add_fila_arrivals(shm_arrivals);
                    // ENVIAR PRA MSQ ->  shm_voos+i;
                    if( (msgsnd(message_queue, &msg, sizeof(msg)-sizeof(long), 0)) == -1){
                        printf("Erro a responder da torre");
                        perror(0);
                    }
                    break;
                }
                else{
                    i++;
                }
            }
        }
        if(msg.fuel == -1){
            for(i=0;i<MAX_DEPARTURES;i++){
                if((shm_departures+i)->slot == 0){
                    (shm_departures+i)->slot = 1;//"ocupar" o slot da shm
                    (shm_departures+i)->takeoff=msg.takeoff;
                    msg.mtype = 1;
                    strcpy((shm_departures+i)->code,msg.code);
                    // ENVIAR PRA MSQ ->  shm_voos+i;
                    msg.ids=(int)(shm_arrivals+i);
                    if( (msgsnd(message_queue, &msg, sizeof(msg)-sizeof(long), 0)) == -1){
                        printf("Erro a responder da torre");
                        perror(0);
                    }
                    break;
                }
                else{
                    i++;
                }
            }
        }
        sleep(1);
    }
}

void redireciona(char* code,int i){
    char str[1000];
    sprintf(str,"%s LEAVING TO OTHER AIRPORT => FUEL = 0\n",code);
    ficheiro_log(str);
    (shm_arrivals+i)->slot=0; //libertar o espaço da shm
    //remove_voo(code);
}

void *thread_fuel(void* arg){
    int i;
    while(1){
        for(i=0;i<MAX_ARRIVALS;i++){
            if((shm_arrivals+i)->slot != 0){
                //aumentar a prioridade
                if((shm_arrivals+i)->fuel <= (4+ (shm_arrivals+i)->eta + duracao_aterragem)){
                    (shm_arrivals+i)->prioridade++;
                }
                if((shm_arrivals+i)->fuel==0){
                    pthread_mutex_lock(&mutex_fuel);
                    redireciona((shm_arrivals+i)->code,i);
                    pthread_mutex_unlock(&mutex_fuel);
                }
                //printf("code: %s fuel: %d\n",(shm_arrivals+i)->code,(shm_arrivals+i)->fuel);          
                (shm_arrivals+i)->fuel--;
                //print_arr_shm(shm_arrivals);
            }
        }
        sleep(1);
    }
}


void torre_de_controlo(){
    pthread_t fuel_thread;
    pthread_t msgq_thread;
    int thread_fuel_id;
    int thread_msgq_id;
    fila_arrivals=malloc(sizeof(struct arrival));
    fila_arrivals->next=NULL;
    fila_departures=malloc(sizeof(struct departure));
    fila_departures->next=NULL;
    //char str[1000];
    //printf("PID da torre de controlo: %d\n",getpid());
    //sprintf(str,"PID da torre de controlo: %d\n",getpid());
    //ficheiro_log(str);

    //Thread que atualiza o combustível
    pthread_create(&fuel_thread,NULL,thread_fuel,&thread_fuel_id);

    //Thread que lê a msg queue e devolve ao voo o seu espaço na shared memory
    pthread_create(&msgq_thread,NULL,msgq,&thread_msgq_id);
    
    pthread_join(msgq_thread,NULL);
    pthread_join(fuel_thread,NULL);
}


void ficheiro_log(char* mensagem){//manter o ficheiro aberto, e mecanismo de sincronismo
    sem_wait(mutexlog);
    time_t horas;
    struct tm* time_struct;
    mensagem[strlen(mensagem)-1]='\0';
    time(&horas);
    time_struct = localtime(&horas);
    fprintf(f_log,"%d:%d:%d %s\n",time_struct->tm_hour,time_struct->tm_min,time_struct->tm_sec,mensagem);
    printf("%d:%d:%d %s\n",time_struct->tm_hour,time_struct->tm_min,time_struct->tm_sec,mensagem);
    sem_post(mutexlog);
}

int inicia(){
    j=0;
    pthread_t pipe_thread;
    pthread_t time_thread;
    pthread_t tempo_atual_thread;
    pid_t torre_controlo;
    //j=0;

    int pipe_thread_id;
    int time_thread_id;
    int tempo_atual_thread_id;
    char str[1000];
    signal(SIGINT, sigint);
    time_init = time(NULL);
    //printf("time: %d",time_init);
    header_arrivals=malloc(sizeof(struct arrival));
    header_arrivals->next=NULL;
    header_departures=malloc(sizeof(struct departure));
    header_departures->next=NULL;
    header_voos=malloc(sizeof(struct voo));
    header_voos->next=NULL;
    f_log=fopen("log.txt","a");

    sem_unlink("MUTEXX");
    mutexx=sem_open("MUTEXX",O_CREAT|O_EXCL,0700,1);

    sem_unlink("MUTEXLOG");
    mutexlog=sem_open("MUTEXLOG",O_CREAT|O_EXCL,0700,1);

    sem_unlink("MUTEXMSG");
    mutexx=sem_open("MUTEXMSG",O_CREAT|O_EXCL,0700,1);

    read_config();
    qtd_max_voos=qtd_max_chegadas+qtd_max_partidas;
    //print_struct();

    //cria a memoria partilhada
    cria_memoria();

    //MQ
    cria_mensage_queue();


    torre_controlo=fork();

    if(torre_controlo==0){
        printf("PID da torre de controlo: %d\n",getpid());
        sprintf(str,"PID da torre de controlo: %d \n",getpid());
        ficheiro_log(str);
        //execl("torre","torre");
        torre_de_controlo();
    }




    //PIPE
    cria_pipe();
    //para escrever no pipe abrir outro terminal e escrever echo "cena">input_pipe

    //THREAD que lê o pipe
    pthread_create(&pipe_thread,NULL,thread_leitura,(void*)&pipe_thread_id);

    //THREAD que crias as outras threads
    pthread_create(&time_thread,NULL,thread_cria_voos,(void*)&time_thread_id);

    //THREAD para mostrar o tempo atual ya dps apaga-se
    pthread_create(&tempo_atual_thread,NULL,thread_controlo,&tempo_atual_thread_id);

    //Inicia mutex
    if (pthread_mutex_init(&mutex, NULL) != 0){
        printf("Erro ao inicializar o mutex\n");
        return -1;
    }

    pthread_join(pipe_thread,NULL);
    return 0;
}


int main() {
    
    inicia();
    fclose(f_log);
    wait(NULL);
    return 0;
}
