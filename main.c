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

void inicia(){
    struct config* config=malloc(sizeof(struct config));
    config=read_config(config);
    //print_struct(config);
    pid_t processo;
    processo=fork();
    if(processo==0){
        printf("ID da torre de controlo: %d\n",pid());
    }

    else{
        printf("Nao sou a torre de controlo!\n");
    }
}


int main() {
    inicia();
    return 0;
}