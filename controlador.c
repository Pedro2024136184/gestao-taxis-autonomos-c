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
#include <fcntl.h>


/* --- 1. DEFINES E CONSTANTES --- */
#define MAX_SERVICOS 20
#define MAX_UTILIZADORES 30

/* --- 2. ESTRUTURAS DE DADOS --- */

// Estrutura do Utilizador
typedef struct {
    char username[TAM_MAX_USERNAME];
    pid_t pid;
    int ocupado; // 0 = livre, 1 = logado
} UtilizadorInfo;

// Estrutura do Serviço (Viagem)
typedef struct {
    int id;
    pid_t pid_cliente;
    char origem[50]; 
    char destino[50];
    int distancia;
    int estado; // 0=Pendente, 1=Atribuído, 2=Concluído
    pid_t pid_veiculo; // Para saber quem é o táxi
    int pipe_fd;       // Para ler mensagens do táxi
} Servico;

/* --- 3. VARIÁVEIS GLOBAIS (Visíveis a todas as threads) --- */
int n_veiculos_max = 0;
int fdp_recebe;
pthread_mutex_t trinco = PTHREAD_MUTEX_INITIALIZER;

// Bases de dados do sistema (Têm de estar aqui para o ADMIN ver)
UtilizadorInfo utilizadores[MAX_UTILIZADORES]; 
Servico servicos[MAX_SERVICOS]; 

/* --- 4. PROTÓTIPOS --- */
void carregar_configuracoes();
void preparar_recursos();
void limpar_recursos();
void* thread_atende_clientes(void* arg);
void* interface_admin(void* arg);
void processar_mensagem_cliente(Mensagem msg);
void tratar_sinal_interrupcao(int sinal);

/* =========================================================================
   FUNÇÃO: interface_admin (Thread separada para comandos do teclado)
   ========================================================================= */
void *interface_admin(void *arg) {
    (void)arg; // Silencia warning
    char comando[100];

    printf("[ADMIN] Interface iniciada. Comandos: listar, utiliz, terminar\n");

    while (1) {
        printf("ADMIN >> ");
        // Lê o comando do teclado
        if (fgets(comando, sizeof(comando), stdin) == NULL) break;
        comando[strcspn(comando, "\n")] = 0; // Remove o \n

        // --- COMANDO TERMINAR ---
        if (strcmp(comando, "terminar") == 0) {
            printf("[ADMIN] A encerrar o sistema...\n");
            limpar_recursos();
            exit(0);
        }

        // --- COMANDO LISTAR (Serviços) ---
        else if (strcmp(comando, "listar") == 0) {
            pthread_mutex_lock(&trinco);
            printf("\n--- LISTA DE SERVIÇOS ---\n");
            int vazia = 1;
            for (int i = 0; i < MAX_SERVICOS; i++) {
                if (servicos[i].id != -1) { 
                    printf("ID: %d | Cliente PID: %d | Origem: %s | Dist: %d | Estado: %d\n", 
                           servicos[i].id, servicos[i].pid_cliente, servicos[i].origem, servicos[i].distancia, servicos[i].estado);
                    vazia = 0;
                }
            }
            if (vazia) printf("Nenhum serviço agendado.\n");
            printf("-------------------------\n");
            pthread_mutex_unlock(&trinco);
        }

        // --- COMANDO UTILIZ (Utilizadores) ---
        else if (strcmp(comando, "utiliz") == 0) {
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
        else {
            printf("[ERRO] Comando desconhecido.\n");
        }
    }
    return NULL;
}


void lancar_veiculo(int indice_servico) {
    int fd_pipe[2]; // [0] = leitura, [1] = escrita
    
    // 1. Criar Pipe
    if (pipe(fd_pipe) == -1) {
        perror("[ERRO] Falha ao criar pipe");
        return;
    }

    // 2. Criar Processo Filho (Fork)
    pid_t pid = fork();

    if (pid == -1) {
        perror("[ERRO] Falha no fork");
        return;
    }

    if (pid == 0) { 
        // --- CÓDIGO DO FILHO (VEÍCULO) ---
        
        // A. Fechar a extremidade de leitura do pipe (o filho só escreve)
        close(fd_pipe[0]);

        // B. Redirecionar o STDOUT para o pipe (o que o veículo fizer printf, o pai lê)
        dup2(fd_pipe[1], STDOUT_FILENO);
        close(fd_pipe[1]); // Fecha o original pois já foi duplicado

        // C. Preparar argumentos para o executável ./veiculo
        char str_id[10];
        char str_dist[10];
        sprintf(str_id, "%d", servicos[indice_servico].id);
        sprintf(str_dist, "%d", servicos[indice_servico].distancia);

        // D. Substituir a imagem do processo pelo executável do veículo
        // Sintaxe: execl(caminho, nome_processo, arg1, arg2, NULL)
        execl("./veiculo", "veiculo", str_id, servicos[indice_servico].origem, str_dist, NULL);
        
        // Se o execl falhar (ex: não encontrar o ficheiro), o código continua aqui:
        perror("[ERRO CRÍTICO] Falha no execl do veículo");
        exit(1);
    } 
    else {
        // --- CÓDIGO DO PAI (CONTROLADOR) ---
        int flags = fcntl(fd_pipe[0], F_GETFL, 0);
        fcntl(fd_pipe[0], F_SETFL, flags | O_NONBLOCK);
        // A. Fechar a extremidade de escrita (o pai só lê)
        close(fd_pipe[1]);

        // B. Guardar informações na struct
        servicos[indice_servico].pid_veiculo = pid;
        servicos[indice_servico].pipe_fd = fd_pipe[0]; // Guardamos para ler depois
        servicos[indice_servico].estado = 1; // 1 = Atribuído / Em curso

        printf("[CONTROLADOR] Veículo lançado (PID: %d) para serviço ID %d\n", pid, servicos[indice_servico].id);
    }
}



/* =========================================================================
   FUNÇÃO: processar_mensagem_cliente (Lógica principal)
   ========================================================================= */
void processar_mensagem_cliente(Mensagem msg) {
    Mensagem resposta;
    char fifo_resp[50];
    int fdp_envia;
    int aceito = 1;

    // Prepara nome do FIFO de resposta
    sprintf(fifo_resp, FIFO_CLIENTE_FMT, msg.pid_origem);

    // --- TRATAMENTO DE LOGIN ---
    if (msg.tipo == MSG_LOGIN) {
        pthread_mutex_lock(&trinco);
        
        // 1. Verificar se já existe
        for (int i = 0; i < MAX_UTILIZADORES; i++) {
            if (utilizadores[i].ocupado && strcmp(utilizadores[i].username, msg.username) == 0) {
                aceito = 0;
                strcpy(resposta.dados, "Login Recusado: Username em uso.");
                break;
            }
        }

        // 2. Registar se aceite
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
                printf("[INFO] Novo utilizador registado: %s\n", msg.username);
            } else {
                aceito = 0;
                strcpy(resposta.dados, "Login Recusado: Sistema cheio.");
            }
        }
        pthread_mutex_unlock(&trinco);

        // Enviar resposta ao cliente (apenas no Login)
        resposta.tipo = MSG_LOGIN_RESP;
        fdp_envia = open(fifo_resp, O_WRONLY);
        if (fdp_envia != -1) {
            write(fdp_envia, &resposta, sizeof(Mensagem));
            close(fdp_envia);
        } else {
            printf("[ERRO] Nao consegui responder ao cliente %d\n", msg.pid_origem);
        }
    }
    
    // --- TRATAMENTO DE AGENDAMENTO (Linha 15 da tabela) ---
    else if (msg.tipo == MSG_AGENDAR_VIAGEM) {
        pthread_mutex_lock(&trinco);
        
        printf("[PEDIDO] Novo agendamento de %s (PID: %d)\n", msg.username, msg.pid_origem);

        // 1. Procurar slot livre
        int slot_servico = -1;
        for (int i = 0; i < MAX_SERVICOS; i++) {
            if (servicos[i].id == -1) {
                slot_servico = i;
                break;
            }
        }

        // 2. Guardar dados
        if (slot_servico != -1) {
            servicos[slot_servico].id = slot_servico + 1; // ID começa em 1
            servicos[slot_servico].pid_cliente = msg.pid_origem;
            strcpy(servicos[slot_servico].origem, msg.local_origem);
            servicos[slot_servico].distancia = msg.distancia;
            servicos[slot_servico].estado = 0; // 0 = Pendente
            
            printf("[SUCESSO] Agendamento ID %d registado. Aguardando condutor...\n", servicos[slot_servico].id);
	    lancar_veiculo(slot_servico); 
       } else {
            printf("[ERRO] Lista de serviços cheia!\n");
        }
        
        pthread_mutex_unlock(&trinco);
    }
}

/* =========================================================================
   FUNÇÃO: thread_atende_clientes (Lê do FIFO)
   ========================================================================= */
void* thread_atende_clientes(void* arg) {
    (void)arg;
    Mensagem msg;
    
    // Abre o FIFO principal para leitura
    fdp_recebe = open(FIFO_CONTROLADOR, O_RDWR);
    if (fdp_recebe == -1) {
        perror("[ERRO] Abrir FIFO Servidor");
        exit(1);
    }

    while (1) {
        if (read(fdp_recebe, &msg, sizeof(Mensagem)) == sizeof(Mensagem)) {
            processar_mensagem_cliente(msg);
        }
    }
    return NULL;
}



void* monitor_veiculos(void* arg) {
    (void)arg;
    char buffer[256];
    
    printf("[MONITOR] Iniciado e a aguardar telemetria...\n");

    while (1) {
        pthread_mutex_lock(&trinco);
        
        for (int i = 0; i < MAX_SERVICOS; i++) {
            // Se o serviço está "Em Curso" (estado 1)
            if (servicos[i].id != -1 && servicos[i].estado == 1) {
                
                // Tenta ler do pipe
                int n = read(servicos[i].pipe_fd, buffer, sizeof(buffer) - 1);
                
                if (n > 0) {
                    // --- RECEBEU DADOS (TAREFA 23) ---
                    buffer[n] = '\0'; // Fecha a string
                    printf("[TELEMETRIA] Veículo %d diz: %s", servicos[i].pid_veiculo, buffer);
                } 
                else if (n == 0) {
                    // --- VEÍCULO FECHOU O PIPE (ACABOU A VIAGEM - TAREFA 25) ---
                    // O read retorna 0 quando o escritor (filho) fecha o pipe ou morre.
                    
                    waitpid(servicos[i].pid_veiculo, NULL, 0); // Limpa o processo zombie
                    close(servicos[i].pipe_fd); // Fecha o descritor do pai
                    
                    printf("[CONCLUÍDO] Viagem ID %d terminada.\n", servicos[i].id);
                    
                    // Liberta o slot
                    servicos[i].id = -1;
                    servicos[i].estado = 0;
                    // (Opcional) Avisar utilizador via FIFO aqui
                }
            }
        }
        
        pthread_mutex_unlock(&trinco);
        usleep(100000); // Dorme 0.1s para não gastar 100% de CPU
    }
    return NULL;
}
/* =========================================================================
   FUNÇÃO: main
   ========================================================================= */
int main() {
    pthread_t t_clientes;
    pthread_t t_admin;
    pthread_t t_monitor; // <--- 1. Nova variável para a thread do monitor

    signal(SIGINT, tratar_sinal_interrupcao); 
    
    carregar_configuracoes();
    preparar_recursos();

    printf("\n[CONTROLADOR] Iniciado. Max Veiculos: %d\n", n_veiculos_max);
    
    // 2. Lança a thread que ouve os clientes (FIFO)
    if (pthread_create(&t_clientes, NULL, thread_atende_clientes, NULL) != 0) {
        perror("[ERRO] Thread clientes");
        exit(1);
    }

    // 3. Lança a thread que MONITORIZA os veículos (NOVO)
    // Esta thread vai correr a função 'monitor_veiculos' que criámos antes
    if (pthread_create(&t_monitor, NULL, monitor_veiculos, NULL) != 0) {
        perror("[ERRO] Thread monitor");
        exit(1);
    }

    // 4. Lança a thread que ouve o administrador (Teclado)
    if (pthread_create(&t_admin, NULL, interface_admin, NULL) != 0) {
        perror("[ERRO] Thread admin");
        exit(1);
    }

    // 5. O main fica à espera. Adicionamos o join do monitor também (opcional, mas boa prática)
    pthread_join(t_admin, NULL);
    pthread_join(t_clientes, NULL);
    pthread_join(t_monitor, NULL);

    limpar_recursos();
    return 0;
}

/* --- Funções Auxiliares --- */
void carregar_configuracoes() {
    char* env = getenv("NVEICULOS");
    n_veiculos_max = (env) ? atoi(env) : 3; // Valor por defeito se não houver VAR ambiente
    
    // Inicializar arrays
    memset(utilizadores, 0, sizeof(utilizadores));
    // Inicializar serviços como livres (-1)
    for(int i=0; i<MAX_SERVICOS; i++) servicos[i].id = -1; 
}

void preparar_recursos() {
    if (access(FIFO_CONTROLADOR, F_OK) == 0) unlink(FIFO_CONTROLADOR);
    if (mkfifo(FIFO_CONTROLADOR, 0666) == -1) { perror("mkfifo"); exit(1); }
}

void limpar_recursos() {
    close(fdp_recebe);
    unlink(FIFO_CONTROLADOR);
    pthread_mutex_destroy(&trinco);
}

void tratar_sinal_interrupcao(int sinal) { 
    (void)sinal; 
    printf("\n[SINAL] A desligar...\n");
    limpar_recursos(); 
    exit(0); 
}
