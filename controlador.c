#include "common.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_SERVICOS 20
#define MAX_UTILIZADORES 30

// Estrutura para armazenar informações de utilizadores conectados
typedef struct {
    char username[TAM_MAX_USERNAME];
    pid_t pid;
    int ocupado; 
} UtilizadorInfo;

// Estrutura para armazenar informações de serviços (viagens)
typedef struct {
    int id;
    pid_t pid_cliente;
    char origem[50];
    int distancia;
    int hora_agendada;
    int estado;              // 0=PENDENTE, 1=EM CURSO
    pid_t pid_veiculo;
    int pipe_fd;             // Pipe para ler dados do veículo
    float progresso_atual;
} Servico;

// Variáveis globais
int veiculos_ativos = 0;
int n_veiculos_max = 0;
int segundos_decorridos = 0;
int fdp_recebe;
long long total_kms_percorridos = 0;
pthread_mutex_t trinco = PTHREAD_MUTEX_INITIALIZER;  // Mutex para sincronização

UtilizadorInfo utilizadores[MAX_UTILIZADORES];
Servico servicos[MAX_SERVICOS];

// Declarações de funções
void carregar_configuracoes();
void* simulador_tempo(void* arg);
void preparar_recursos();
void limpar_recursos();
void* thread_atende_clientes(void* arg);
void* interface_admin(void* arg);
void* monitor_veiculos(void* arg);
void processar_mensagem_cliente(Mensagem msg);
void tratar_sinal_interrupcao(int sinal);
void lancar_veiculo(int indice_servico);

// Thread: Interface administrativa para comandos do sistema
void *interface_admin(void *arg) {
    (void)arg;
    char comando[100];
    printf("[ADMIN] Interface iniciada. Comandos: listar, utiliz, frota, km, hora, cancelar <id>, terminar\n");

    while (1) {
        printf("ADMIN >> ");
        if (fgets(comando, sizeof(comando), stdin) == NULL) break;
        comando[strcspn(comando, "\n")] = 0;

        if (strlen(comando) == 0) continue;

        if (strcmp(comando, "terminar") == 0) {
            printf("[ADMIN] A encerrar o sistema...\n");
            kill(getpid(), SIGINT); 
            break;
        }
        else if (strcmp(comando, "listar") == 0) {
            // Lista todos os serviços agendados
            pthread_mutex_lock(&trinco);
            printf("\n--- LISTA DE SERVIÇOS ---\n");
            int vazia = 1;
            for (int i = 0; i < MAX_SERVICOS; i++) {
                if (servicos[i].id != -1) {
                    char estado_str[20];
                    if(servicos[i].estado == 0) strcpy(estado_str, "PENDENTE");
                    else if(servicos[i].estado == 1) strcpy(estado_str, "EM CURSO");
                    
                    printf("ID: %d | Cliente PID: %d | Origem: %s | Dist: %d | Estado: %s\n",
                           servicos[i].id, servicos[i].pid_cliente, servicos[i].origem,
                           servicos[i].distancia, estado_str);
                    vazia = 0;
                }
            }
            if (vazia) printf("Nenhum serviço agendado.\n");
            printf("-------------------------\n");
            pthread_mutex_unlock(&trinco);
        }
        else if (strcmp(comando, "utiliz") == 0) {
            // Lista utilizadores online
            pthread_mutex_lock(&trinco);
            printf("\n--- UTILIZADORES ON-LINE ---\n");
            int vazia = 1;
            for (int i = 0; i < MAX_UTILIZADORES; i++) {
                if (utilizadores[i].ocupado) {
                    printf("Slot %d: %s (PID: %d)\n", i, utilizadores[i].username, utilizadores[i].pid);
                    vazia = 0;
                }
            }
            if (vazia) printf("Nenhum utilizador ligado.\n");
            printf("----------------------------\n");
            pthread_mutex_unlock(&trinco);
        }
        else if (strcmp(comando, "frota") == 0) {
            // Mostra estado dos veículos em circulação
            pthread_mutex_lock(&trinco);
            printf("\n--- ESTADO DA FROTA ---\n");
            int ativos = 0;
            for(int i=0; i<MAX_SERVICOS; i++) {
                if(servicos[i].id != -1 && servicos[i].estado == 1) {
                    float perc = 0;
                    if(servicos[i].distancia > 0) 
                        perc = (servicos[i].progresso_atual / (float)servicos[i].distancia) * 100.0;
                    
                    printf("Veículo PID %d (Serviço %d): %.1f%% concluído\n", 
                           servicos[i].pid_veiculo, servicos[i].id, perc);
                    ativos++;
                }
            }
            if(ativos == 0) printf("Nenhum veículo em circulação.\n");
            printf("Veículos ativos: %d / %d\n", veiculos_ativos, n_veiculos_max);
            printf("-----------------------\n");
            pthread_mutex_unlock(&trinco);
        }
        else if (strcmp(comando, "km") == 0) {
            // Mostra estatística de kms
            pthread_mutex_lock(&trinco);
            printf("[ESTATISTICA] Total Kms percorridos pela frota: %lld km\n", total_kms_percorridos);
            pthread_mutex_unlock(&trinco);
        }
        else if (strcmp(comando, "hora") == 0) {
            // Mostra tempo simulado
            pthread_mutex_lock(&trinco);
            printf("[RELOGIO] Tempo simulado: %d segundos\n", segundos_decorridos);
            pthread_mutex_unlock(&trinco);
        }
        else if (strncmp(comando, "cancelar", 8) == 0) {
            // Cancela serviço específico ou todos
            int id_cancelar;
            if (sscanf(comando, "%*s %d", &id_cancelar) == 1) {
                pthread_mutex_lock(&trinco);
                if (id_cancelar == 0) {
                    printf("[ADMIN] A cancelar TODOS os serviços...\n");
                    for (int i = 0; i < MAX_SERVICOS; i++) {
                        if (servicos[i].id != -1) {
                            if (servicos[i].estado == 1) {
                                kill(servicos[i].pid_veiculo, SIGUSR1);
                            }
                            servicos[i].id = -1;
                            servicos[i].estado = 0;
                        }
                    }
                }
                else {
                    int encontrado = 0;
                    for (int i = 0; i < MAX_SERVICOS; i++) {
                        if (servicos[i].id == id_cancelar) {
                            encontrado = 1;
                            if (servicos[i].estado == 1) {
                                kill(servicos[i].pid_veiculo, SIGUSR1);
                                printf("[ADMIN] Sinal enviado ao veiculo PID %d.\n", servicos[i].pid_veiculo);
                            } else {
                                printf("[ADMIN] Agendamento ID %d removido.\n", id_cancelar);
                            }
                            servicos[i].id = -1;
                            servicos[i].estado = 0;
                            break;
                        }
                    }
                    if (!encontrado) printf("[ERRO] ID %d não encontrado.\n", id_cancelar);
                }
                pthread_mutex_unlock(&trinco);
            } else {
                printf("[ERRO] Use: cancelar <id> (0 para todos)\n");
            }
        }
        else {
            printf("[ERRO] Comando desconhecido.\n");
        }
    }
    return NULL;
}

// Thread: Monitora veículos e lê dados dos pipes
void* monitor_veiculos(void* arg) {
    (void)arg;
    char buffer[256];
    printf("[MONITOR] Iniciado.\n");

    while (1) {
        pthread_mutex_lock(&trinco); 

        // Verifica cada serviço em curso
        for (int i = 0; i < MAX_SERVICOS; i++) {
            if (servicos[i].id != -1 && servicos[i].estado == 1) {
                // Lê dados do pipe do veículo
                int n = read(servicos[i].pipe_fd, buffer, sizeof(buffer) - 1);
                if (n > 0) {
                    buffer[n] = '\0';
                    
                    if (strstr(buffer, "FIM_SERVICO_KMS") != NULL) {
                        // Serviço concluído
                        float kms_finais = 0;
                        sscanf(buffer, "FIM_SERVICO_KMS: %f", &kms_finais);
                        total_kms_percorridos += (long long)kms_finais;
                        
                        printf("[FROTA] Serviço %d concluído. Kms: %.1f\n", servicos[i].id, kms_finais);
                        close(servicos[i].pipe_fd);
                        servicos[i].id = -1;
                        servicos[i].estado = 0;
                        veiculos_ativos--;
                    }
                    else if (strstr(buffer, "ANDAMENTO") != NULL) {
                        // Atualiza progresso do serviço
                        float kms_atuais = 0;
                        if(sscanf(buffer, "ANDAMENTO %f", &kms_atuais) == 1) {
                            servicos[i].progresso_atual = kms_atuais;
                        }
                    }
                }
                else if (n == 0) {
                    // Pipe fechado - serviço terminou
                    close(servicos[i].pipe_fd);
                    servicos[i].id = -1;
                    servicos[i].estado = 0;
                    veiculos_ativos--;
                }
            }
        }
        pthread_mutex_unlock(&trinco); 
        usleep(100000);
    }
    return NULL;
}

// Processa mensagens recebidas dos clientes
void processar_mensagem_cliente(Mensagem msg) {
    Mensagem resposta;
    char fifo_resp[50];
    sprintf(fifo_resp, FIFO_CLIENTE_FMT, msg.pid_origem);

    if (msg.tipo == MSG_LOGIN) {
        // Processa login do cliente
        pthread_mutex_lock(&trinco);
        int aceito = 1;
        for (int i = 0; i < MAX_UTILIZADORES; i++) {
            if (utilizadores[i].ocupado && strcmp(utilizadores[i].username, msg.username) == 0) {
                aceito = 0; break;
            }
        }
        if (aceito) {
            int slot = -1;
            for (int i = 0; i < MAX_UTILIZADORES; i++) {
                if (!utilizadores[i].ocupado) { slot = i; break; }
            }
            if (slot != -1) {
                utilizadores[slot].ocupado = 1;
                utilizadores[slot].pid = msg.pid_origem;
                strcpy(utilizadores[slot].username, msg.username);
                strcpy(resposta.dados, "Login Aceite!");
                printf("[LOGIN] Novo utilizador: %s\n", msg.username);
            } else {
                aceito = 0; strcpy(resposta.dados, "Login Recusado: Cheio.");
            }
        } else {
            strcpy(resposta.dados, "Login Recusado: Username em uso.");
        }
        pthread_mutex_unlock(&trinco);

        resposta.tipo = MSG_LOGIN_RESP;
        int fd = open(fifo_resp, O_WRONLY);
        if (fd != -1) { write(fd, &resposta, sizeof(Mensagem)); close(fd); }
    }
    
    else if (msg.tipo == MSG_CONSULTAR_VIAGENS) {
        // Retorna lista de viagens do cliente
        pthread_mutex_lock(&trinco);
        char buffer_lista[TAM_BUFFER_MSG];
        strcpy(buffer_lista, "As tuas viagens:\n");
        int encontrou = 0;

        for(int i=0; i<MAX_SERVICOS; i++) {
            if (servicos[i].id != -1 && servicos[i].pid_cliente == msg.pid_origem) {
                char linha[100];
                char estado_str[20];
                if(servicos[i].estado == 0) strcpy(estado_str, "PENDENTE");
                else if(servicos[i].estado == 1) strcpy(estado_str, "EM CURSO");
                
                sprintf(linha, "> ID: %d | Dest: %s | Status: %s\n", 
                        servicos[i].id, servicos[i].origem, estado_str);
                
                if (strlen(buffer_lista) + strlen(linha) < TAM_BUFFER_MSG - 1) {
                    strcat(buffer_lista, linha);
                }
                encontrou = 1;
            }
        }
        if (!encontrou) strcat(buffer_lista, "Nenhuma viagem agendada.");
        pthread_mutex_unlock(&trinco);

        resposta.tipo = MSG_INFO_VEICULO; 
        strcpy(resposta.dados, buffer_lista);
        
        int fd = open(fifo_resp, O_WRONLY | O_NONBLOCK);
        if (fd != -1) { write(fd, &resposta, sizeof(Mensagem)); close(fd); }
    }

    else if (msg.tipo == MSG_AGENDAR_VIAGEM) {
        // Agenda nova viagem
        pthread_mutex_lock(&trinco);
        int slot = -1;
        for (int i = 0; i < MAX_SERVICOS; i++) {
            if (servicos[i].id == -1) { slot = i; break; }
        }

        if (slot != -1) {
            servicos[slot].id = slot + 1; 
            servicos[slot].pid_cliente = msg.pid_origem;
            strcpy(servicos[slot].origem, msg.local_origem);
            servicos[slot].distancia = msg.distancia;
            servicos[slot].hora_agendada = msg.hora_agendada;
            servicos[slot].estado = 0;  // PENDENTE
            servicos[slot].progresso_atual = 0;

            if (veiculos_ativos < n_veiculos_max) {
                printf("[AGENDA] Serviço %d iniciado.\n", servicos[slot].id);
            } else {
                printf("[FILA] Serviço %d em espera.\n", servicos[slot].id);
                strcpy(resposta.dados, "Agendamento em espera (Frota cheia).");
                resposta.tipo = MSG_INFO_VEICULO;
                int fd = open(fifo_resp, O_WRONLY | O_NONBLOCK);
                if(fd != -1) { write(fd, &resposta, sizeof(Mensagem)); close(fd); }
            }
        } else {
            strcpy(resposta.dados, "Erro: Limite de agendamentos atingido.");
            resposta.tipo = MSG_INFO_VEICULO;
            int fd = open(fifo_resp, O_WRONLY | O_NONBLOCK);
            if(fd != -1) { write(fd, &resposta, sizeof(Mensagem)); close(fd); }
        }
        pthread_mutex_unlock(&trinco);
    }

    else if (msg.tipo == MSG_CANCELAR_VIAGEM) {
        // Cancela viagem do cliente
        pthread_mutex_lock(&trinco);
        if (msg.id_viagem == 0) {
            for (int i = 0; i < MAX_SERVICOS; i++) {
                if (servicos[i].pid_cliente == msg.pid_origem && servicos[i].id != -1) {
                    if (servicos[i].estado == 0) {
                        servicos[i].id = -1;
                    }
                }
            }
            printf("[CANCELAR] Todos os pendentes do cliente %d removidos.\n", msg.pid_origem);
        } else {
            for (int i = 0; i < MAX_SERVICOS; i++) {
                if (servicos[i].pid_cliente == msg.pid_origem && servicos[i].id == msg.id_viagem) {
                    if (servicos[i].estado == 0) {
                        servicos[i].id = -1;
                        printf("[CANCELAR] Agendamento %d cancelado pelo cliente.\n", msg.id_viagem);
                    }
                    break;
                }
            }
        }
        pthread_mutex_unlock(&trinco);
    }

    else if (msg.tipo == MSG_ENCERRAR) {
        // Cliente desconecta
        pthread_mutex_lock(&trinco);
        for (int i = 0; i < MAX_SERVICOS; i++) {
            if (servicos[i].pid_cliente == msg.pid_origem && servicos[i].estado == 0) {
                servicos[i].id = -1;
            }
        }
        for (int i = 0; i < MAX_UTILIZADORES; i++) {
            if (utilizadores[i].pid == msg.pid_origem) {
                utilizadores[i].ocupado = 0;
                printf("[LOGOUT] Cliente %s saiu.\n", utilizadores[i].username);
                break;
            }
        }
        pthread_mutex_unlock(&trinco);
    }
}

// Cria processo filho para executar veículo
void lancar_veiculo(int indice_servico) {
    int fd_pipe[2]; 
    if (pipe(fd_pipe) == -1) return;

    pid_t pid = fork();
    if (pid == 0) {
        // Processo filho: executa programa veiculo
        close(fd_pipe[0]);
        dup2(fd_pipe[1], STDOUT_FILENO);
        close(fd_pipe[1]);

        char str_id[10], str_dist[10], str_pid_cli[20];
        sprintf(str_id, "%d", servicos[indice_servico].id);
        sprintf(str_dist, "%d", servicos[indice_servico].distancia);
        sprintf(str_pid_cli, "%d", servicos[indice_servico].pid_cliente);

        execl("./veiculo", "veiculo", str_id, servicos[indice_servico].origem, str_dist, str_pid_cli, NULL);
        exit(1);
    } else {
        // Processo pai: configura pipe como não-bloqueante
        int flags = fcntl(fd_pipe[0], F_GETFL, 0);
        fcntl(fd_pipe[0], F_SETFL, flags | O_NONBLOCK);
        close(fd_pipe[1]);
        servicos[indice_servico].pid_veiculo = pid;
        servicos[indice_servico].pipe_fd = fd_pipe[0]; 
        servicos[indice_servico].estado = 1;  // EM CURSO
    }
}

// Thread: Recebe mensagens dos clientes via FIFO
void* thread_atende_clientes(void* arg) {
    (void)arg;
    Mensagem msg;
    fdp_recebe = open(FIFO_CONTROLADOR, O_RDWR);
    while (1) {
        if (read(fdp_recebe, &msg, sizeof(Mensagem)) == sizeof(Mensagem)) {
            processar_mensagem_cliente(msg);
        }
    }
    return NULL;
}

// Thread: Simula passagem de tempo e inicia serviços agendados
void* simulador_tempo(void* arg) {
    (void)arg;
    while (1) {
        sleep(1);
        pthread_mutex_lock(&trinco);
        segundos_decorridos++;

        // Verifica se algum serviço deve ser iniciado
        for (int i = 0; i < MAX_SERVICOS; i++) {
            if (servicos[i].id != -1 && servicos[i].estado == 0) {
                if (servicos[i].hora_agendada <= segundos_decorridos) {
                    if (veiculos_ativos < n_veiculos_max) {
                        printf("[RELOGIO] Hora %d: A iniciar serviço %d (Agendado: %d)\n", 
                               segundos_decorridos, servicos[i].id, servicos[i].hora_agendada);
                        
                        lancar_veiculo(i);
                        veiculos_ativos++;
                    }
                }
            }
        }
        pthread_mutex_unlock(&trinco);
    }
    return NULL;
}

// Carrega configurações do sistema (número máximo de veículos)
void carregar_configuracoes() {
    char* env = getenv("NVEICULOS");
    n_veiculos_max = (env) ? atoi(env) : 3; 
    if(n_veiculos_max > 10) n_veiculos_max = 10;

    memset(utilizadores, 0, sizeof(utilizadores));
    for(int i=0; i<MAX_SERVICOS; i++) servicos[i].id = -1;
}

// Prepara recursos (cria FIFO do controlador)
void preparar_recursos() {
    if (access(FIFO_CONTROLADOR, F_OK) == 0) unlink(FIFO_CONTROLADOR);
    if (mkfifo(FIFO_CONTROLADOR, 0666) == -1) exit(1);
}

// Liberta recursos (fecha FIFO, mata veículos)
void limpar_recursos() {
    close(fdp_recebe);
    unlink(FIFO_CONTROLADOR);
    for(int i=0; i<MAX_SERVICOS; i++){
        if(servicos[i].id != -1 && servicos[i].estado == 1) kill(servicos[i].pid_veiculo, SIGKILL);
    }
}

// Handler para sinal de interrupção (CTRL+C)
void tratar_sinal_interrupcao(int sinal) {
    (void)sinal;
    limpar_recursos();
    exit(0);
}

// Handler para processos filhos mortos (evita zombies)
void trata_zombies(int s) { 
    (void)s; 
    while (waitpid(-1, NULL, WNOHANG) > 0); 
}

int main() {
    pthread_t t_clientes, t_admin, t_monitor, t_tempo;
    signal(SIGINT, tratar_sinal_interrupcao);
    signal(SIGCHLD, trata_zombies);

    carregar_configuracoes();
    preparar_recursos();

    printf("\n[CONTROLADOR] Iniciado. Max Veiculos: %d\n", n_veiculos_max);

    // Cria threads principais
    pthread_create(&t_clientes, NULL, thread_atende_clientes, NULL);
    pthread_create(&t_monitor, NULL, monitor_veiculos, NULL);
    pthread_create(&t_admin, NULL, interface_admin, NULL);
    pthread_create(&t_tempo, NULL, simulador_tempo, NULL);

    pthread_join(t_admin, NULL);
    limpar_recursos();
    return 0;
}
