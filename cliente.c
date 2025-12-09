#include "common.h"
#include "utils.h"

// Variáveis globais para limpeza
char fifo_nome_cliente[50];
int fdp_cliente = -1; 
int fdp_controlador = -1; 
int pid_veiculo_alocado = -1; // Para saber para onde mandar o 'entrar'
pthread_mutex_t trinco_cli = PTHREAD_MUTEX_INITIALIZER;


// Função para fechar pipes e apagar o FIFO do disco
void limpar_recursos() {
    if (fdp_cliente != -1) close(fdp_cliente);
    if (fdp_controlador != -1) close(fdp_controlador);
    
    // Só apaga se o nome tiver sido definido
    if (strlen(fifo_nome_cliente) > 0) {
        unlink(fifo_nome_cliente);
    }
    printf("\n[CLIENTE] Encerrado.\n");
}

// Handler para quando carregas em Ctrl+C
void tratar_sinal(int sinal) {
    (void)sinal; // Ignorar warning de variavel nao usada
    limpar_recursos();
    exit(0);
}

void* recebe_respostas(void* arg) {
    (void)arg;
    Mensagem resp;
    while(1) {
        if (read(fdp_cliente, &resp, sizeof(Mensagem)) > 0) {
            printf("\n[MSG RECEBIDA] %s\nCliente> ", resp.dados);
            fflush(stdout);
            
            if (resp.tipo == MSG_INFO_VEICULO) {
                // Lógica 1: Veículo Chegou (Guardar PID)
                // Se já estivermos ocupados, ignoramos outros avisos (segurança)
                if (pid_veiculo_alocado == -1 && (strstr(resp.dados, "Cheguei") != NULL || strstr(resp.dados, "chegou") != NULL)) {
                     pthread_mutex_lock(&trinco_cli);
                     pid_veiculo_alocado = resp.pid_origem;
                     pthread_mutex_unlock(&trinco_cli);
                }

                // Lógica 2: Viagem Acabou (Libertar PID) - CORREÇÃO DO BUG
                // Se recebermos "CONCLUIDA" ou "CANCELADA", o veículo morreu.
                if (strstr(resp.dados, "CONCLUIDA") != NULL || strstr(resp.dados, "CANCELADA") != NULL) {
                    pthread_mutex_lock(&trinco_cli);
                    pid_veiculo_alocado = -1; // <--- RESET! Agora podes agendar outra.
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

    // 1. Validar argumentos
    if (argc != 2) {
        printf("Uso: %s <username>\n", argv[0]);
        return 1;
    }

    // 2. Configurar sinais (para limpar FIFO ao sair)
    signal(SIGINT, tratar_sinal);

    // 3. Criar o FIFO exclusivo deste cliente
    sprintf(fifo_nome_cliente, FIFO_CLIENTE_FMT, getpid());
    if (mkfifo(fifo_nome_cliente, 0666) == -1) {
        perror("[ERRO] Criar FIFO cliente");
        return 1;
    }

    // 4. Abrir o FIFO do Controlador (Escrita)
    // Se falhar aqui, o controlador provavelmente não está a correr
    fdp_controlador = open(FIFO_CONTROLADOR, O_WRONLY);
    if (fdp_controlador == -1) {
        printf("[ERRO] O Controlador não está em execução!\n");
        unlink(fifo_nome_cliente);
        return 1;
    }

    // 5. Enviar pedido de LOGIN
    printf("[CLIENTE] A tentar login como '%s'...\n", argv[1]);
    
    msg.tipo = MSG_LOGIN;
    msg.pid_origem = getpid();
    strcpy(msg.username, argv[1]);
    
    // Escreve a mensagem no pipe do controlador
    write(fdp_controlador, &msg, sizeof(Mensagem));

    // 6. Abrir o nosso FIFO para receber a resposta (Leitura)
    fdp_cliente = open(fifo_nome_cliente, O_RDWR); // O_RDWR evita bloqueios estranhos
    
    // Fica à espera da resposta...
    if (read(fdp_cliente, &resposta, sizeof(Mensagem)) > 0) {
        if (resposta.tipo == MSG_LOGIN_RESP) {
            printf("[CONTROLADOR] %s\n", resposta.dados);
            
            // Verifica se a resposta contém a palavra "Recusado"
            if (strstr(resposta.dados, "Recusado") != NULL) {
                limpar_recursos();
                return 1; // Sai do programa
            }
        }
    }
    pthread_t t_leitura;
    pthread_create(&t_leitura, NULL, recebe_respostas, NULL);
    // 7. Loop Principal (Login aceite)
    printf(">> Aguardando comandos (Ctrl+C para sair)...\n");
    // --- Loop de Comandos ---
    char linha[TAM_BUFFER_MSG];
    char comando[20];

    while (1) {
        printf("Cliente> ");
        if (fgets(linha, sizeof(linha), stdin) == NULL) break;
        
        // Remove o \n do final
        linha[strcspn(linha, "\n")] = 0;
        
        // Se a linha estiver vazia, ignora
        if (strlen(linha) == 0) continue;

        // Extrai a primeira palavra para ver qual é o comando
        sscanf(linha, "%s", comando);

        if (strcmp(comando, "terminar") == 0) {
          // REQUERIMENTO 1: Não pode sair se estiver em viagem (veículo alocado)
            if (pid_veiculo_alocado != -1) {
                printf("[ERRO] Nao pode terminar. Tem um serviço em curso ou à espera de iniciar.\n");
                continue;
            }

            // REQUERIMENTO 2: Avisar controlador para cancelar agendamentos pendentes
            msg.tipo = MSG_ENCERRAR; 
            msg.pid_origem = getpid();
            write(fdp_controlador, &msg, sizeof(Mensagem));
            printf("[INFO] A informar controlador para cancelar agendamentos pendentes.\n");
            break; // Sai do loop e chama limpar_recursos
        }
        else if (strcmp(comando, "agendar") == 0) {
            // Formato: agendar <hora> <local> <distancia>
            int hora, distancia;
            char local[TAM_MAX_LOCAL];
            
            // sscanf retorna o número de variáveis preenchidas com sucesso
            int n = sscanf(linha, "%*s %d %s %d", &hora, local, &distancia);
            
            if (n == 3) {
                // Preencher a mensagem
                msg.tipo = MSG_AGENDAR_VIAGEM;
                msg.pid_origem = getpid();
                msg.hora_agendada = hora;
                msg.distancia = distancia;
                strcpy(msg.local_origem, local);
                
                // Enviar ao controlador
                write(fdp_controlador, &msg, sizeof(Mensagem));
                printf("[INFO] Pedido de viagem enviado!\n");
            } else {
                printf("[ERRO] Formato invalido! Use: agendar <hora> <local> <distancia>\n");
            }
        } else if (strcmp(comando, "entrar") == 0) {
        if (pid_veiculo_alocado != -1) {
            char fifo_veic[50];
            sprintf(fifo_veic, FIFO_VEICULO_FMT, pid_veiculo_alocado);
            int fd_v = open(fifo_veic, O_WRONLY);
            
            Mensagem m; 
            m.tipo = MSG_PEDIDO_ENTRAR;
            m.pid_origem = getpid();
            write(fd_v, &m, sizeof(Mensagem));
            close(fd_v);
            printf("[INFO] Entraste no veículo.\n");
            } else {
              printf("[ERRO] Nenhum veículo chegou ainda.\n");
            } 
        } else if (strcmp(comando, "sair") == 0) {
         if (pid_veiculo_alocado != -1) {
            char fifo_veic[50];
            sprintf(fifo_veic, FIFO_VEICULO_FMT, pid_veiculo_alocado);
            int fd_v = open(fifo_veic, O_WRONLY);
            Mensagem m; m.tipo = MSG_PEDIDO_SAIR;
            write(fd_v, &m, sizeof(Mensagem));
            close(fd_v);
            pid_veiculo_alocado = -1; // Reset
         }
        } else if (strcmp(comando, "cancelar") == 0) {
            int id_cancelar;
            // Cliente cancela APENAS agendamentos (Estado 0 ou pendente).
            if (sscanf(linha, "%*s %d", &id_cancelar) == 1 && id_cancelar > 0) {
                msg.tipo = MSG_CANCELAR_VIAGEM;
                msg.pid_origem = getpid();
                msg.id_viagem = id_cancelar;
                write(fdp_controlador, &msg, sizeof(Mensagem));
                printf("[INFO] Pedido de cancelamento ID %d enviado ao controlador.\n", id_cancelar);
            } else {
                printf("[ERRO] Formato inválido! Use: cancelar <id> (ID > 0)\n");
            }
        }
        
        // --- NOVO COMANDO: CONSULTAR ---
        else if (strcmp(comando, "consultar") == 0) {
            // O enunciado exige que o cliente consulte os seus próprios serviços no controlador.
            msg.tipo = MSG_CONSULTAR_VIAGENS; 
            msg.pid_origem = getpid();
            write(fdp_controlador, &msg, sizeof(Mensagem));
            printf("[INFO] Pedido de consulta de viagens enviado.\n");
        }
        else {
            printf("[ERRO] Comando desconhecido. Tente: agendar, terminar\n");
        }
    }

    limpar_recursos();
    return 0;
}
