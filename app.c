#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

//Constantes
#define FILEMAXCHAR 1000


struct ip_list_struct {
    size_t num_ips; 
    char **ips; 
};

struct loc_info_struct {
	int socket_desc, 
		client_sock,
		c, 
		*new_sock,
		port;
};

void initialize_ip_list_struct(struct ip_list_struct *ip_list){
	ip_list->num_ips = 0;
	ip_list->ips = NULL;
}

void build_ip_array(struct ip_list_struct *ip_list){

	//COLOCA IPS DO ARQUIVO DE TEXTO EM UM ARRAY ==========================
	FILE *fp;
    char str[FILEMAXCHAR];
    char* filename = "ipList.txt";

	ssize_t read;
	char * line = NULL;
	size_t len = 0;

	fp = fopen(filename, "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);

	// ===========

	size_t currentIndex = 0;
	printf("===IPs no arquivo de texto:===\n\n");
    while ((read = getline(&line, &len, fp)) != -1) {
        printf("-IP fixo: ");
		ip_list->ips = realloc(ip_list->ips, sizeof(*ip_list->ips) * ip_list->num_ips+1);
		ip_list->ips[ip_list->num_ips] = line;
		printf("%s \n", ip_list->ips[ip_list->num_ips]);
		ip_list->num_ips++;
    }
	printf("==============================\n");	

    fclose(fp);
    if (line)
        free(line);

	// ===================================================================
}

void *connection_handler(void *socket_desc){
    //Get the socket descriptor
    int sock = *(int*)socket_desc;
    int read_size;
    char *message , buffer[6];

    while( (read_size = recv(sock , buffer , sizeof(buffer) , 0)) > 0 )
    {
		// SOLICITACAO DE TRINCAS -- CLIENTE
		if(buffer[0] == 0x10
			|| buffer[0] == 0x11
			|| buffer[0] == 0x12
			|| buffer[0] == 0x13
			|| buffer[0] == 0x14){

			printf("** RECEBIDA SOLICITACAO DE TRINCAS **\n\n");

			printf("<<--\n",buffer[0]);
			printf("|=============|\n",buffer[0]);
			printf("|| Cabecalho || \n");
			printf("|=============|\n",buffer[0]);
			printf("|     0x%hhx    |\n",buffer[0]);
			printf("|=============|\n\n",buffer[0]);

			int codon_number = 0;
			if(buffer[0] == 0x10)
				codon_number = 1;
			else if(buffer[0] == 0x11)
				codon_number = 2;
			else if(buffer[0] == 0x12)
				codon_number = 3;
			else if(buffer[0] == 0x13)
				codon_number = 4;
			else if(buffer[0] == 0x14)
				codon_number = 5;

			srand( (unsigned)time( NULL ) );
			//int random_number_codons = (rand() % 5)+1;
			char codons[codon_number];
			int i = 0;

			printf("                 -->>\n",buffer[0]);	
			printf("|====================|\n",buffer[0]);
			printf("|| Trincas enviadas || \n");
			printf("|====================|\n",buffer[0]);


			for ( i = 0; i < sizeof(codons); ++i) {
				codons[i] = (char)(rand() % 65);
				printf("|        0x%x        |\n", codons[i] & 0xff);
			}

			printf("|====================|\n\n",buffer[0]);


			//Send the message back to client
			write(sock , codons , strlen(buffer));
			//printf("|| IP: %s\n", inet_ntoa( ((struct sockaddr_in *) &socket_desc)->sin_addr));

		}

		// RESPOSTA DE UMA SOLICITACAO -- SERVIDOR
		if(buffer[0] == 0x00
			|| buffer[0] == 0x01
			|| buffer[0] == 0x02
			|| buffer[0] == 0x03
			|| buffer[0] == 0x04){

			printf("** RECEBIDA RESPOSTA COM TRINCAS **\n");
		}
    }

    if(read_size == 0)
    {
        puts("-- CONEXAO DESFEITA --\n");
        fflush(stdout);
    }
    else if(read_size == -1)
    {
        perror("xx recv falhou xx");
    }

    free(socket_desc);
    close(sock);
    pthread_exit(NULL); 
}

int handle_connection_threads(struct loc_info_struct *loc_info, struct sockaddr_in *server, struct sockaddr_in *client){
	loc_info->socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (loc_info->socket_desc == -1)
    {
        printf("xx Nao foi possivel criar um socket xx");
    }
    printf("++ Socket criado ++\n");

	//printf("%s", atoi(loc_info->port));

	server->sin_family = AF_INET;
    server->sin_addr.s_addr = INADDR_ANY;
    server->sin_port = htons( loc_info->port );

	if( bind(loc_info->socket_desc,(struct sockaddr *)server , sizeof(*server)) < 0)
    {
        //print the error message
        perror("zz Bind() falhou. Erro: ");
        return 1;
    }
    puts("++ Bind foi realizado ++");

	listen(loc_info->socket_desc , 3);

	puts("... Esperando conexoes ...");
    loc_info->c = sizeof(struct sockaddr_in);
	
	
	while( loc_info->client_sock = accept(loc_info->socket_desc, (struct sockaddr *)&client, (socklen_t*)&loc_info->c)) {
		puts("++ CONEXAO FEITA ++");

		pthread_t connection_thread;
        loc_info->new_sock = malloc(sizeof *loc_info->new_sock);
        *loc_info->new_sock = loc_info->client_sock;

		if( pthread_create( &connection_thread , NULL ,  connection_handler , (void*) loc_info->new_sock) < 0)
        {
            perror("xx Nao foi possivel criar a thread xx");
            return 1;
        }
	}

	if (loc_info->client_sock < 0)
    {
        perror("Nao aceito");
        return 1;
    }
}

int main(int argc, char *argv[]) {
	//Verifica se foi colocado argumento da porta
    if (argc != 2) {
		printf("Digite a porta para iniciar a aplicacao \n");
		exit(1);
	}

	printf("*** Trabalho 2 de redes - Aminoacidos ***\n\n");

	// == Inicializa ips
	
	struct ip_list_struct ip_list;
	initialize_ip_list_struct(&ip_list);
	build_ip_array(&ip_list);
	
	// == Controla as threads do cliente

	struct loc_info_struct loc_info = {socket(AF_INET , SOCK_STREAM , 0),
										0,
										sizeof(struct sockaddr_in),
										0,
										atoi(argv[1])};
	
	struct sockaddr_in server, client;

	handle_connection_threads(&loc_info, &server, &client);

    return 0;

	// =	
};


