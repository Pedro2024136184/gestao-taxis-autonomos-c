#include "common.h"
#include "utils.h"

int main(int argc, char *argv[]) {
    // Estas linhas evitam o erro de "unused parameter" enquanto não usamos as variáveis
    (void)argc;
    (void)argv;

    // O veículo comunica via stdout, por isso usamos printf
    printf("Veiculo iniciado (PID: %d)...\n", getpid());

    // Código futuro virá aqui...
    
    return 0;
}
