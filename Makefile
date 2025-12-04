# --- Variáveis de Compilação ---
CC = gcc
CFLAGS = -Wall -Wextra -g -pthread

# --- Ficheiros Objeto ---
OBJS_CONTROLADOR = controlador.o utils.o
OBJS_CLIENTE = cliente.o utils.o
OBJS_VEICULO = veiculo.o utils.o

# --- Targets Principais ---
all: controlador cliente veiculo

controlador: $(OBJS_CONTROLADOR)
	$(CC) $(CFLAGS) -o controlador $(OBJS_CONTROLADOR)

cliente: $(OBJS_CLIENTE)
	$(CC) $(CFLAGS) -o cliente $(OBJS_CLIENTE)

veiculo: $(OBJS_VEICULO)
	$(CC) $(CFLAGS) -o veiculo $(OBJS_VEICULO)

# --- Regras Genéricas ---
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o controlador cliente veiculo
