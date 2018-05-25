// Client 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "tp_socket.c"

void error(const char *msg){
	perror(msg);
	exit(1);
}

void create_packet(char* akc, char* dados, char checksum, char* packet){
	strcpy(packet, akc);
	packet[1] = checksum;
	strcat(packet, dados);
}

void extract_packet(char* packet, char* ack, char* dados, int total_recebido ){
	strncpy(ack, packet, 1);  // Be aware that strncpy does NOT null terminate
	ack[1] = '\0';
	memmove(dados, packet+2, total_recebido-2);
	printf("PACKAGE: %s, DADOS: %s \n", packet, dados);
}

void extract_ack(char* packet, char* ack){
	strncpy(ack, packet, 1);  // Be aware that strncpy does NOT null terminate
	ack[1] = '\0';
}

void toggle_ack(char *ack){
	if (strcmp(ack, "0") == 0){
		strcpy(ack, "1");
	}
	else if (strcmp(ack, "1") == 0){
		strcpy(ack, "0");
	}
	else{
		error("Valor de ACK desconhecido");
	}
}

int bytes_to_write(int total_recebido, int tam_cabecalho){
	int write_n;
	if (total_recebido-tam_cabecalho > 0){
		write_n = total_recebido-tam_cabecalho;
	}
	else{
		write_n = 0;
	}
	return write_n;
}

char checksum(char* s, int total_lido ){
	char sum = 1;
	int i = 0;

	while (i < total_lido)
	{	
		sum += *s;
		s++;
		//printf("Checksummmmmmmmmmmmm: %c %d\n    ",sum, sum);   //, %d", sum, atoi(sum));
		i++;
		//printf("i < total_lido: %d < %d \n", i, total_lido);
	}
	return sum;
}

char extract_checksum(char* packet){
	printf("Packet[1] = %d \n", packet[1]);
	return packet[1];
}

int main(int argc, char **argv){
	// PROCESSANDO ARGUMENTOS DA LINHA DE COMANDO
	if(argc < 5){	
		fprintf(stderr , "Parametros faltando \n");
		printf("Formato: [host_do_servidor], [porta_do_servidor], [nome_do_arquivo], [tamanho_buffer] \n");
		exit(1);
	}

	size_t host_len = strlen(argv[1]) + 1;  
	//converte o numero de porta de string para inteiro
	int porta_do_servidor;
	porta_do_servidor = atoi(argv[2]);
	size_t filename_len = strlen(argv[3]) + 1;
	char *nome_do_servidor = calloc(host_len, sizeof (*nome_do_servidor));
	char *nome_do_arquivo = calloc(filename_len, sizeof (*nome_do_arquivo));
	int tam_buffer = atoi(argv[4]);
	//verifica se o espaço de memória foi alocado com sucesso
	if (!nome_do_servidor) {   
		fprintf (stderr, "error: virtual memory exhausted allocating 'nome_do_servidor'\n");
		return 1;
    	}
	//salva o nome do servidor (na forma de endereço IP) recbido pela linha de comando na variável nome_do_servidor
	strncpy(nome_do_servidor, argv[1], host_len-1);
	if (!nome_do_arquivo) { 
		fprintf (stderr, "error: virtual memory exhausted allocating 'nome_do_arquivo'\n");
		return 1;
    	}
	//salva o nome do arquivo recebido pela linha de comando na variável nome_do_arquivo
    strncpy(nome_do_arquivo, argv[3], filename_len);

	printf("Nome do servidor: %s,\n", nome_do_servidor);
	printf("Porta do servidor: %d\n", porta_do_servidor);
	printf("Nome do arquivo: %s, %zu, %zu\n", nome_do_arquivo, sizeof(nome_do_arquivo), strlen(nome_do_arquivo));
	printf("Tamanho do buffer: %d\n", tam_buffer);

	// Inicializando TP Socket
	tp_init();

	// Cria um socket udp
	int udp_socket;
	unsigned short porta_cliente = 9000;  // numro arbitrario para porta do cliente
	udp_socket = tp_socket(porta_cliente);
	if (udp_socket == -1){
		error("Falha ao criar o socket\n");
	}
	else if (udp_socket == -2){
		error("Falha ao estabelecer endereco (tp_build_addr)\n");
	}
	else if (udp_socket == -3){
		error("Falha de bind\n");
	}

	//Estabelecendo endereco de envio
	so_addr server;
	if (tp_build_addr(&server,nome_do_servidor,porta_do_servidor)< 0){
		error("Falha ao estabelecer endereco do servidor\n");
	}

	// Seta nome do arquivo
	char *nome_do_arquivo_pkg = calloc(filename_len+2, sizeof (*nome_do_arquivo_pkg));
	//strcpy(nome_do_arquivo_pkg, "0");
	//strcat(nome_do_arquivo_pkg, nome_do_arquivo);
	char ack[] = "0";
	char sum = '\0';
	char sum_recebido = '\0';
	sum = checksum(nome_do_arquivo, strlen(nome_do_arquivo));
	
	//printf("Sum : %c %d \n", sum, sum);
	//printf("Nome de nome_do_arquivo_pkg: %s\n, tamanho: %zu, strlen: %zu \n", nome_do_arquivo_pkg, sizeof(nome_do_arquivo_pkg), strlen(nome_do_arquivo_pkg));

	// Realiza temporizacao espara 1s
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	if(setsockopt(udp_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))<0){
		perror("Error setsockopt\n");}

	// Envia nome do arquivo
	int total_recebido;
	int sent;
	char *buffer = calloc(tam_buffer, sizeof (*buffer));
	do {
		printf("Envia nome_do_arquivo ......\n");
		create_packet(ack, nome_do_arquivo, sum, nome_do_arquivo_pkg);
		tp_sendto(udp_socket, nome_do_arquivo_pkg, strlen(nome_do_arquivo_pkg)+1, &server);
		total_recebido = tp_recvfrom(udp_socket, buffer, tam_buffer, &server);  // Esperando ACK = 0
	}while ((total_recebido == -1) || buffer[0] != '0');
	printf("OK, server recebeu o nome do arquivo !\n");

	// Confirma inicio da conexão
	strcpy(ack, "1");
	tp_sendto(udp_socket, ack, sizeof(ack), &server); // Manda ACK = 1

	//abre um arquivo para salvar os dados recebidos
	printf("Nome do arquivo: %s \n", nome_do_arquivo);
	FILE *arq;
	arq = fopen(nome_do_arquivo, "w+");
	if (arq == NULL){
		printf("Problemas na criacao do arquivo");
		exit(1);	
	}
	fflush(arq);

	// Recebe os dados por Stop and Wait
	int tam_cabecalho = 2;
	int tam_dados = tam_buffer-tam_cabecalho;
	char *dados = calloc(tam_dados, sizeof (*dados));
	int total_gravado; //recebe o numero em bytes do que foi gravado no arquivo em cada iteracao
	int tam_arquivo = 0; 
	char ack_esperado[] = "0";
	char ack_recebido[2] = { 0 };
	int max_timeouts = 10;
	int timeouts = 0;
	int write_n;

	do {
		if (strcmp(ack_esperado, "0") == 0){
			do{

				total_recebido = tp_recvfrom(udp_socket, buffer, tam_buffer, &server);  // Esperando ACK = 0
				if (total_recebido > 0){
					extract_ack(buffer, ack_recebido);
					printf("Aguardando ACK=0, ack_recebido=%s \n", ack_recebido);
					sum_recebido = extract_checksum(buffer);
					extract_packet(buffer, ack_recebido, dados, total_recebido);
					//printf("DADOS :%s, sizeof: %zu \n", dados, sizeof(dados));
					sum = checksum(dados, total_recebido-tam_cabecalho);
				
					if (sum != sum_recebido){
						printf("WRONNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNG \n");
						//printf("SUM = %d, SUM_RECEBIDO = %d \n", sum, sum_recebido);
					}
					if ((strcmp(ack_recebido, "0") == 0) && (sum == sum_recebido)){
						tp_sendto(udp_socket, "0", sizeof("0"), &server); // Manda ACK = 0
						//printf("Recebi corretamente : %s \n", buffer);
						timeouts = 0;
					}
				}
				else{
					sent = tp_sendto(udp_socket, "1", sizeof("1"), &server); // Manda ACK = 1
					timeouts++;
				}

			}while(((total_recebido == -1) || (strcmp(ack_recebido, "0") != 0) || (sum != sum_recebido)) && timeouts<=max_timeouts);
		}

		else if(strcmp(ack_esperado, "1") == 0){
			do{
			total_recebido = tp_recvfrom(udp_socket, buffer, tam_buffer, &server);  // Esperando ACK = 1
			if (total_recebido > 0){
					extract_ack(buffer, ack_recebido);
					printf("Aguardando ACK=0, ack_recebido=%s \n", ack_recebido);
					sum_recebido = extract_checksum(buffer);
					extract_packet(buffer, ack_recebido, dados, total_recebido);
					sum = checksum(dados, total_recebido-tam_cabecalho);
				
				if (sum != sum_recebido){
					printf("WRONNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNG \n");
					//printf("SUM = %d, SUM_RECEBIDO = %d \n", sum, sum_recebido);
					//printf("DADOS DO ERRADO: %s \n", dados);
				}
				if ( (strcmp(ack_recebido, "1") == 0) && (sum == sum_recebido) ){
					tp_sendto(udp_socket, "1", sizeof("1"), &server); // Manda ACK = 1
					//printf("Recebi corretamente : %s \n", buffer);
				}
			}
			else{
				sent = tp_sendto(udp_socket, "0", sizeof("0"), &server); // Manda ACK = 0
				timeouts++;
			}
			}while(((total_recebido == -1) || (strcmp(ack_recebido, "1") != 0) || (sum != sum_recebido)) &&  timeouts<=max_timeouts);
		}

		extract_packet(buffer, ack_recebido, dados, total_recebido);
		//printf("DADOS: %s \n", dados);
		fflush(arq);
		write_n = bytes_to_write(total_recebido, tam_cabecalho);
		total_gravado = fwrite(dados, 1, write_n, arq);
		tam_arquivo += total_gravado;
		printf("total_gravado: %d, total_recebido-tam_cabecalho: %d \n", total_gravado, total_recebido-tam_cabecalho);

		if ((total_gravado != total_recebido-tam_cabecalho) && (total_recebido-tam_cabecalho > 0)){
			printf("Erro na escrita do arquivo.\n");
			exit(1);
		}
		memset(dados, 0, tam_dados);
		toggle_ack(ack_esperado);

	}while((total_recebido != tam_cabecalho) && timeouts<=max_timeouts);

	printf("Foram recebidos, %d bytes!! \n", tam_arquivo);
	printf("Numero de timeouts: %d \n\n", timeouts);

	//fecha o arquivo
	fclose(arq);

	// Libera os ponteiros alocados
	free(nome_do_arquivo_pkg);
	free(nome_do_arquivo);
	free(nome_do_servidor);

	return 0;
}
