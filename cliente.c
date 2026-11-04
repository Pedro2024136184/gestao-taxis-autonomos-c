/**
 * Gestão de Frota de Táxis Autónomos - Módulo Cliente (Versão Melhorada)
 * Unidade Curricular: Sistemas Operativos (SO) - Ano 2025
 * Autor: Pedro Costa (2024136184)
 */

#include "common.h"
#include "utils.h"

static char fifo_nome_cliente[64];
static int fdp_cliente = -1;
static int fdp_controlador = -1;
static pid_t pid_veiculo_alocado = -1;
static volatile sig_atomic_t g_executando = 1;
static pthread_mutex_t trinco_cli = PTHREAD_MUTEX_INITIALIZER;

void limpar_recursos(void) {
    g_executando = 0;
    if (fdp_cliente != -1) {
        close(fdp_cliente);
        fdp_cliente = -1;
    }
    if (fdp_controlador != -1) {
        close(fdp_controlador);
        fdp_controlador = -1;
    }
    if (strlen(fifo_nome_cliente) > 0) {
        unlink(fifo_nome_cliente);
    }
    printf("\n[CLIENTE] Recursos libertados e sessao terminada.\n");
}

void tratar_sinal(int sinal) {
    (void)sinal;
    limpar_recursos();
    exit(0);
}

void* recebe_respostas(void* arg) {
    (void)arg;
    Mensagem resp;
    while (g_executando) {
        ssize_t bytes = read(fdp_cliente, &resp, sizeof(Mensagem));
        if (bytes > 0) {
            if (resp.tipo == MSG_INFO_VEICULO) {
                printf("\n[NOTIFICACAO SISTEMA] %s\nCliente> ", resp.dados);
                fflush(stdout);

                if (strstr(resp.dados, "chegou") != NULL || strstr(resp.dados, "Cheguei") != NULL) {
                    pthread_mutex_lock(&trinco_cli);
                    pid_veiculo_alocado = resp.pid_origem;
                    pthread_mutex_unlock(&trinco_cli);
                }

                if (strstr(resp.dados, "CONCLUIDA") != NULL || strstr(resp.dados, "CANCELADA") != NULL) {
                    pthread_mutex_lock(&trinco_cli);
                    pid_veiculo_alocado = -1;
                    pthread_mutex_unlock(&trinco_cli);
                }
            } else if (resp.tipo == MSG_ENCERRAR) {
                printf("\n[ALERTA] O controlador foi encerrado pelo administrador. A fechar...\n");
                limpar_recursos();
                exit(0);
            }
        } else if (bytes == 0) {
            // EOF detectado
            usleep(200000);
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <username>\n", argv[0]);
        return 1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = tratar_sinal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    snprintf(fifo_nome_cliente, sizeof(fifo_nome_cliente), FIFO_CLIENTE_FMT, (int)getpid());
    unlink(fifo_nome_cliente);
    if (mkfifo(fifo_nome_cliente, 0666) == -1) {
        perror("[CLIENTE] Erro ao criar FIFO proprio");
        return 1;
    }

    // Abre FIFO do cliente em modo Leitura/Escrita para evitar bloqueios em EOF
    fdp_cliente = open(fifo_nome_cliente, O_RDWR);
    if (fdp_cliente == -1) {
        perror("[CLIENTE] Erro ao abrir FIFO proprio");
        unlink(fifo_nome_cliente);
        return 1;
    }

    // Conectar ao controlador
    fdp_controlador = open(FIFO_CONTROLADOR, O_WRONLY);
    if (fdp_controlador == -1) {
        printf("[ERRO] O controlador nao esta em execucao. Inicie './controlador' primeiro.\n");
        limpar_recursos();
        return 1;
    }

    // Autenticação / Login
    Mensagem msg;
    memset(&msg, 0, sizeof(msg));
    msg.tipo = MSG_LOGIN;
    msg.pid_origem = getpid();
    strncpy(msg.username, argv[1], sizeof(msg.username) - 1);

    write(fdp_controlador, &msg, sizeof(Mensagem));

    Mensagem resposta;
    read(fdp_cliente, &resposta, sizeof(Mensagem));
    if (resposta.tipo != MSG_LOGIN_RESP || strcmp(resposta.dados, "OK") != 0) {
        printf("[ERRO] Falha no login: %s\n", resposta.dados);
        limpar_recursos();
        return 1;
    }

    printf("========================================================\n");
    printf("   Bem-vindo ao Sistema de Taxis Autonomos, %s!         \n", argv[1]);
    printf("========================================================\n");
    printf("Comandos disponiveis:\n");
    printf("  agendar <origem> <distancia_km> <hora_agendada>\n");
    printf("  cancelar <id_viagem>\n");
    printf("  consultar\n");
    printf("  sair\n");
    printf("========================================================\n");

    pthread_t t_leitor;
    pthread_create(&t_leitor, NULL, recebe_respostas, NULL);

    char linha[256];
    while (g_executando) {
        printf("Cliente> ");
        fflush(stdout);
        if (!fgets(linha, sizeof(linha), stdin)) break;
        linha[strcspn(linha, "\n")] = 0;
        if (strlen(linha) == 0) continue;

        char cmd[32];
        sscanf(linha, "%31s", cmd);

        if (strcmp(cmd, "sair") == 0) {
            break;
        } else if (strcmp(cmd, "agendar") == 0) {
            char orig[TAM_MAX_LOCAL];
            int dist = 0, hora = 0;
            if (sscanf(linha, "%*s %49s %d %d", orig, &dist, &hora) >= 3) {
                memset(&msg, 0, sizeof(msg));
                msg.tipo = MSG_AGENDAR_VIAGEM;
                msg.pid_origem = getpid();
                strncpy(msg.username, argv[1], sizeof(msg.username) - 1);
                strncpy(msg.local_origem, orig, sizeof(msg.local_origem) - 1);
                msg.distancia = dist;
                msg.hora_agendada = hora;
                write(fdp_controlador, &msg, sizeof(Mensagem));
            } else {
                printf("[USO] agendar <origem> <distancia_km> <hora_agendada>\n");
            }
        } else if (strcmp(cmd, "cancelar") == 0) {
            int id = -1;
            if (sscanf(linha, "%*s %d", &id) == 1) {
                memset(&msg, 0, sizeof(msg));
                msg.tipo = MSG_CANCELAR_VIAGEM;
                msg.pid_origem = getpid();
                strncpy(msg.username, argv[1], sizeof(msg.username) - 1);
                msg.id_viagem = id;
                write(fdp_controlador, &msg, sizeof(Mensagem));
            } else {
                printf("[USO] cancelar <id_viagem>\n");
            }
        } else if (strcmp(cmd, "consultar") == 0) {
            memset(&msg, 0, sizeof(msg));
            msg.tipo = MSG_CONSULTAR_VIAGENS;
            msg.pid_origem = getpid();
            strncpy(msg.username, argv[1], sizeof(msg.username) - 1);
            write(fdp_controlador, &msg, sizeof(Mensagem));
        } else {
            printf("[ERRO] Comando desconhecido: '%s'\n", cmd);
        }
    }

    limpar_recursos();
    return 0;
}
