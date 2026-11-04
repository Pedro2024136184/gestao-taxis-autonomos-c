# Makefile para Gestão de Táxis Autónomos - SO (ISEC 2025)
CC = gcc
CFLAGS = -Wall -Wextra -pthread -O2 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE

OBJS_CONTROLADOR = controlador.o utils.o
OBJS_CLIENTE = cliente.o utils.o
OBJS_VEICULO = veiculo.o utils.o

TARGETS = controlador cliente veiculo

all: $(TARGETS)

controlador: $(OBJS_CONTROLADOR)
	$(CC) $(CFLAGS) -o $@ $(OBJS_CONTROLADOR)

cliente: $(OBJS_CLIENTE)
	$(CC) $(CFLAGS) -o $@ $(OBJS_CLIENTE)

veiculo: $(OBJS_VEICULO)
	$(CC) $(CFLAGS) -o $@ $(OBJS_VEICULO)

%.o: %.c common.h utils.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGETS) fifo_*

.PHONY: all clean
