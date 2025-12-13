#include "common.h"
#include "utils.h"

char fifo_nome_cliente[50];
int fdp_cliente = -1; 
int fdp_controlador = -1; 
int pid_veiculo_alocado = -1; 
pthread_mutex_t trinco_cli = PTHREAD_MUTEX_INITIALIZER;

// Fecha FIFOs e remove arquivo do cliente
void limpar_recursos() {
    if (fdp_cliente != -1) close(fdp_cliente);
    if (fdp_controlador != -1) close(fdp_controlador);
    if (strlen(fifo_nome_cliente) > 0) unlink(fifo_nome_cliente);
    printf("\n[CLIENTE] Encerrado.\n");
}

// Trata sinais (SIGINT) e encerra
void tratar_sinal(int sinal) {
    (void)sinal;
    limpar_recursos();
    exit(0);
}

// Thread: recebe respostas do controlador e atualiza estado do veículo
void* recebe_respostas(void* arg) {
    (void)arg;
    Mensagem resp;
    while(1) {
        if (read(fdp_cliente, &resp, sizeof(Mensagem)) > 0) {
            if (resp.tipo == MSG_INFO_VEICULO) {
                printf("\n[INFO SISTEMA]\n%s\nCliente> ", resp.dados);
                fflush(stdout);

                // Veículo chegou
                if (pid_veiculo_alocado == -1 && 
                    (strstr(resp.dados, "Cheguei") != NULL || strstr(resp.dados, "chegou") != NULL)) {
                    pthread_mutex_lock(&trinco_cli);
                    pid_veiculo_alocado = resp.pid_origem;
                    pthread_mutex_unlock(&trinco_cli);
                }

                // Viagem concluída ou cancelada
                if (strstr(resp.dados, "CONCLUIDA") != NULL || strstr(resp.dados, "CANCELADA") != NULL) {
                    pthread_mutex_lock(&trinco_cli);
                    pid_veiculo_alocado = -1; 
                    pthread_mutex_unlock(&trinco_cli);
                }
            }
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    Mensagem msg;
    Mensagem resposta;

    if (argc != 2) { printf("Uso: %s <username>\n", argv[0]); return 1; }
    signal(SIGINT, tratar_sinal);

    // Cria FIFO do cliente
    sprintf(fifo_nome_cliente, FIFO_CLIENTE_FMT, getpid());
    if (mkfifo(fifo_nome_cliente, 0666) == -1) { perror("FIFO"); return 1; }

    // Conecta ao controlador
    fdp_controlador = open(FIFO_CONTROLADOR, O_WRONLY);
    if (fdp_controlador == -1) { 
        printf("Controlador offline.\n"); unlink(fifo_nome_cliente); return 1; 
    }

    // Envia login
    printf("[CLIENTE] Login: '%s'...\n", argv[1]);
    msg.tipo = MSG_LOGIN;
    msg.pid_origem = getpid();
    strcpy(msg.username, argv[1]);
    write(fdp_controlador, &msg, sizeof(Mensagem));

    // Recebe resposta de login
    fdp_cliente = open(fifo_nome_cliente, O_RDWR);
    if (read(fdp_cliente, &resposta, sizeof(Mensagem)) > 0) {
        if (resposta.tipo == MSG_LOGIN_RESP) {
            printf("[CONTROLADOR] %s\n", resposta.dados);
            if (strstr(resposta.dados, "Recusado") != NULL) {
                limpar_recursos(); return 1;
            }
        }
    }

    // Inicia thread de leitura de respostas
    pthread_t t_leitura;
    pthread_create(&t_leitura, NULL, recebe_respostas, NULL);

    printf(">> Comandos: agendar, cancelar, consultar, entrar, sair, terminar\n");
    char linha[TAM_BUFFER_MSG];
    char comando[20];

    // Loop principal: processa comandos do utilizador
    while (1) {
        printf("Cliente> ");
        if (fgets(linha, sizeof(linha), stdin) == NULL) break;
        linha[strcspn(linha, "\n")] = 0;
        if (strlen(linha) == 0) continue;

        sscanf(linha, "%s", comando);

        if (strcmp(comando, "terminar") == 0) {
            if (pid_veiculo_alocado != -1) {
                printf("[ERRO] Não pode sair durante viagem/embarque.\n"); continue;
            }
            msg.tipo = MSG_ENCERRAR; 
            msg.pid_origem = getpid();
            write(fdp_controlador, &msg, sizeof(Mensagem));
            break; 
        }
        else if (strcmp(comando, "agendar") == 0) { 
            int hora, distancia;
            char local[TAM_MAX_LOCAL];
            if (sscanf(linha, "%*s %d %s %d", &hora, local, &distancia) == 3) {
                msg.tipo = MSG_AGENDAR_VIAGEM;
                msg.pid_origem = getpid();
                msg.distancia = distancia;
                msg.hora_agendada = hora;
                strcpy(msg.local_origem, local);
                write(fdp_controlador, &msg, sizeof(Mensagem));
            } else printf("Erro: agendar <hora> <local> <dist>\n");
        }	
        else if (strcmp(comando, "entrar") == 0) {
            if (pid_veiculo_alocado != -1) {
                char fifo_veic[50];
                sprintf(fifo_veic, FIFO_VEICULO_FMT, pid_veiculo_alocado);
                int fd_v = open(fifo_veic, O_WRONLY);
                if(fd_v != -1) {
                    Mensagem m; m.tipo = MSG_PEDIDO_ENTRAR;
                    m.pid_origem = getpid();
                    write(fd_v, &m, sizeof(Mensagem));
                    close(fd_v);
                    printf(">> Entraste no veículo.\n");
                }
            } else printf("[ERRO] Nenhum veículo te chamou.\n");
        } 
        else if (strcmp(comando, "sair") == 0) {
            if (pid_veiculo_alocado != -1) {
                char fifo_veic[50];
                sprintf(fifo_veic, FIFO_VEICULO_FMT, pid_veiculo_alocado);
                int fd_v = open(fifo_veic, O_WRONLY);
                if(fd_v != -1) {
                    Mensagem m; m.tipo = MSG_PEDIDO_SAIR;
                    write(fd_v, &m, sizeof(Mensagem));
                    close(fd_v);
                }
                pid_veiculo_alocado = -1;
            } else printf("[ERRO] Não estás num veículo.\n");
        } 
        else if (strcmp(comando, "cancelar") == 0) {
            int id_cancelar;
            if (sscanf(linha, "%*s %d", &id_cancelar) == 1) {
                msg.tipo = MSG_CANCELAR_VIAGEM;
                msg.pid_origem = getpid();
                msg.id_viagem = id_cancelar;
                write(fdp_controlador, &msg, sizeof(Mensagem));
            } else printf("Erro: cancelar <id> (use 0 p/ todos)\n");
        }
        else if (strcmp(comando, "consultar") == 0) {
            msg.tipo = MSG_CONSULTAR_VIAGENS; 
            msg.pid_origem = getpid();
            write(fdp_controlador, &msg, sizeof(Mensagem));
        }
        else {
            printf("Comando inválido.\n");
        }
    }

    limpar_recursos();
    return 0;
}
