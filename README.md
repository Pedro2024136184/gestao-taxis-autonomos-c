# Simulação de Frota de Táxis Autónomos - Sistemas Operativos (2025)

![C](https://img.shields.io/badge/C-POSIX_UNIX-00599C?style=for-the-badge&logo=c&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-Kernel_IPC-FCC624?style=for-the-badge&logo=linux&logoColor=black)
![Concurrency](https://img.shields.io/badge/Concurrency-pthreads_&_FIFOs-success?style=for-the-badge)
![Academic](https://img.shields.io/badge/ISEC-LEI-blue?style=for-the-badge)
![Status](https://img.shields.io/badge/Versão-Melhorada-success?style=for-the-badge)

---

## 📌 Enquadramento Académico
* **Cadeira:** Sistemas Operativos (SO)
* **Ano Letivo / Realização:** 2025
* **Instituição:** Instituto Superior de Engenharia de Coimbra (ISEC)
* **Curso:** Licenciatura em Engenharia Informática

---

## 🚕 Descrição da Plataforma Concorrente
O projeto implementa uma plataforma distribuída em ambiente **UNIX / Linux** concebida para a gestão, escalonamento e monitorização em tempo real de uma frota de táxis autónomos e dos respetivos passageiros.

O sistema divide-se em três processos desacoplados:
1. **Controlador Central (`controlador`):**
   * Processo servidor multithread (*pthreads*) responsável pela receção de mensagens dos clientes através de um canal central (`FIFO_CONTROLADOR`).
   * Escalonamento temporal de viagens com simulador de relógio global.
   * Criação e monitorização dinâmica de processos filhos (`fork` / `exec`) que representam os táxis em trânsito.
   * Interface administrativa interativa para o operador do sistema.
2. **Interface do Cliente (`cliente`):**
   * Processo utilizador que estabelece comunicação bidirecional através de um canal FIFO dedicado (`fifo_cliente_<PID>`).
   * Permite efetuar login, agendar novos trajetos, consultar o estado de viagens e cancelar pedidos.
3. **Módulo do Veículo Autónomo (`veiculo`):**
   * Processo autónomo criado sob demanda pelo controlador.
   * Notifica o cliente da chegada ao ponto de partida, reporta periodicamente o progresso da viagem quilómetro a quilómetro através de um pipe anónimo e encerra com contabilização rigorosa de quilometragem.

---

## 🚀 Comparação de Versões: Original vs. Versão Melhorada

| Componente / Mecanismo | Versão Original (Ramo `main`) | Versão Melhorada (Ramo `versao-melhorada`) |
| :--- | :--- | :--- |
| **Encerramento e Sinais POSIX** | Usava a função legada `signal()` sem proteção contra sinais concorrentes e eliminava processos de táxis abruptamente com `SIGKILL` sem libertação de recursos. | Adoção da API moderna **`sigaction` com máscara `SA_RESTART`** para `SIGINT`, `SIGTERM` e `SIGCHLD`. Transição graciosa: envio prévio de `SIGTERM` para que os táxis fechem os seus próprios FIFOs e ficheiros. |
| **Comunicação de Encerramento aos Clientes** | Se o controlador fosse desligado, os processos clientes ficavam bloqueados no terminal indefinidamente à espera de resposta no FIFO. | **Notificação de broadcast `MSG_ENCERRAR`**: o controlador informa todos os clientes com sessão ativa antes de desligar, permitindo aos clientes terminarem ordenadamente. |
| **Gestão do Fim de Ficheiro (EOF) no FIFO do Cliente** | O cliente abria o seu FIFO apenas em leitura (`O_RDONLY`), gerando ciclos infinitos de leitura quando não havia processos a escrever. | Abertura do FIFO em modo Leitura/Escrita (`O_RDWR`), prevenindo loops ocupados de CPU em condição de EOF. |
| **Cancelamento de Threads Trabalhadoras** | Ao terminar a thread `t_admin`, as restantes três threads (`t_clientes`, `t_monitor`, `t_tempo`) ficavam órfãs ou eram interrompidas sem controlo. | Uso explícito de `pthread_cancel()` sobre as threads operárias após a saída do administrador, garantindo desalocação determinística de semáforos e mutexes. |
| **Limpeza Geral de FIFOs Residuais** | FIFOs antigos (`fifo_*`) podiam permanecer no sistema de ficheiros em caso de encerramento forçado. | Alvo `make clean` atualizado para remover automaticamente `fifo_*` residuais e rotina centralizada de `unlink()` em todos os processos. |

---

## 📂 Arquitetura de Ficheiros

```text
gestao-taxis-autonomos-c/
├── common.h         # Estruturas de dados (Mensagem), constantes e tipos IPC
├── utils.c/.h       # Funções utilitárias e geradores auxiliares
├── controlador.c    # Servidor multithread, escalonador e interface admin
├── cliente.c        # Cliente interativo com thread ouvinte de notificações
├── veiculo.c        # Processo representativo de cada táxi autónomo
├── Makefile         # Script de compilação POSIX com flags -pthread e -O2
├── RELATORIO.docx   # Documentação formal e especificação de requisitos
└── README.md        # Documentação completa do projeto
```

---

## 💻 Como Compilar e Executar

1. **Clonar e aceder ao repositório:**
   ```bash
   git clone https://github.com/Pedro2024136184/gestao-taxis-autonomos-c.git
   cd gestao-taxis-autonomos-c
   ```

2. **Alternar entre as versões:**
   * Para a versão original:
     ```bash
     git checkout main
     ```
   * Para a versão melhorada:
     ```bash
     git checkout versao-melhorada
     ```

3. **Compilar com Make:**
   ```bash
   make
   ```

4. **Executar a simulação (em terminais separados):**
   * **Terminal 1 (Controlador):**
     ```bash
     ./controlador
     ```
   * **Terminal 2 (Cliente 1):**
     ```bash
     ./cliente Alice
     # Dentro do cliente:
     agendar Baixa 5 2
     ```
   * **Terminal 3 (Cliente 2):**
     ```bash
     ./cliente Bob
     # Dentro do cliente:
     agendar Celas 8 4
     ```

---

## 👨‍💻 Autor
* **Pedro Costa** (Nº 2024136184) – Licenciatura em Engenharia Informática, ISEC.
