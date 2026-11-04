/**
 * Gestão de Frota de Táxis Autónomos - Módulo Veículo (Versão Melhorada)
 * Unidade Curricular: Sistemas Operativos (SO) - Ano 2025
 * Autor: Pedro Costa (2024136184)
 */

#include "common.h"
#include "utils.h"

static char fifo_meu[50];
static int fd_meu = -1;
static pid_t pid_cliente = 0;
static double distancia_feita = 0;

void terminar_veiculo(int sinal) {
    (void)sinal;
    if (pid_cliente > 0) {
        char fifo_cli[50];
        snprintf(fifo_cli, sizeof(fifo_cli), FIFO_CLIENTE_FMT, (int)pid_cliente);
        int fd = open(fifo_cli, O_WRONLY | O_NONBLOCK);
        if (fd != -1) {
            Mensagem m;
            memset(&m, 0, sizeof(m));
            m.tipo = MSG_INFO_VEICULO;
            strncpy(m.dados, "Viagem CANCELADA ou terminada pelo sistema.", sizeof(m.dados) - 1);
            write(fd, &m, sizeof(Mensagem));
            close(fd);
        }
    }

    fprintf(stderr, "[VEICULO %d] A terminar... Quilometragem final: %.1f km\n", getpid(), distancia_feita);
    printf("FIM_SERVICO_KMS: %.1f\n", distancia_feita);
    fflush(stdout);

    if (fd_meu != -1) close(fd_meu);
    unlink(fifo_meu);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Uso: %s <id_viagem> <origem> <distancia> <pid_cliente>\n", argv[0]);
        return 1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = terminar_veiculo;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);

    int id_viagem = atoi(argv[1]);
    int distancia = atoi(argv[3]);
    pid_cliente = (pid_t)atoi(argv[4]);

    snprintf(fifo_meu, sizeof(fifo_meu), FIFO_VEICULO_FMT, (int)getpid());
    unlink(fifo_meu);
    if (mkfifo(fifo_meu, 0666) == -1) {
        perror("[VEICULO] Erro ao criar FIFO proprio");
        return 1;
    }

    fprintf(stderr, "[VEICULO %d] Pronto para iniciar servico ID=%d (Distancia: %d km)\n", getpid(), id_viagem, distancia);

    // Notificar cliente de chegada
    char fifo_cli_nome[50];
    snprintf(fifo_cli_nome, sizeof(fifo_cli_nome), FIFO_CLIENTE_FMT, (int)pid_cliente);
    int fd_cli = open(fifo_cli_nome, O_WRONLY | O_NONBLOCK);
    if (fd_cli != -1) {
        Mensagem m;
        memset(&m, 0, sizeof(m));
        m.tipo = MSG_INFO_VEICULO;
        m.pid_origem = getpid();
        snprintf(m.dados, sizeof(m.dados), "O Veiculo %d chegou ao local de partida para a viagem #%d.", getpid(), id_viagem);
        write(fd_cli, &m, sizeof(Mensagem));
        close(fd_cli);
    }

    // Abertura do FIFO do veículo em modo não bloqueante
    fd_meu = open(fifo_meu, O_RDONLY | O_NONBLOCK);

    // Simulação do trajeto quilómetro a quilómetro
    for (int k = 1; k <= distancia; k++) {
        sleep(1);
        distancia_feita = k;

        // Verificar mensagens de cancelamento no FIFO próprio
        if (fd_meu != -1) {
            Mensagem msg_in;
            if (read(fd_meu, &msg_in, sizeof(Mensagem)) > 0) {
                if (msg_in.tipo == MSG_CANCELAR_VIAGEM || msg_in.tipo == MSG_ENCERRAR) {
                    terminar_veiculo(SIGTERM);
                }
            }
        }

        // Progresso em stdout para o pipe anónimo do controlador
        float perc = ((float)k / (float)distancia) * 100.0f;
        printf("KMS: %.1f | PERC: %.1f%%\n", distancia_feita, perc);
        fflush(stdout);

        // Notificar cliente do progresso a meio caminho
        if (k == distancia / 2 && fd_cli != -1) {
            int fd_cli_mid = open(fifo_cli_nome, O_WRONLY | O_NONBLOCK);
            if (fd_cli_mid != -1) {
                Mensagem m_mid;
                memset(&m_mid, 0, sizeof(m_mid));
                m_mid.tipo = MSG_INFO_VEICULO;
                snprintf(m_mid.dados, sizeof(m_mid.dados), "Viagem #%d a 50%% concluida (%d/%d km).", id_viagem, k, distancia);
                write(fd_cli_mid, &m_mid, sizeof(Mensagem));
                close(fd_cli_mid);
            }
        }
    }

    // Viagem concluída com sucesso
    int fd_cli_fim = open(fifo_cli_nome, O_WRONLY | O_NONBLOCK);
    if (fd_cli_fim != -1) {
        Mensagem m_fim;
        memset(&m_fim, 0, sizeof(m_fim));
        m_fim.tipo = MSG_INFO_VEICULO;
        snprintf(m_fim.dados, sizeof(m_fim.dados), "Viagem #%d CONCLUIDA com sucesso! Destino alcancado.", id_viagem);
        write(fd_cli_fim, &m_fim, sizeof(Mensagem));
        close(fd_cli_fim);
    }

    printf("FIM_SERVICO_KMS: %.1f\n", distancia_feita);
    fflush(stdout);

    if (fd_meu != -1) close(fd_meu);
    unlink(fifo_meu);
    return 0;
}
