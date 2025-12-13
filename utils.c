#include "utils.h"

void log_debug(const char *mensagem) {
    fprintf(stderr, "[DEBUG] %s\n", mensagem);
}
