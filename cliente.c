#include "common.h"
#include "utils.h"

// Variáveis globais para limpeza
char fifo_nome_cliente[50];
int fdp_cliente = -1; 
int fdp_controlador = -1;

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
             // O utilizador quer sair
             // Devíamos avisar o controlador, mas para já saímos só
             break;
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
        }
        else {
            printf("[ERRO] Comando desconhecido. Tente: agendar, terminar\n");
        }
    }

    limpar_recursos();
    return 0;
}
