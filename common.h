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

#define MAX_UTILIZADORES    30
#define MAX_VEICULOS        10
#define TAM_MAX_USERNAME    20
#define TAM_MAX_LOCAL       50
#define TAM_BUFFER_MSG      256

#define FIFO_CONTROLADOR    "fifo_controlador"
#define FIFO_CLIENTE_FMT    "fifo_cliente_%d"
#define FIFO_VEICULO_FMT    "fifo_veiculo_%d"

typedef enum {
    MSG_LOGIN,
    MSG_LOGIN_RESP,
    MSG_AGENDAR_VIAGEM,
    MSG_CANCELAR_VIAGEM,
    MSG_CONSULTAR_VIAGENS,
    MSG_PEDIDO_ENTRAR,
    MSG_PEDIDO_SAIR,
    MSG_INFO_VEICULO,
    MSG_ENCERRAR
} TipoMsg;

typedef struct {
    TipoMsg tipo;
    pid_t pid_origem;
    char username[TAM_MAX_USERNAME];
    char dados[TAM_BUFFER_MSG];
    int id_viagem;
    int hora_agendada;
    int distancia;
    char local_origem[TAM_MAX_LOCAL];
    char local_destino[TAM_MAX_LOCAL];
} Mensagem;

#endif
