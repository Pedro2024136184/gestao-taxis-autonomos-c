#include "utils.h"

// Implementação da função de exemplo
void log_debug(const char *mensagem) {
    fprintf(stderr, "[DEBUG] %s\n", mensagem);
}
