# Simulação de Frota de Táxis Autónomos - Sistemas Operativos (POSIX / C)

![C](https://img.shields.io/badge/C-POSIX_UNIX-00599C?style=for-the-badge&logo=c&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-Kernel_IPC-FCC624?style=for-the-badge&logo=linux&logoColor=black)
![Concurrência](https://img.shields.io/badge/Concurrency-pthreads_&_FIFOs-success?style=for-the-badge)
![Academic](https://img.shields.io/badge/ISEC-LEI-blue?style=for-the-badge)

Trabalho prático desenvolvido no âmbito da unidade curricular de **Sistemas Operativos (SO / SOTP)** da Licenciatura em Engenharia Informática no **Instituto Superior de Engenharia de Coimbra (ISEC)**, no ano letivo 2025/2026.

O projeto implementa uma plataforma distribuída em ambiente **UNIX/Linux** para gestão, escalonamento e monitorização de uma frota de táxis autónomos e clientes, tirando partido de concorrência com **threads**, sinais POSIX e canais de comunicação com **Named Pipes (FIFOs)**.

---

## 🏛️ Arquitetura do Sistema

O sistema é construído sobre uma arquitetura cliente-servidor multitarefa composta por três processos autónomos:

```
    ┌──────────────┐                       ┌──────────────┐
    │   Cliente    │ ────[ FIFO_CONTROLADOR ]───► │              │
    │  (Passageiro)│ ◄───[ FIFO_CLIENTE_pid ]──── │              │
    └──────────────┘                              │ Controlador  │
                                                  │  (Servidor)  │
    ┌──────────────┐                              │              │
    │   Veículo    │ ────[ FIFO_CONTROLADOR ]───► │              │
    │  (Táxi Aut.) │ ◄───[ FIFO_VEICULO_pid ]──── │              │
    └──────────────┘                       └──────────────┘
```

### Componentes:
1. **Controlador (`controlador.c`):**
   - Atua como o servidor central e orquestrador do ecossistema.
   - Cria o FIFO público (`fifo_controlador`) para escutar pedidos.
   - Faz a gestão dos veículos disponíveis e das viagens agendadas pelos clientes.
   - Utiliza **threads (`pthreads`)** para monitorizar comandos da consola administrativa e o ciclo de vida dos veículos concorrentemente.

2. **Veículo (`veiculo.c`):**
   - Simula um táxi autónomo que entra ao serviço no sistema.
   - Cria o seu próprio FIFO privado (`fifo_veiculo_<PID>`) para receber ordens do controlador.
   - Simula a deslocação até ao passageiro, transporte e cálculo de distâncias.

3. **Cliente (`cliente.c`):**
   - Permite ao utilizador autenticar-se, solicitar viagens imediatas ou agendadas, consultar o estado do serviço e cancelar viagens.
   - Comunica com o servidor e aguarda respostas através do seu FIFO dedicado (`fifo_cliente_<PID>`).

---

## ⚙️ Mecanismos de Comunicação & Sincronização

- **Named Pipes (FIFOs):** Comunicação bidirecional sem perda de mensagens através de pacotes de dados estruturados (`struct Mensagem`).
- **POSIX Threads (`pthread_create`, `pthread_join`, `mutex`):** Processamento concorrente no servidor sem bloqueio da receção de dados.
- **Sinais POSIX (`SIGINT`, `SIGUSR1`, etc.):** Interceção de encerramento gracioso para eliminar todos os ficheiros FIFO criados em disco e evitar fugas de recursos.

---

## 📁 Estrutura do Código

```text
SOTP/
├── common.h         # Estruturas comuns de mensagens, enumerados e constantes globais
├── controlador.c    # Servidor central e orquestração de frotas
├── cliente.c        # Interface de utilizador / passageiro
├── veiculo.c        # Simulação do táxi autónomo
├── utils.c/utils.h  # Funções auxiliares de I/O, validação e manipulação de tempo
├── Makefile         # Regras de compilação automatizada com gcc
└── RELATORIO.docx   # Relatório técnico completo de arquitetura e testes
```

---

## 🚀 Como Compilar e Executar

### Pré-requisitos
- Ambiente Linux (Ubuntu, Debian, Fedora, Arch ou WSL no Windows)
- Compilador `gcc` e utilitário `make`

### Compilação

Basta executar no terminal:
```bash
make
```
Isto gerará os três executáveis: `controlador`, `cliente` e `veiculo`. Para limpar os binários:
```bash
make clean
```

### Execução

Abra diferentes terminais (ou abas):

1. **Terminal 1 – Iniciar o Controlador:**
   ```bash
   ./controlador
   ```

2. **Terminal 2 – Lançar um ou mais Veículos:**
   ```bash
   ./veiculo
   ```

3. **Terminal 3 – Lançar um Cliente para solicitar viagens:**
   ```bash
   ./cliente
   ```

---

## 👥 Autores

- **Pedro Costa** (Nº 2024136184) – Licenciatura em Engenharia Informática, ISEC
- **Afonso Henriques** (Nº 2024138824) – Licenciatura em Engenharia Informática, ISEC
