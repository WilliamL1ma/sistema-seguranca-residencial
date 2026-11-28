/******************************************************************************

                        SISTEMA DE SEGURANÇA RESIDENCIAL
                     (com detecção de movimento por câmera)

   Compilar (no terminal, dentro da pasta do projeto):
       gcc main.c -o alarme.exe
   Executar (no PowerShell é obrigatório o ".\" na frente):
       .\alarme.exe

   A pasta precisa conter escapi.dll e msvcr120.dll (versão 64 bits).

*******************************************************************************/

#include <stdio.h>   //Biblioteca adicionada (printf, scanf, getchar)
#include <stdlib.h>  //Biblioteca adicionada (exit: encerra o programa / malloc e free: reservam e liberam memória)
#include <time.h>    //Biblioteca adicionada (time e difftime: contagem regressiva, tempo de entrada e bloqueio)
#include <windows.h> //Biblioteca adicionada (LoadLibrary e GetProcAddress: carregam a escapi.dll / Sleep: pausa em milissegundos)
#include <conio.h>   //Biblioteca adicionada (_kbhit e _getch: leem uma tecla sem travar o programa)

#define SENHA_CORRETA 1234   //Define uma constante: para trocar a senha, basta alterar este valor
#define TEMPO_ENTRADA 10     //Define uma constante: segundos que o morador tem para digitar a senha depois de abrir a porta
#define TEMPO_BLOQUEIO 30    //Define uma constante: segundos de bloqueio depois de 3 senhas erradas
#define FREQUENCIA_ALARME 1000 //Frequência do bip em Hertz
#define DURACAO_BIP_ALARME 500 //Duração de cada bip em milissegundos

//----------------------------- CONFIGURAÇÃO DA CÂMERA -----------------------------
#define LARGURA_CAMERA 160         //Largura da imagem capturada (imagem pequena = comparação mais rápida)
#define ALTURA_CAMERA 120          //Altura da imagem capturada
#define LIMIAR_PIXEL 30            //Quanto o brilho de um pixel (0 a 255) precisa mudar para ser considerado "mudou"
#define PERCENTUAL_MOVIMENTO 2.0   //Percentual mínimo de pixels que mudaram para considerar que houve movimento
#define QUADROS_CONFIRMACAO 3      //Quantos quadros seguidos com movimento são necessários para confirmar (evita alarme falso)
#define QUADROS_AQUECIMENTO 20     //Quadros descartados ao ligar a câmera (ela ainda está ajustando brilho e foco)

#define EVENTO_MOVIMENTO 1   //Código devolvido pelo monitoramento: movimento confirmado pela câmera
#define EVENTO_PORTA 2       //Código devolvido pelo monitoramento: porta aberta (tecla P)
#define EVENTO_SAIR 3        //Código devolvido pelo monitoramento: usuário pediu para encerrar (tecla S)

//----------------------------- BIBLIOTECA ESCAPI -----------------------------
//A escapi.dll é uma biblioteca que captura imagens da webcam no Windows.
//Ela é carregada enquanto o programa roda: cada função dela é guardada em um "ponteiro de função".

struct SimpleCapParams       //Estrutura exigida pela escapi para descrever onde e em que tamanho a imagem será guardada
{
    int *mTargetBuf;         //Vetor onde a escapi escreve a imagem (cada int é um pixel no formato 0xAARRGGBB)
    int mWidth;              //Largura da imagem
    int mHeight;             //Altura da imagem
};

typedef void (*initCOMProc)(void);                                                    //Tipo da função que prepara o Windows para usar a câmera
typedef int  (*countCaptureDevicesProc)(void);                                        //Tipo da função que conta quantas câmeras existem
typedef int  (*initCaptureProc)(unsigned int camera, struct SimpleCapParams *params); //Tipo da função que abre a câmera
typedef void (*deinitCaptureProc)(unsigned int camera);                               //Tipo da função que fecha a câmera
typedef void (*doCaptureProc)(unsigned int camera);                                   //Tipo da função que pede uma nova foto (quadro)
typedef int  (*isCaptureDoneProc)(unsigned int camera);                               //Tipo da função que avisa se a foto já ficou pronta
typedef void (*getCaptureDeviceNameProc)(unsigned int camera, char *nome, int tamanho); //Tipo da função que devolve o nome da câmera

initCaptureProc initCapture;                 //Ponteiros que vão guardar o endereço de cada função da DLL
deinitCaptureProc deinitCapture;
doCaptureProc doCapture;
isCaptureDoneProc isCaptureDone;

struct SimpleCapParams parametros;           //Parâmetros da captura (vetor da imagem, largura e altura)
unsigned char *referencia = NULL;            //Imagem anterior em tons de cinza: o quadro atual é comparado com ela
int temReferencia = 0;                       //0 = ainda não há imagem anterior para comparar / 1 = já existe
int cameraLigada = 0;                        //0 = sem câmera (usa só o teclado) / 1 = câmera funcionando

//Função que liga a câmera. Devolve 1 se conseguiu e 0 se não encontrou a DLL ou nenhuma câmera.
int iniciarCamera()
{
    HMODULE dll;                                 //Guarda a DLL carregada
    initCOMProc initCOM;                         //Função que prepara o Windows para a captura
    countCaptureDevicesProc countCaptureDevices; //Função que conta as câmeras
    getCaptureDeviceNameProc getCaptureDeviceName; //Função que devolve o nome da câmera
    char nome[256];                              //Armazena o nome da câmera

    dll = LoadLibraryA("escapi.dll");            //Carrega a DLL que está na mesma pasta do programa
    if (dll == NULL) {                           //Não encontrou o arquivo escapi.dll:
        printf("Aviso: escapi.dll nao encontrada.\n");
        return 0;                                //Devolve 0 (sem câmera)
    }

    initCOM             = (initCOMProc)GetProcAddress(dll, "initCOM");                          //Busca cada função dentro da DLL pelo nome
    countCaptureDevices = (countCaptureDevicesProc)GetProcAddress(dll, "countCaptureDevices");
    getCaptureDeviceName = (getCaptureDeviceNameProc)GetProcAddress(dll, "getCaptureDeviceName");
    initCapture         = (initCaptureProc)GetProcAddress(dll, "initCapture");
    deinitCapture       = (deinitCaptureProc)GetProcAddress(dll, "deinitCapture");
    doCapture           = (doCaptureProc)GetProcAddress(dll, "doCapture");
    isCaptureDone       = (isCaptureDoneProc)GetProcAddress(dll, "isCaptureDone");

    if (initCOM == NULL || countCaptureDevices == NULL || getCaptureDeviceName == NULL || initCapture == NULL
        || deinitCapture == NULL || doCapture == NULL || isCaptureDone == NULL) {   //Alguma função não foi encontrada:
        printf("Aviso: escapi.dll invalida ou de versao diferente.\n");
        return 0;
    }

    initCOM();                                   //Prepara o Windows para usar a câmera

    if (countCaptureDevices() < 1) {             //Nenhuma câmera conectada:
        printf("Aviso: nenhuma camera encontrada.\n");
        return 0;
    }

    parametros.mWidth = LARGURA_CAMERA;          //Define o tamanho da imagem desejada
    parametros.mHeight = ALTURA_CAMERA;
    parametros.mTargetBuf = (int *)malloc(LARGURA_CAMERA * ALTURA_CAMERA * sizeof(int));     //Reserva memória para a imagem colorida
    referencia = (unsigned char *)malloc(LARGURA_CAMERA * ALTURA_CAMERA);                    //Reserva memória para a imagem anterior (cinza)

    if (parametros.mTargetBuf == NULL || referencia == NULL) {   //Faltou memória:
        printf("Aviso: memoria insuficiente para a camera.\n");
        return 0;
    }

    if (initCapture(0, &parametros) == 0) {      //Tenta abrir a primeira câmera (número 0):
        printf("Aviso: nao foi possivel abrir a camera.\n");
        return 0;
    }

    getCaptureDeviceName(0, nome, sizeof(nome)); //Lê o nome da câmera
    printf("Camera conectada: %s\n", nome);      //Mostra o nome da câmera
    return 1;                                    //Devolve 1 (câmera funcionando)
}

//Função que tira uma foto (quadro). Devolve 1 se a foto ficou pronta e 0 se a câmera não respondeu.
int capturarQuadro()
{
    DWORD inicio = GetTickCount();               //Guarda o horário (em milissegundos) do pedido

    doCapture(0);                                //Pede uma nova foto para a câmera
    while (isCaptureDone(0) == 0) {              //Enquanto a foto ainda não ficou pronta:
        if (GetTickCount() - inicio > 1000) {    //Se já passou 1 segundo, desiste
            return 0;
        }
        Sleep(1);                                //Espera 1 milissegundo antes de verificar de novo
    }
    return 1;                                    //Foto pronta
}

//Função que descarta os primeiros quadros, enquanto a câmera ainda ajusta brilho e foco.
void aquecerCamera()
{
    int i;                                       //Contador de quadros
    for (i = 0; i < QUADROS_AQUECIMENTO; i++) {  //Captura e descarta QUADROS_AQUECIMENTO quadros
        capturarQuadro();
    }
    temReferencia = 0;                           //A próxima foto vira a nova imagem de referência
}

//Função que compara a foto atual com a anterior.
//Devolve o percentual (0 a 100) de pixels que mudaram de brilho. Devolve -1 se a foto falhou.
double medirMudanca()
{
    int i;                                       //Posição do pixel no vetor
    int total = LARGURA_CAMERA * ALTURA_CAMERA;  //Quantidade total de pixels da imagem
    int mudaram = 0;                             //Quantos pixels mudaram mais que o LIMIAR_PIXEL
    int pixel, vermelho, verde, azul, cinza, diferenca;

    if (capturarQuadro() == 0) {                 //Câmera não respondeu:
        return -1;
    }

    for (i = 0; i < total; i++) {                //Percorre todos os pixels da imagem
        pixel = parametros.mTargetBuf[i];        //Pixel no formato 0xAARRGGBB
        vermelho = (pixel >> 16) & 0xFF;         //Separa o componente vermelho (bits 16 a 23)
        verde    = (pixel >> 8) & 0xFF;          //Separa o componente verde (bits 8 a 15)
        azul     = pixel & 0xFF;                 //Separa o componente azul (bits 0 a 7)
        cinza = (vermelho + verde + azul) / 3;   //Converte para tons de cinza (média das três cores)

        if (temReferencia == 1) {                //Se existe uma imagem anterior, compara:
            diferenca = abs(cinza - referencia[i]);   //Quanto o brilho deste pixel mudou
            if (diferenca > LIMIAR_PIXEL) {      //Mudou o suficiente:
                mudaram++;                       //Conta mais um pixel alterado
            }
        }
        referencia[i] = (unsigned char)cinza;    //A foto atual vira a referência para a próxima comparação
    }

    if (temReferencia == 0) {                    //Primeira foto: ainda não havia com o que comparar
        temReferencia = 1;
        return 0;
    }

    return 100.0 * mudaram / total;              //Devolve o percentual de pixels que mudaram
}

//Função que desliga a câmera e libera a memória.
void encerrarCamera()
{
    if (cameraLigada == 1) {                     //Só fecha se a câmera foi aberta
        deinitCapture(0);
    }
    free(parametros.mTargetBuf);                 //Libera a memória da imagem (free(NULL) não faz nada)
    free(referencia);
}

//----------------------------- FUNÇÕES DO SISTEMA -----------------------------

//Função que lê um número inteiro dentro de um intervalo (min até max).
//Recebe a mensagem a ser mostrada, o menor e o maior valor permitidos. Devolve o valor válido.
int lerInteiro(const char *mensagem, int min, int max)
{
    int valor;                                   //Armazena o número digitado
    int lido;                                    //Armazena o retorno do scanf (1 = leu um número / 0 = não leu / EOF = entrada acabou)

    while (1) {                                  //Repete para sempre; só sai pelo "return" quando o valor for válido
        printf("%s", mensagem);                  //Mostra o texto informativo recebido da chamada
        lido = scanf("%d", &valor);              //scanf devolve quantos valores conseguiu ler; se digitar letra, devolve 0

        if (lido == EOF) {                       //Entrada encerrada (nada mais a ler):
            exit(0);                             //sai do programa para não repetir para sempre
        }

        if (lido == 1 && valor >= min && valor <= max) {  //Leu um número E ele está dentro do intervalo permitido:
            return valor;                        //Devolve o valor e encerra a função
        }

        while (getchar() != '\n');               //Limpa o buffer: descarta tudo o que sobrou digitado até o Enter
        printf("Valor invalido!\n");             //Mensagem de aviso: usuário deve inserir o input permitido
    }
}

//Função que espera a quantidade de segundos informada.
void esperar(int segundos)
{
    time_t inicio = time(NULL);                  //Guarda o horário atual (momento em que a espera começou)
    while (difftime(time(NULL), inicio) < segundos);  //Repete enquanto o tempo passado desde o início for menor que o desejado
}

//----------------------------- SOM DO ALARME -----------------------------
//O som toca em uma "thread": uma segunda linha de execução que roda ao mesmo tempo que o main.
//Assim o alarme continua apitando enquanto o programa espera o usuário digitar no scanf.
//Não usa arquivo MP3 nem biblioteca externa: Beep e CreateThread fazem parte da Windows API.

volatile int alarmeAtivo = 0;   //0 = desligado / 1 = ligado. É global para a thread do som também enxergar.
                                //"volatile" avisa o compilador que outra thread pode mudar o valor a qualquer momento
HANDLE threadSom = NULL;        //Identificador da thread do som (NULL = nenhuma thread criada)

//Função executada pela thread do som: repete o bip enquanto o alarme estiver ativo.
DWORD WINAPI tocarAlarme(LPVOID parametro)
{
    (void)parametro;                                  //Parâmetro exigido pelo Windows, mas não usado aqui
    while (alarmeAtivo == 1) {                        //Enquanto o alarme estiver ligado:
        Beep(FREQUENCIA_ALARME, DURACAO_BIP_ALARME);  //Emite um bip agudo
        Sleep(DURACAO_BIP_ALARME);                    //Intervalo silencioso antes do próximo bip
    }
    return 0;                                         //Alarme desligado: a thread termina
}

//Função que liga o alarme e começa a tocar o som sem travar o programa.
void dispararAlarme()
{
    if (threadSom != NULL) {                          //Se sobrou uma thread de um disparo anterior:
        WaitForSingleObject(threadSom, INFINITE);     //Espera ela terminar (no máximo o último bip)
        CloseHandle(threadSom);                       //Libera o identificador
        threadSom = NULL;
    }

    alarmeAtivo = 1;                                  //Registra que o alarme foi acionado
    threadSom = CreateThread(NULL, 0, tocarAlarme, NULL, 0, NULL);   //Cria a thread que toca o som
    if (threadSom == NULL) {                          //Não conseguiu criar a thread:
        printf("Aviso: nao foi possivel tocar o som do alarme.\n");
    }
}

//Função que desliga o alarme. A thread do som percebe a mudança e para sozinha.
void desligarAlarme()
{
    alarmeAtivo = 0;                                  //A thread sai do while depois do bip atual
}

//Função que simula o envio de um alerta para o celular do proprietário.
//Recebe o motivo do disparo (ex.: "Movimento detectado pela câmera") e mostra a mensagem com o horário.
void notificarProprietario(const char *motivo)
{
    time_t agora = time(NULL);                   //Horário atual
    struct tm *hora = localtime(&agora);         //Separa o horário em hora, minuto e segundo

    printf("\nEnviando alerta para o celular do proprietário...\n");
    esperar(1);                                  //Pequena pausa para simular o envio
    printf("+------------------------------------------------+\n");
    printf("| SMS ENVIADO AO PROPRIETÁRIO                    |\n");
    printf("+------------------------------------------------+\n");
    printf("  Alerta de segurança às %02d:%02d:%02d\n", hora->tm_hour, hora->tm_min, hora->tm_sec);
    printf("  Motivo: %s\n", motivo);
    printf("  Verifique sua residência ou ligue 190.\n");
    printf("+------------------------------------------------+\n");
}

//Função da contagem regressiva de saída (usada ao armar o sistema no modo Ausente).
//Durante a contagem a câmera não é verificada, para o próprio morador não disparar o alarme ao sair.
void contagemSaida()
{
    int i;                                       //Contador da contagem regressiva

    printf("\nSistema sera armado. Saia do local!\n");   //Aviso para o morador sair antes de o sistema armar
    for (i = 5; i >= 1; i--) {                   //Contagem regressiva: começa em 5 e vai diminuindo de 1 em 1 até chegar a 1
        printf("Armando em %d...\n", i);         //Mostra quantos segundos faltam
        esperar(1);                              //Espera 1 segundo antes do próximo número
    }
}

//Função que pede a senha. Devolve 1 se a senha estiver correta e 0 se errou 3 vezes, estourou o tempo ou está bloqueado.
//tentativas: quantas vezes errou / bloqueadoAte: horário em que o bloqueio termina / tempoLimite: segundos para digitar (0 = sem limite)
int pedirSenha(int *tentativas, time_t *bloqueadoAte, int tempoLimite)
{
    int senha;                                   //Armazena a senha digitada
    time_t inicio;                               //Guarda o horário em que a senha começou a ser pedida

    if (time(NULL) < *bloqueadoAte) {            //Se o horário atual ainda é anterior ao fim do bloqueio:
        printf("\nSistema bloqueado! Aguarde %d segundos.\n", (int)(*bloqueadoAte - time(NULL)));   //Informa quanto falta
        return 0;                                //Não pede a senha: devolve 0 (não autorizado)
    }

    inicio = time(NULL);                         //Registra o horário de início para medir o tempo limite

    while (*tentativas < 3) {                    //Enquanto o total de 3 tentativas não é alcançado: solicite senha.
        senha = lerInteiro("\nDigite a senha: ", 0, 999999999);   //Lê a senha; aceita apenas números

        if (tempoLimite > 0 && difftime(time(NULL), inicio) > tempoLimite) {   //Se há tempo limite e ele já passou:
            printf("TEMPO ESGOTADO!\n");         //Avisa que o tempo acabou
            return 0;                            //Devolve 0 (não autorizado), mesmo que a senha esteja certa
        }

        if (senha == SENHA_CORRETA) {            //Comparação: "A senha é a senha correta?"
            *tentativas = 0;                     //Zera as tentativas erradas, pois o acesso foi autorizado
            printf("Acesso Autorizado! :P\n");   //Caso sim, texto de afirmação para acesso liberado.
            return 1;                            //Devolve 1 (autorizado)
        }

        (*tentativas)++;                         //Incrementa o contador de tentativas em 1.
        printf("SENHA INCORRETA! >:(\n");        //Apresenta mensagem de aviso.
    }

    *bloqueadoAte = time(NULL) + TEMPO_BLOQUEIO; //3 erros: define o horário em que o bloqueio termina (agora + TEMPO_BLOQUEIO segundos)
    *tentativas = 0;                             //Zera as tentativas: ao fim do bloqueio o usuário volta a ter 3 chances
    printf("\nXXX SISTEMA BLOQUEADO POR %d SEGUNDOS! XXX\n", TEMPO_BLOQUEIO);   //Mensagem indicando acesso bloqueado.
    return 0;                                    //Devolve 0 (não autorizado)
}

//Função de monitoramento contínuo: fica verificando a câmera e o teclado SEM travar o programa,
//até acontecer algo importante. Devolve EVENTO_MOVIMENTO, EVENTO_PORTA ou EVENTO_SAIR.
int aguardarEvento(int modo)
{
    int tecla;                                   //Armazena a tecla apertada
    int quadrosSeguidos = 0;                     //Quantos quadros seguidos tiveram movimento
    int avisouEmCasa = 0;                        //1 = já avisou que o movimento foi ignorado (evita repetir a mensagem)
    int movimento;                               //1 = movimento neste instante / 0 = sem movimento
    double mudanca;                              //Percentual de pixels que mudaram na câmera

    printf("\nMonitorando... Teclas: [P] Porta aberta  [M] Simular movimento  [S] Sair\n");

    if (cameraLigada == 1) {                     //Antes de começar, a próxima foto vira a referência,
        temReferencia = 0;                       //para não comparar com uma foto antiga (de antes da senha, por exemplo)
    }

    while (1) {                                  //Repete até devolver algum evento
        movimento = 0;                           //Começa considerando que não há movimento

        if (_kbhit()) {                          //Se alguma tecla foi apertada (não trava se nenhuma foi):
            tecla = _getch();                    //Lê a tecla sem precisar de Enter
            if (tecla == 'p' || tecla == 'P') {  //P = porta aberta
                printf("\n");
                return EVENTO_PORTA;
            }
            if (tecla == 's' || tecla == 'S') {  //S = sair do monitoramento
                printf("\n");
                return EVENTO_SAIR;
            }
            if (tecla == 'm' || tecla == 'M') {  //M = simula o sensor (útil para testar sem câmera)
                movimento = 1;
                quadrosSeguidos = QUADROS_CONFIRMACAO;   //Movimento simulado já vem confirmado
            }
        }

        if (cameraLigada == 1 && movimento == 0) {   //Com câmera: mede quanto a imagem mudou
            mudanca = medirMudanca();
            if (mudanca < 0) {                   //A câmera parou de responder:
                printf("\nAviso: a camera parou de responder. Use a tecla M para simular.\n");
                cameraLigada = 0;
            } else {
                printf("\rCamera: %5.1f%% da imagem mudou   ", mudanca);   //Mostra o valor na mesma linha (\r volta ao início)
                if (mudanca >= PERCENTUAL_MOVIMENTO) {   //Mudou o suficiente: há movimento neste quadro
                    movimento = 1;
                    quadrosSeguidos++;           //Conta mais um quadro seguido com movimento
                } else {
                    quadrosSeguidos = 0;         //Sem movimento: zera a sequência
                    avisouEmCasa = 0;            //Permite avisar de novo no próximo movimento
                }
            }
        } else if (cameraLigada == 0) {
            Sleep(50);                           //Sem câmera: pequena pausa para não ocupar 100% do processador
        }

        if (movimento == 1 && quadrosSeguidos >= QUADROS_CONFIRMACAO) {   //Movimento confirmado:
            if (modo == 1) {                     //Modo Em casa: movimento é normal e não dispara o alarme
                if (avisouEmCasa == 0) {
                    printf("\nModo Em casa: movimento ignorado.\n");
                    avisouEmCasa = 1;
                }
            } else {                             //Modo Ausente: movimento é uma possível invasão
                printf("\nMOVIMENTO DETECTADO!\n");
                return EVENTO_MOVIMENTO;
            }
        }
    }
}

int main()                    //Início do programa;
{
    int modo;                 //Criação de variável (1 = Em casa: só a porta dispara / 2 = Ausente: porta e movimento disparam)
    int evento;               //Criação de variável (O que o monitoramento detectou: movimento, porta ou sair)
    int tentativas = 0;       //Criação de variável (Quantas vezes a senha foi digitada e estava incorreta - inicia 0)
    time_t bloqueadoAte = 0;  //Criação de variável (Horário em que o bloqueio termina - inicia 0 = sem bloqueio)
    int armado = 0;           //Criação de variável (0 = sistema desarmado / 1 = sistema armado e monitorando)
    int disparos = 0;         //Criação de variável (Quantas vezes o alarme foi acionado - inicia 0)
    int desejaSenha;          //Criação de variável (Armazena a escolha do usuário: digitar a senha ou não)
    int rearmar;              //Criação de variável (Armazena a escolha do usuário: armar o sistema novamente ou não)
    int continuar = 1;        //Controla a continuidade do monitoramento.

    SetConsoleOutputCP(CP_UTF8);   //Faz o terminal do Windows mostrar acentos corretamente

    printf("====================================\n"
           "        SISTEMA DE SEGURANÇA\n"                       //Apresenta texto "SISTEMA DE SEGURANÇA"
           "====================================\n");

    cameraLigada = iniciarCamera();                   //Tenta ligar a câmera
    if (cameraLigada == 1) {
        printf("Ajustando a camera...\n");
        aquecerCamera();                              //Descarta os primeiros quadros
    } else {
        printf("Sem camera: o movimento sera simulado pela tecla M.\n");
    }

    modo = lerInteiro("\nModo (1-Em casa / 2-Ausente): ", 1, 2);   //Lê o modo de funcionamento; só aceita 1 ou 2

    if (modo == 2) {                                  //Tempo de saída: acontece apenas no modo Ausente
        contagemSaida();                              //Executa a contagem regressiva para o morador sair
    }
    armado = 1;                                       //Registra que o sistema está armado
    printf("SISTEMA ARMADO!\n");                      //Fim da contagem: sistema pronto para monitorar

    while (continuar == 1) {                          // Repete o monitoramento enquanto continuar for igual a 1.

        if (armado == 0) {                            //Sistema desarmado (morador entrou e digitou a senha):
            printf("\nSISTEMA DESARMADO: sensores e camera desligados.\n");   //Informa que nada está sendo monitorado
            rearmar = lerInteiro("\nArmar o sistema? (1-Sim/0-Nao): ", 0, 1);   //Pergunta se quer armar o sistema novamente

            if (rearmar == 1) {                       //Usuário escolheu armar (1 = Sim):
                if (modo == 2) {                      //No modo Ausente:
                    contagemSaida();                  //Executa a contagem regressiva para o morador sair
                }
                armado = 1;                           //Registra que o sistema está armado
                printf("SISTEMA ARMADO!\n");          //Informa que o sistema voltou a monitorar
            } else {
                continuar = lerInteiro("\nContinuar monitoramento? (1-Sim/0-Nao): ", 0, 1);   //Pergunta se o programa continua
            }

        } else if (alarmeAtivo == 1) {                //Sistema armado e alarme tocando: permite tentar a senha
            printf("\nALARME CONTINUA ATIVO!\n");     //Mantém o alarme ativo e indica mensagem de aviso (o som segue tocando na thread).

            if (time(NULL) < bloqueadoAte) {          //Se o sistema ainda está bloqueado:
                printf("\nSistema bloqueado! Aguarde %d segundos para tentar a senha.\n", (int)(bloqueadoAte - time(NULL)));   //Informa quanto falta
                continuar = lerInteiro("\nContinuar monitoramento? (1-Sim/0-Nao): ", 0, 1);

            } else {                                  //Sistema liberado:
                desejaSenha = lerInteiro("\nDigitar senha para desativar o alarme? (1-Sim/0-Nao): ", 0, 1);   //Pergunta se quer digitar a senha agora

                if (desejaSenha == 1 && pedirSenha(&tentativas, &bloqueadoAte, 0) == 1) {   //Quis digitar e a senha está correta (sem tempo limite):
                    desligarAlarme();                 //Altera o estado do alarme para 0 (desligado) e para o som.
                    armado = 0;                       //Desarma o sistema junto com o alarme
                    printf("\nALARME DESATIVADO COM SUCESSO!\n");   //Informa que o alarme foi desativado.
                } else {
                    continuar = lerInteiro("\nContinuar monitoramento? (1-Sim/0-Nao): ", 0, 1);
                }
            }

        } else {                                      //Sistema armado e alarme desligado: monitora câmera e porta
            evento = aguardarEvento(modo);            //Fica monitorando até a câmera ou o teclado indicar algo

            if (evento == EVENTO_SAIR) {              //Usuário apertou S:
                continuar = 0;                        //Encerra o monitoramento

            } else if (evento == EVENTO_MOVIMENTO) {  //Câmera confirmou movimento no modo Ausente:
                disparos++;                           //Incrementa o contador de disparos em 1
                dispararAlarme();                     //Registra que o alarme foi acionado e começa o som.

                printf("\nALERTA! POSSÍVEL INVASÃO!\n");          //Texto de alerta caso alarme ativado.
                printf("ALERTA ACIONADO!\n");                     //Texto de alerta caso alarme ativado.
                notificarProprietario("Movimento detectado pela câmera");   //Avisa o dono da casa pelo celular

            } else {                                  //Porta aberta com o alarme desligado: começa o tempo de entrada
                printf("\nPORTA ABERTA! Digite a senha em ate %d segundos.\n", TEMPO_ENTRADA);   //Avisa o tempo que o morador tem

                if (pedirSenha(&tentativas, &bloqueadoAte, TEMPO_ENTRADA) == 1) {   //Senha correta dentro do tempo:
                    armado = 0;                       //Desarma o sistema: o morador chegou
                    printf("\nSISTEMA DESARMADO!\n"); //Informa que o sistema foi desarmado
                } else {                              //Errou, estourou o tempo ou está bloqueado: dispara o alarme
                    disparos++;                       //Incrementa o contador de disparos em 1
                    dispararAlarme();                 //Registra que o alarme foi acionado e começa o som.

                    printf("\nALERTA! POSSÍVEL INVASÃO!\n");      //Texto de alerta caso alarme ativado.
                    printf("ALERTA ACIONADO!\n");                 //Texto de alerta caso alarme ativado.
                    notificarProprietario("Porta aberta sem a senha correta");   //Avisa o dono da casa pelo celular
                }
            }
        }
    }

    printf("\nTotal de disparos do alarme: %d\n", disparos);   //Mostra quantas vezes o alarme foi acionado; %d: número inteiro

    if (alarmeAtivo == 1) {                         //Verifica o estado final do alarme
        printf("\nSTATUS FINAL: ALARME ATIVO!\n");  //Se estiver ativo = 1 -> Aviso de ativação
    } else {
        printf("\nSTATUS FINAL: ALARME INATIVO!\n");//Se estiver inativo = 0 -> Aviso de inativação
    }

    encerrarCamera();                               //Desliga a câmera e libera a memória

    return 0; //Indica o encerramento do programa
}             //Fecha a função main()
