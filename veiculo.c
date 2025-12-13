#include "common.h"
#include "utils.h"

char fifo_meu[50];
int fd_meu = -1;
int pid_cliente = 0;
double distancia_feita = 0;

// Função de tratamento de sinal para encerrar o veículo
void terminar_veiculo(int sinal) {
    (void)sinal;
    // Notifica o cliente sobre o cancelamento
    if (pid_cliente > 0) {
        char fifo_cli[50];
        sprintf(fifo_cli, FIFO_CLIENTE_FMT, pid_cliente);
        int fd = open(fifo_cli, O_WRONLY | O_NONBLOCK);
        if (fd != -1) {
            Mensagem m;
            m.tipo = MSG_INFO_VEICULO;
            strcpy(m.dados, "Viagem CANCELADA pelo sistema.");
            write(fd, &m, sizeof(Mensagem));
            close(fd);
        }
    }
    // Imprime quilometragem final e limpa recursos
    printf("FIM_SERVICO_KMS: %.1f\n", distancia_feita);
    fflush(stdout);

    if (fd_meu != -1) close(fd_meu);
    unlink(fifo_meu);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc < 5) return 1;
    // Registra handlers para sinais de encerramento
    signal(SIGINT, terminar_veiculo);
    signal(SIGUSR1, terminar_veiculo);

    int id_viagem = atoi(argv[1]);
    int distancia = atoi(argv[3]);
    pid_cliente = atoi(argv[4]);

    // Cria FIFO do veículo
    sprintf(fifo_meu, FIFO_VEICULO_FMT, getpid());
    if (mkfifo(fifo_meu, 0666) == -1) {
        perror("mkfifo veiculo");
        return 1;
    }

    printf("Veiculo %d (ID Viagem: %d) pronto.\n", getpid(), id_viagem);
    fflush(stdout);

    // Notifica cliente que veículo chegou
    char fifo_cli_nome[50];
    sprintf(fifo_cli_nome, FIFO_CLIENTE_FMT, pid_cliente);
    int fd_cli = open(fifo_cli_nome, O_WRONLY);
    if (fd_cli != -1) {
        Mensagem m;
        m.tipo = MSG_INFO_VEICULO;
        m.pid_origem = getpid();
        strcpy(m.dados, "O veiculo chegou! Escreva: entrar <destino>");
        write(fd_cli, &m, sizeof(Mensagem));
        close(fd_cli);
    }

    // Aguarda cliente entrar no veículo
    fd_meu = open(fifo_meu, O_RDWR);
    Mensagem msg_in;
    int a_bordo = 0;

    while (read(fd_meu, &msg_in, sizeof(Mensagem)) > 0) {
        if (msg_in.tipo == MSG_PEDIDO_ENTRAR) {
            a_bordo = 1;
            printf("CLIENTE_EMBARCOU\n");
            fflush(stdout);
            break;
        }
    }

    // Simula a viagem em 10 passos
    if (a_bordo == 1) {
        double passo_dist = distancia * 0.1;
        double tempo_passo = passo_dist;

        // Define FIFO como não-bloqueante para leitura
        int flags = fcntl(fd_meu, F_GETFL, 0);
        fcntl(fd_meu, F_SETFL, flags | O_NONBLOCK);

        for (int i = 1; i <= 10; i++) {
            usleep((useconds_t)(tempo_passo * 1000000));
            distancia_feita += passo_dist;

            printf("ANDAMENTO %.1f kms\n", distancia_feita);
            fflush(stdout);

            // Envia progresso ao cliente
            sprintf(fifo_cli_nome, FIFO_CLIENTE_FMT, pid_cliente);
            int fd_cli = open(fifo_cli_nome, O_WRONLY | O_NONBLOCK);
            if (fd_cli != -1) {
                Mensagem relatorio;
                relatorio.tipo = MSG_INFO_VEICULO;
                sprintf(relatorio.dados, "[VIAGEM] Progresso: %d%%", i*10);
                write(fd_cli, &relatorio, sizeof(Mensagem));
                close(fd_cli);
            }

            // Verifica se cliente quer sair antes do fim
            if (read(fd_meu, &msg_in, sizeof(Mensagem)) > 0) {
                if (msg_in.tipo == MSG_PEDIDO_SAIR) {
                    printf("CLIENTE_SAIU_ANTECIPADAMENTE\n");
                    fflush(stdout);
                    terminar_veiculo(0);
                }
            }
        }

        // Notifica conclusão da viagem
        fd_cli = open(fifo_cli_nome, O_WRONLY | O_NONBLOCK);
        if (fd_cli != -1) {
            Mensagem m;
            m.tipo = MSG_INFO_VEICULO;
            strcpy(m.dados, "VIAGEM CONCLUIDA");
            write(fd_cli, &m, sizeof(Mensagem));
            close(fd_cli);
        }

        printf("VIAGEM_CONCLUIDA\n");
        fflush(stdout);
        terminar_veiculo(0);
    }
    return 0;
}
