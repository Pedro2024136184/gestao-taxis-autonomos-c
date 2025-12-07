#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    // argv[1] = ID, argv[2] = Origem, argv[3] = Distancia
    if (argc < 4) return 1;

    // Como redirecionámos o stdout, este printf vai para o pipe do Controlador!
    printf("Veiculo iniciado! ID=%s Origem=%s Dist=%s\n", argv[1], argv[2], argv[3]);
    
    // Simula uma viagem rápida
    sleep(5); 
    
    printf("Viagem concluída.\n");
    return 0;
}
