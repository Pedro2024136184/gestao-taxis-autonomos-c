#include "common.h"
#include "utils.h"
#include <pthread.h>

/* --- Estrutura interna para guardar utilizadores logados --- */
typedef struct {
    char username[TAM_MAX_USERNAME];
    pid_t pid;
    int ocupado; // 0 = livre, 1 = logado
} UtilizadorInfo;

/* --- Variáveis Globais --- */
int n_veiculos_max = 0;
int fdp_recebe;
pthread_mutex_t trinco;
UtilizadorInfo utilizadores[MAX_UTILIZADORES]; // Base de dados de users

/* --- Protótipos --- */
void carregar_configuracoes();
void preparar_recursos();
void limpar_recursos();
void* thread_atende_clientes(void* arg);
void processar_mensagem_cliente(Mensagem msg);
void tratar_sinal_interrupcao(int sinal);
void cmd_terminar();

int main() {
    char comando[50];
    pthread_t t_clientes;

    signal(SIGINT, tratar_sinal_interrupcao); 
    carregar_configuracoes();
    preparar_recursos();

    printf("\n[CONTROLADOR] Iniciado. Max Veiculos: %d\n", n_veiculos_max);
    
    if (pthread_create(&t_clientes, NULL, thread_atende_clientes, NULL) != 0) {
        perror("[ERRO] Thread clientes");
        exit(1);
    }

    while (1) {
        printf("\nAdmin> ");
        if (fgets(comando, sizeof(comando), stdin) == NULL) break;
        comando[strcspn(comando, "\n")] = 0;

        if (strcmp(comando, "terminar") == 0) {
            cmd_terminar();
            break; 
        } else if (strcmp(comando, "utiliz") == 0) {
            // Comando 'utiliz' do enunciado
            printf("--- Utilizadores Online ---\n");
            pthread_mutex_lock(&trinco);
            for(int i=0; i<MAX_UTILIZADORES; i++) {
                if(utilizadores[i].ocupado)
                    printf("- %s (PID: %d)\n", utilizadores[i].username, utilizadores[i].pid);
            }
            pthread_mutex_unlock(&trinco);
        }
    }
    limpar_recursos();
    return 0;
}

/* --- Lógica de Resposta ao Cliente --- */
void processar_mensagem_cliente(Mensagem msg) {
    Mensagem resposta;
    char fifo_resp[50];
    int fdp_envia;
    int aceito = 1;

    printf("[DEBUG] Msg de %s (PID: %d): Tipo %d\n", msg.username, msg.pid_origem, msg.tipo);

    if (msg.tipo == MSG_LOGIN) {
        pthread_mutex_lock(&trinco);
        
        // 1. Verificar se já existe
        for (int i = 0; i < MAX_UTILIZADORES; i++) {
            if (utilizadores[i].ocupado && strcmp(utilizadores[i].username, msg.username) == 0) {
                aceito = 0; // Nome repetido
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
// ... código do login que já tinhas ...
	}
    // fecha a chaveta do if (msg.tipo == MSG_LOGIN) { ... }
    
    else if (msg.tipo == MSG_AGENDAR_VIAGEM) {
        pthread_mutex_lock(&trinco);
        printf("[PEDIDO] Novo agendamento recebido de %s (PID: %d)\n", msg.username, msg.pid_origem);
        printf("         -> Destino: %s | Distancia: %d km | Hora: %d\n", 
               msg.local_origem, msg.distancia, msg.hora_agendada);
        
        // AQUI (FUTURO): Vamos adicionar à lista de tarefas e lançar o veículo
        // Por enquanto apenas confirmamos que chegou.
        
        pthread_mutex_unlock(&trinco);
    }
        // 3. Enviar resposta
        resposta.tipo = MSG_LOGIN_RESP;
        sprintf(fifo_resp, FIFO_CLIENTE_FMT, msg.pid_origem);
        
        fdp_envia = open(fifo_resp, O_WRONLY);
        if (fdp_envia != -1) {
            write(fdp_envia, &resposta, sizeof(Mensagem));
            close(fdp_envia);
        } else {
            printf("[ERRO] Nao consegui responder ao cliente %d\n", msg.pid_origem);
        }
    }


/* --- Funções Auxiliares (Iguais às anteriores) --- */
void carregar_configuracoes() {
    char* env = getenv("NVEICULOS");
    n_veiculos_max = (env) ? atoi(env) : MAX_VEICULOS;
    if(n_veiculos_max > MAX_VEICULOS) n_veiculos_max = MAX_VEICULOS;
    pthread_mutex_init(&trinco, NULL);
    // Limpar array de users
    memset(utilizadores, 0, sizeof(utilizadores));
}

void preparar_recursos() {
    unlink(FIFO_CONTROLADOR);
    if (mkfifo(FIFO_CONTROLADOR, 0666) == -1) { perror("mkfifo"); exit(1); }
}

void limpar_recursos() {
    close(fdp_recebe);
    unlink(FIFO_CONTROLADOR);
    pthread_mutex_destroy(&trinco);
}

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

void cmd_terminar() { }
void tratar_sinal_interrupcao(int sinal) { (void)sinal; limpar_recursos(); exit(0); }
