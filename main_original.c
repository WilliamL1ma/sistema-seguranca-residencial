/******************************************************************************

                        SISTEMA DE SEGURANÇA RESIDENCIAL

*******************************************************************************/

#include <stdio.h>   //Biblioteca adicionada (printf, scanf, getchar)
#include <stdlib.h>  //Biblioteca adicionada (exit: encerra o programa)
#include <time.h>    //Biblioteca adicionada (time e difftime: contagem regressiva, tempo de entrada e bloqueio)

#define SENHA_CORRETA 1234   //Define uma constante: para trocar a senha, basta alterar este valor
#define TEMPO_ENTRADA 10     //Define uma constante: segundos que o morador tem para digitar a senha depois de abrir a porta
#define TEMPO_BLOQUEIO 30   //Define uma constante: segundos de bloqueio depois de 3 senhas erradas

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

//Função da contagem regressiva de saída (usada ao armar o sistema no modo Ausente).
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

int main()                    //Início do programa;
{
    int modo;                 //Criação de variável (1 = Em casa: só a porta dispara / 2 = Ausente: porta e movimento disparam)
    int movimento;            //Criação de variável (Se o sensor detectou movimento);
    int porta;                //Criação de variável (Se a porta está aberta)
    int pessoaNoLocal;        //Criação de variável (1 = pessoa ainda no local / 0 = pessoa já saiu)
    int tentativas = 0;       //Criação de variável (Quantas vezes a senha foi digitada e estava incorreta - inicia 0)
    time_t bloqueadoAte = 0;  //Criação de variável (Horário em que o bloqueio termina - inicia 0 = sem bloqueio)
    int armado = 0;           //Criação de variável (0 = sistema desarmado / 1 = sistema armado e monitorando)
    int alarmeAtivo = 0;      //Criação de variável (0 = desligado / 1 = ligado)
    int disparos = 0;         //Criação de variável (Quantas vezes o alarme foi acionado - inicia 0)
    int desejaSenha;          //Criação de variável (Armazena a escolha do usuário: digitar a senha ou não)
    int rearmar;              //Criação de variável (Armazena a escolha do usuário: armar o sistema novamente ou não)
    int continuar = 1;        //Controla a continuidade do monitoramento.

    printf("====================================\n"
           "        SISTEMA DE SEGURANÇA\n"                       //Apresenta texto "SISTEMA DE SEGURANÇA"
           "====================================\n");

    modo = lerInteiro("\nModo (1-Em casa / 2-Ausente): ", 1, 2);   //Lê o modo de funcionamento; só aceita 1 ou 2

    if (modo == 2) {                                  //Tempo de saída: acontece apenas no modo Ausente
        contagemSaida();                              //Executa a contagem regressiva para o morador sair
    }
    armado = 1;                                       //Registra que o sistema está armado
    printf("SISTEMA ARMADO!\n");                      //Fim da contagem: sistema pronto para monitorar

    while (continuar == 1) {                          // Repete o monitoramento enquanto continuar for igual a 1.

        if (armado == 0) {                            //Sistema desarmado (morador entrou e digitou a senha):
            printf("\nSISTEMA DESARMADO: sensores desligados.\n");   //Informa que os sensores não estão sendo monitorados
            rearmar = lerInteiro("\nArmar o sistema? (1-Sim/0-Nao): ", 0, 1);   //Pergunta se quer armar o sistema novamente

            if (rearmar == 1) {                       //Usuário escolheu armar (1 = Sim):
                if (modo == 2) {                      //No modo Ausente:
                    contagemSaida();                  //Executa a contagem regressiva para o morador sair
                }
                armado = 1;                           //Registra que o sistema está armado
                printf("SISTEMA ARMADO!\n");          //Informa que o sistema voltou a monitorar
            }

        } else {                                      //Sistema armado: executa o monitoramento normal

            movimento = lerInteiro("\nMovimento detectado? (1-Sim/0-Nao): ", 0, 1);   //Lê o sensor; só aceita 0 ou 1 (letras são recusadas)
            porta     = lerInteiro("\nPorta aberta? (1-Sim/0-Nao): ", 0, 1);          //Lê a porta; só aceita 0 ou 1 (letras são recusadas)

            if (modo == 1 && movimento == 1) {        //Condição "SE" -> && "E": modo Em casa e sensor detectou movimento:
                movimento = 0;                        //Zera o movimento: em casa, o movimento é normal e não deve disparar o alarme
                printf("Modo Em casa: movimento ignorado.\n");          //Informa que o movimento foi ignorado
            }

            if (movimento == 1) {                     //Se ainda há movimento (modo Ausente), confirma se alguém está mesmo no local
                pessoaNoLocal = lerInteiro("\nA pessoa ainda esta no local? (1-Sim/0-Nao): ", 0, 1);   //Pergunta se a pessoa ainda está no local
                if (pessoaNoLocal == 0) {             //Pessoa já saiu: o sensor ainda marca movimento, mas não é uma invasão
                    movimento = 0;                    //Zera o movimento para que ele não acione o alarme
                    printf("Movimento ignorado: ninguem no local.\n");  //Informa que o movimento foi ignorado
                }
            }

            if (movimento == 1 || (porta == 1 && alarmeAtivo == 1)) {   //Movimento, ou porta aberta com o alarme já tocando = alerta acionado.
                if (alarmeAtivo == 0) {               //Só conta um novo disparo se o alarme estava desligado antes
                    disparos++;                       //Incrementa o contador de disparos em 1
                }
                alarmeAtivo = 1;                      //Registra que o alarme foi acionado.

                printf("\nALERTA! POSSÍVEL INVASÃO!\n");          //Texto de alerta caso alarme ativado.
                printf("ALERTA ACIONADO!\n");                     //Texto de alerta caso alarme ativado.

            } else if (porta == 1) {                  //Porta aberta com o alarme desligado: começa o tempo de entrada
                printf("\nPORTA ABERTA! Digite a senha em ate %d segundos.\n", TEMPO_ENTRADA);   //Avisa o tempo que o morador tem

                if (pedirSenha(&tentativas, &bloqueadoAte, TEMPO_ENTRADA) == 1) {   //Senha correta dentro do tempo:
                    armado = 0;                       //Desarma o sistema: o morador chegou
                    printf("\nSISTEMA DESARMADO!\n"); //Informa que o sistema foi desarmado
                } else {                              //Errou, estourou o tempo ou está bloqueado: dispara o alarme
                    disparos++;                       //Incrementa o contador de disparos em 1
                    alarmeAtivo = 1;                  //Registra que o alarme foi acionado.

                    printf("\nALERTA! POSSÍVEL INVASÃO!\n");      //Texto de alerta caso alarme ativado.
                    printf("ALERTA ACIONADO!\n");                 //Texto de alerta caso alarme ativado.
                }

            } else if (alarmeAtivo == 1) {            //Caso alarme já ativado anteriormente:
                printf("\nALARME CONTINUA ATIVO!\n"); //mantém o alarme ativo e indica mensagem de aviso.

            } else {
                printf("\nAlerta Inativo. Nenhuma invasão indentificada.\n");  //Aviso de inatividade caso condições não acionadas.
            }

            if (armado == 1 && alarmeAtivo == 1) {    //Condição "SE" -> && "E": sistema armado e alarme tocando: permite tentar a senha

                if (time(NULL) < bloqueadoAte) {      //Se o sistema ainda está bloqueado:
                    printf("\nSistema bloqueado! Aguarde %d segundos para tentar a senha.\n", (int)(bloqueadoAte - time(NULL)));   //Informa quanto falta

                } else {                              //Sistema liberado:
                    desejaSenha = lerInteiro("\nDigitar senha para desativar o alarme? (1-Sim/0-Nao): ", 0, 1);   //Pergunta se quer digitar a senha agora

                    if (desejaSenha == 1 && pedirSenha(&tentativas, &bloqueadoAte, 0) == 1) {   //Quis digitar e a senha está correta (sem tempo limite):
                        alarmeAtivo = 0;              //Altera o estado do alarme para 0 (desligado).
                        armado = 0;                   //Desarma o sistema junto com o alarme
                        printf("\nALARME DESATIVADO COM SUCESSO!\n");   //Informa que o alarme foi desativado.
                    }
                }
            }
        }

        continuar = lerInteiro("\nContinuar monitoramento? (1-Sim/0-Nao): ", 0, 1);   //Pergunta se o monitoramento continua; só aceita 0 ou 1
    }

    printf("\nTotal de disparos do alarme: %d\n", disparos);   //Mostra quantas vezes o alarme foi acionado; %d: número inteiro

    if (alarmeAtivo == 1) {                         //Verifica o estado final do alarme
        printf("\nSTATUS FINAL: ALARME ATIVO!\n");  //Se estiver ativo = 1 -> Aviso de ativação
    } else {
        printf("\nSTATUS FINAL: ALARME INATIVO!\n");//Se estiver inativo = 0 -> Aviso de inativação
    }

    return 0; //Indica o encerramento do programa
}             //Fecha a função main()