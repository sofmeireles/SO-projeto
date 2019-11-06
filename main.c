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
#define PIPE_NAME "input_pipe"

struct config{
    int unidade;
    int duracao_descolagem;
    int duracao_aterragem;
    int int_descolagem;
    int int_aterragem;
    int hold_max;
    int hold_min;
    int qtd_max_partidas;
    int qtd_max_chegadas;
};

void print_struct(struct config* conf){
    printf("ua: %d\n",conf->unidade);
    printf("dd: %d, id: %d\n",conf->duracao_descolagem,conf->int_descolagem);
    printf("da: %d, ia: %d\n",conf->duracao_aterragem,conf->int_aterragem);
    printf("hmin: %d, holmax: %d\n",conf->hold_min,conf->hold_max);
    printf("qtdp: %d\n",conf->qtd_max_partidas);
    printf("qtdc: %d\n",conf->qtd_max_chegadas);
}


struct config* read_config(struct config* conf){
    FILE*f=fopen("config.txt","r");
    fscanf(f,"%d\n%d, %d\n%d, %d\n%d, %d\n%d\n%d",&conf->unidade,&conf->duracao_descolagem,&conf->int_descolagem,&conf->duracao_aterragem,&conf->int_aterragem,&conf->hold_min,&conf->hold_max,&conf->qtd_max_partidas,&conf->qtd_max_chegadas);
    return conf;
}

int inicia(){
    int message_queue;
    struct config* config=malloc(sizeof(struct config));
    config=read_config(config);
    //print_struct(config);
    pid_t processo;
    int fd;
    char comando[1000];
    
    //PIPE
    if ((mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0600)<0) && (errno!= EEXIST)) {
        perror("Erro ao criar o pipe: ");
        exit(0);
    }else printf("Pipe criado!\n");

    if ((fd = open(PIPE_NAME, O_RDONLY, O_WRONLY)) < 0) { //ler do pipe
        perror("Erro ao ler o pipe: ");
        exit(0);
    }
    else{
        read(fd,comando,1000);
        printf("pipe lido: %s\n",comando);
    }
    //para escrever no pipe abrir outro terminal e escrever echo "cena">input_pipe


    //MQ
    if ((message_queue = msgget(IPC_PRIVATE, IPC_CREAT | 0700))==-1){
        printf("Erro ao criar a message queue!\n");
        return -1;
    }else printf("Message queue criada!\n");

    processo=fork();
    if(processo==0){
        printf("PID da torre de controlo: %d\n",getpid());
        execl("torre","torre");
    }

    else{
        printf("PID do gestor de simulacao: %d\n",getpid());
    }
    return 0;
}

void ficheiro_log(char* mensagem){
    FILE *f=fopen("log.txt","a");
    time_t horas;
    struct tm* time_struct;
    
    time(&horas);
    time_struct = localtime(&horas);
    fprintf(f,"%d:%d:%d %s\n",time_struct->tm_hour,time_struct->tm_min,time_struct->tm_sec,mensagem);
    printf("%d:%d:%d %s\n",time_struct->tm_hour,time_struct->tm_min,time_struct->tm_sec,mensagem);
}


int main() {
    inicia();
    wait(NULL);
    return 0;
}