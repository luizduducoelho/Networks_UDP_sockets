// Server
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "tp_socket.c"

void error(const char *msg){
	perror(msg);
	exit(1);
}

void create_packet(char* akc, char* dados, char checksum, char* packet, int total_lido){
	strncpy(packet, akc, 1);
	packet[1] = checksum;
	memmove(packet+2, dados, total_lido);
}

void extract_packet(char* packet, char* ack, char checksum, char* dados ){
	strncpy(ack, packet, 1);  // Esteja ciente que strncpy nao finaliza com '/0'
	ack[1] = '\0';
	memmove(dados, packet+2, strlen(packet)-1);

}

void extract_ack(char* packet, char* ack){
	strncpy(ack, packet, 1);  // Esteja ciente que strncpy nao finaliza com '/0'
	ack[1] = '\0';
}

char checksum(char* s, int total_lido ){
	char sum = 1;
	int i = 0;

	while (i < total_lido)
	{	
		sum += *s;
		s++;
		i++;
	}
	return sum;
}

char extract_checksum(char* packet){
	return packet[1];
}

int main(int argc, char * argv[]){
	// PROCESSANDO ARGUMENTOS DA LINHA DE COMANDO
	if(argc < 3){	
		fprintf(stderr , "Parametros faltando \n");
		printf("Formato: [porta_do_servidor], [tamanho_buffer] \n");
		exit(1);
	}
	int portno = atoi(argv[1]);
	int tam_buffer = atoi(argv[2]);

	printf("Porta do servidor: %d\n", portno);
	printf("Tamanho do buffer: %d\n", tam_buffer);

	// INICIALIZANDO TP SOCKET
	tp_init();

	// CRIANDO SOCKET UDP
	int udp_socket;
	udp_socket = tp_socket(portno);
	if (udp_socket == -1){
		error("Falha ao criar o socket \n");
	}
	else if (udp_socket == -2){
		error("Falha ao estabelecer endereco (tp_build_addr) \n");
	}
	else if (udp_socket == -3){
		error("Falha de bind \n");
	}

	// FROM
	so_addr cliente; 
	char nome_do_arquivo[256] = { 0 };
	char pacote_com_nome[256] = { 0 };
	char ack_recebido[2] = { 0 };
	int count;
	char sum = '\0';
	char sum_recebido = '\0';

	// INICIALIZA TEMPORIZACAO
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	if(setsockopt(udp_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))<0){
		perror("Error setsockopt \n");
	}

	// REBEBE UM BUFFER COM NOME DO ARQUIVO 
	do {
		count = tp_recvfrom(udp_socket, pacote_com_nome, sizeof(nome_do_arquivo), &cliente);
		if (count > 0){
			//extrai a substring com o nome do arquivo, sem o ACK
			sum_recebido = extract_checksum(pacote_com_nome);
			extract_packet(pacote_com_nome, ack_recebido, sum_recebido, nome_do_arquivo);
			printf("Nome recebido: %s \n", nome_do_arquivo);
			sum = checksum(nome_do_arquivo, strlen(nome_do_arquivo));
		}

	}while((count == -1) || (sum != sum_recebido));

	// AGUARDANDO ACK DO NOME DO ARQUIVO
	int tam_cabecalho = 2;
	char *buffer = calloc(tam_buffer, sizeof (*buffer));
	char ack[] = "0";
	do {
		tp_sendto(udp_socket, ack, sizeof(ack), &cliente); // Manda ACK = 0
		count = tp_recvfrom(udp_socket, buffer, sizeof(buffer), &cliente);  // Esperando 1
		extract_ack(buffer, ack_recebido);

	}while ((count == -1) || (strcmp(ack_recebido, "1") != 0) ); 
	count = -1;
	// Exibe mensagem
	//printf("Cliente confirmou inicio da conexao! Comecaremos a enviar dados\n");

	// ABRE O ARQUIVO
	FILE *arq;
	arq = fopen(nome_do_arquivo, "r");
	if(arq == NULL){
		printf("Erro na abertura do arquivo.\n");
		exit(1);
	}
	int total_lido;
	int tam_dados = tam_buffer-tam_cabecalho;
	char *dados = calloc(tam_dados, sizeof (*dados));

	//ROTINA STOP-AND-WAIT - LOOP PRINCIPAL
	do{
		total_lido = fread(dados, 1, tam_dados, arq);
		sum = checksum(dados, total_lido);

		if(ack[0]=='0'){
			do {
				create_packet(ack, dados, sum, buffer, total_lido);
				tp_sendto(udp_socket, buffer, total_lido + tam_cabecalho, &cliente); // Manda pacote de dados 0
				count = tp_recvfrom(udp_socket, ack_recebido, sizeof(ack_recebido), &cliente);  // Espera ACK = 0
				// Do something better here 
			}while ((count == -1) || (strcmp(ack_recebido, "0") != 0));
			memset(ack, 0, 1);
			strcpy(ack,"1");
			count = -1;
			}
		else if(ack[0]=='1'){
			do {
				create_packet(ack, dados, sum, buffer, total_lido);
				//printf("Enviando um buffer: %s, sizeof: %zu, strlen: %zu \n", buffer, sizeof(buffer), strlen(buffer));
				tp_sendto(udp_socket, buffer, total_lido + tam_cabecalho, &cliente); // Manda pacote de dados 1
				count = tp_recvfrom(udp_socket, ack_recebido, sizeof(ack_recebido), &cliente);  // Espera ACK = 1
			}while ((count == -1) || (strcmp(ack_recebido, "1") != 0));
			memset(ack, 0, 1);
			strcpy(ack,"0");
			count = -1;
			}
		memset(buffer, 0, tam_buffer);
		memset(dados, 0, tam_dados);
		memset(ack_recebido, 0, 1);
	}while(total_lido != 0);

	printf("Arquivo enviado totalmente \n\n");

	//FECHA O ARQUIVO
	fclose(arq);

	// LIBERA OS PONTEIROS ALOCADOS
	free(buffer);
	free(dados);

	return 0;
}
