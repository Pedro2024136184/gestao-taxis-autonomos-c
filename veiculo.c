#include "common.h"
#include "utils.h"

// Globais para limpeza no sinal
char fifo_meu[50];
int fd_meu = -1;
int pid_cliente = 0;
double distancia_feita = 0; 

void terminar_veiculo(int sinal) {
    (void)sinal;
    // Avisar cliente se possível
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
    printf("FIM_SERVICO_KMS: %.1f\n", distancia_feita); 
    fflush(stdout);

    if (fd_meu != -1) close(fd_meu);
    unlink(fifo_meu);
    exit(0);
}

int main(int argc, char *argv[]) {
    // argv[1]=ID, argv[2]=Origem, argv[3]=Dist, argv[4]=PID_CLIENTE
    if (argc < 5) return 1;

    signal(SIGINT, terminar_veiculo);
    signal(SIGUSR1, terminar_veiculo); // Tratamento do cancelamento

    int id_viagem = atoi(argv[1]);
    int distancia = atoi(argv[3]);
    pid_cliente = atoi(argv[4]);

    // 1. Criar FIFO do Veículo (para receber 'entrar'/'sair')
    sprintf(fifo_meu, FIFO_VEICULO_FMT, getpid());
    if (mkfifo(fifo_meu, 0666) == -1) {
        perror("mkfifo veiculo");
        return 1;
    }

    // 2. Avisar Controlador (Inicio) via STDOUT
    printf("Veiculo %d (ID Viagem: %d) pronto.\n", getpid(), id_viagem);
    fflush(stdout);

    // 3. Avisar Cliente "Cheguei" DIRETAMENTE
    char fifo_cli_nome[50];
    sprintf(fifo_cli_nome, FIFO_CLIENTE_FMT, pid_cliente);
    int fd_cli = open(fifo_cli_nome, O_WRONLY);
    if (fd_cli != -1) {
        Mensagem m;
        m.tipo = MSG_INFO_VEICULO;
        m.pid_origem = getpid(); // Envia o PID para o cliente saber onde responder
        strcpy(m.dados, "O veiculo chegou! Escreva: entrar <destino>");
        write(fd_cli, &m, sizeof(Mensagem));
        close(fd_cli);
    }

    // 4. Aguardar comando "entrar"
    fd_meu = open(fifo_meu, O_RDWR); // Bloqueia até abrir
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

    // 5. Simulação da Viagem
    // Velocidade = 1km/s. Se distancia = 10km, demora 10s.
    // Atualiza a cada 10%.
    double passo_dist = distancia * 0.1;
    double tempo_passo = passo_dist; // 1km = 1s
    double percorrido = 0;

    // Configurar leitura não bloqueante para comando 'sair'
    int flags = fcntl(fd_meu, F_GETFL, 0);
    fcntl(fd_meu, F_SETFL, flags | O_NONBLOCK);

    for (int i = 1; i <= 10; i++) {
        // Dorme o tempo necessário para fazer 10% da viagem
        usleep((useconds_t)(tempo_passo * 1000000)); 
        
        percorrido += passo_dist;
        
        // Reportar ao controlador
        printf("ANDAMENTO %.1f kms (%d%%)\n", percorrido, i*10);
        fflush(stdout);

        char fifo_cli_nome[50];
        sprintf(fifo_cli_nome, FIFO_CLIENTE_FMT, pid_cliente);
        int fd_cli = open(fifo_cli_nome, O_WRONLY | O_NONBLOCK);

        if (fd_cli != -1) {
            Mensagem relatorio;
            relatorio.tipo = MSG_INFO_VEICULO;
            // Esta é a mensagem que o cliente vai ver na sua thread de leitura:
            sprintf(relatorio.dados, "[VIAGEM] Progresso: %d%%", i*10); 
            write(fd_cli, &relatorio, sizeof(Mensagem));
            close(fd_cli);
        }

        // Verificar se cliente mandou "sair"
        if (read(fd_meu, &msg_in, sizeof(Mensagem)) > 0) {
            if (msg_in.tipo == MSG_PEDIDO_SAIR) {
                printf("CLIENTE_SAIU_ANTECIPADAMENTE\n");
                fflush(stdout);
                terminar_veiculo(0);
            }
        }
    }

    printf("VIAGEM_CONCLUIDA\n");
    fflush(stdout);
    terminar_veiculo(0);
    return 0;
}