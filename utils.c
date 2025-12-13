#include "utils.h"

// Logs a debug message to stderr with [DEBUG] prefix
void log_debug(const char *mensagem) {
    fprintf(stderr, "[DEBUG] %s\n", mensagem);
}
