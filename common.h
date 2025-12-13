#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

/* Limites do sistema */
#define MAX_UTILIZADORES    30
#define MAX_VEICULOS        10
#define TAM_MAX_USERNAME    20
#define TAM_MAX_LOCAL       50
#define TAM_BUFFER_MSG      256

/* Named pipes (FIFOs) para comunicação entre processos */
#define FIFO_CONTROLADOR    "fifo_controlador"
#define FIFO_CLIENTE_FMT    "fifo_cliente_%d"
#define FIFO_VEICULO_FMT    "fifo_veiculo_%d"

/* Tipos de mensagens do sistema */
typedef enum {
    MSG_LOGIN,              /* Autenticação de utilizador */
    MSG_LOGIN_RESP,         /* Resposta de login */
    MSG_AGENDAR_VIAGEM,     /* Solicitar nova viagem */
    MSG_CANCELAR_VIAGEM,    /* Cancelar viagem existente */
    MSG_CONSULTAR_VIAGENS,  /* Consultar viagens agendadas */
    MSG_PEDIDO_ENTRAR,      /* Veículo pronto para entrar em serviço */
    MSG_PEDIDO_SAIR,        /* Veículo sair de serviço */
    MSG_INFO_VEICULO,       /* Informações do veículo */
    MSG_ENCERRAR            /* Encerramento do sistema */
} TipoMsg;

/* Estrutura de mensagem para comunicação entre processos */
typedef struct {
    TipoMsg tipo;                       /* Tipo de mensagem */
    pid_t pid_origem;                   /* PID do processo origem */
    char username[TAM_MAX_USERNAME];    /* Utilizador */
    char dados[TAM_BUFFER_MSG];         /* Dados genéricos */
    int id_viagem;                      /* Identificador da viagem */
    int hora_agendada;                  /* Hora da viagem */
    int distancia;                      /* Distância em km */
    char local_origem[TAM_MAX_LOCAL];   /* Local de partida */
    char local_destino[TAM_MAX_LOCAL];  /* Local de chegada */
} Mensagem;

#endif
