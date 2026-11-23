# Sistema de Segurança Residencial

Simulação de um alarme residencial em C, com detecção de movimento pela webcam usando a biblioteca [ESCAPI](https://github.com/jarikomppa/escapi).

## Funcionalidades

- **Modos:** Em casa (só a porta dispara o alarme) e Ausente (porta e movimento disparam).
- **Detecção de movimento pela câmera:** cada imagem é comparada com a anterior em tons de cinza. Se 2% ou mais dos pixels mudarem em 3 imagens seguidas, o movimento é confirmado.
- **Contagem regressiva de saída** ao armar no modo Ausente. Durante a contagem a câmera é ignorada.
- **Tempo de entrada:** depois de abrir a porta, o morador tem 10 segundos para digitar a senha.
- **Bloqueio** por 30 segundos depois de 3 senhas erradas.
- **Contador de disparos** do alarme.
- **Sem câmera:** o movimento pode ser simulado pelo teclado.

## Requisitos

- Windows (64 bits)
- Compilador GCC (por exemplo, MinGW-w64 ou w64devkit)
- Webcam (opcional)

`escapi.dll` e `msvcr120.dll` (64 bits) precisam ficar na mesma pasta do executável.

## Como compilar e executar

```
gcc main.c -o alarme.exe
.\alarme.exe
```

## Teclas durante o monitoramento

| Tecla | Ação |
|-------|------|
| P | Porta aberta |
| M | Simular movimento |
| S | Sair |

Senha padrão: `1234`. Ela é definida em `SENHA_CORRETA`, no topo de `main.c`.

## Ajuste da sensibilidade

Os valores ficam em `#define`, no topo de `main.c`:

- `LIMIAR_PIXEL`: quanto o brilho de um pixel precisa mudar (de 0 a 255).
- `PERCENTUAL_MOVIMENTO`: percentual mínimo de pixels que precisam mudar.
- `QUADROS_CONFIRMACAO`: quantas imagens seguidas com movimento confirmam a detecção.
