#include "common.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>

// Variáveis globais para limpeza no signal handler
char fifo_meu_nome[50];
int fd_meu_fifo = -1;

void limpar_e_sair() {
    if (fd_meu_fifo != -1) close(fd_meu_fifo);
    unlink(fifo_meu_nome);
    exit(0);
}

void trata_sinal(int sinal) {
    if (sinal == SIGUSR1) {
        // Enunciado: "Caso receba o sinal SIGUSR1 deve cancelar o serviço... informando o cliente"
        printf("CANCELADO: Ordem do Controlador (SIGUSR1)\n");
        fflush(stdout);
        limpar_e_sair();
    }
    if (sinal == SIGINT) {
        limpar_e_sair();
    }
}

int main(int argc, char *argv[]) {
    // argv[1]=ID, argv[2]=Origem, argv[3]=Dist, argv[4]=PID_CLIENTE
    if (argc < 5) return 1;

    int id_viagem = atoi(argv[1]);
    int dist_total = atoi(argv[3]);
    int pid_cliente = atoi(argv[4]);
    char destino[TAM_MAX_LOCAL] = "Indefinido";

    signal(SIGUSR1, trata_sinal);
    signal(SIGINT, trata_sinal);

    // 1. CRIA O SEU PRÓPRIO FIFO (Para receber 'entrar' e 'sair' do cliente)
    sprintf(fifo_meu_nome, FIFO_VEICULO_FMT, getpid());
    if (mkfifo(fifo_meu_nome, 0666) == -1) {
        perror("mkfifo veiculo");
        return 1;
    }

    // 2. CONTACTAR O CLIENTE (Dizer que chegou e passar o seu PID para o cliente responder)
    char fifo_cliente[50];
    sprintf(fifo_cliente, FIFO_CLIENTE_FMT, pid_cliente);
    int fd_cli = open(fifo_cliente, O_WRONLY);
    
    if (fd_cli != -1) {
        Mensagem msg_envio;
        memset(&msg_envio, 0, sizeof(Mensagem));
        msg_envio.tipo = MSG_INFO_VEICULO;
        msg_envio.pid_origem = getpid(); // O meu PID para o cliente saber para onde mandar o 'entrar'
        sprintf(msg_envio.dados, "Veiculo %d chegou! Escreva 'entrar <destino>'", id_viagem);
        
        write(fd_cli, &msg_envio, sizeof(Mensagem));
        close(fd_cli);
    } else {
        printf("ERRO: Nao consegui avisar cliente.\n"); // Vai para o controlador via stdout
        fflush(stdout);
        limpar_e_sair();
    }

    // 3. AGUARDAR O CLIENTE ENTRAR
    // O veiculo fica bloqueado aqui até o cliente mandar "entrar"
    fd_meu_fifo = open(fifo_meu_nome, O_RDWR); // RDWR para não bloquear logo no open
    Mensagem msg_recebida;
    int cliente_no_carro = 0;

    while (!cliente_no_carro) {
        if (read(fd_meu_fifo, &msg_recebida, sizeof(Mensagem)) > 0) {
            if (msg_recebida.tipo == MSG_PEDIDO_ENTRAR) {
                cliente_no_carro = 1;
                strcpy(destino, msg_recebida.local_destino);
                // Informar Controlador que viagem começou (Telemetria)
                printf("INICIO: Cliente entrou. Destino: %s\n", destino);
                fflush(stdout);
            }
        }
    }

    // 4. SIMULAÇÃO DA VIAGEM
    // O enunciado diz: velocidade 1km / unidade de tempo
    // Reportar a cada 10%
    
    // Configurar leitura não bloqueante para verificar comando "sair" durante o loop
    int flags = fcntl(fd_meu_fifo, F_GETFL, 0);
    fcntl(fd_meu_fifo, F_SETFL, flags | O_NONBLOCK);

    int tempo_total = dist_total; // 1 seg por km
    int tempo_passado = 0;
    
    for (int t = 1; t <= tempo_total; t++) {
        sleep(1); 
        tempo_passado++;

        // A. Verificar se cliente quer SAIR a meio
        if (read(fd_meu_fifo, &msg_recebida, sizeof(Mensagem)) > 0) {
            if (msg_recebida.tipo == MSG_PEDIDO_SAIR) {
                printf("CONCLUIDO: Cliente saiu a meio.\n");
                fflush(stdout);
                limpar_e_sair();
            }
        }

        // B. Calcular percentagem
        int perc = (tempo_passado * 100) / tempo_total;
        
        // O enunciado pede reportar a cada 10%
        if (perc % 10 == 0) {
            printf("DISTANCIA: %d\n", perc); // Formato simples para o controlador ler
            fflush(stdout);
        }
    }

    // 5. TERMINAR
    printf("CONCLUIDO: Chegada ao destino %s.\n", destino);
    fflush(stdout);
    
    limpar_e_sair();
    return 0;
}

