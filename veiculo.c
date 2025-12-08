#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    // argv[1] = ID, argv[2] = Origem, argv[3] = Distancia
    if (argc < 4) return 1;
    // Como redirecionámos o stdout, este printf vai para o pipe do Controlador!
    printf("Veiculo iniciado! ID=%s Origem=%s Dist=%s\n", argv[1], argv[2], argv[3]);
    
    
    char fifo_cliente[50];
    sprintf(fifo_cliente, FIFO_CLIENTE_FMT, atoi(argv[4])); // Usa o PID recebido

    int fd_cliente = open(fifo_cliente, O_WRONLY);
    if (fd_cliente != -1) {
        Mensagem msg;
        msg.tipo = MSG_INFO_VEICULO;
        msg.pid_origem = getpid(); // PID do veiculo
        strcpy(msg.dados, "Cheguei! Entra no carro (comando: entrar <destino>)");
        
        write(fd_cliente, &msg, sizeof(Mensagem));
        close(fd_cliente);
    } else {
        // Se falhar, podes reportar erro no stdout para o controlador ver
        printf("ERRO: Nao consegui contactar cliente (FIFO fechado?).\n");
        fflush(stdout);
    }

    // Simula uma viagem rápida
    sleep(5); 
    
    printf("Viagem concluída.\n");
    return 0;
}
