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

struct server_info_struct {
	struct sockaddr_in loc_addr;
	int server_socket_desc;
};

typedef struct {
	uint8_t size;
	char method;
	char payload[5];
} aatp_msg;

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

void initialize_server_info(int port, struct server_info_struct *server_info){
	server_info->loc_addr.sin_family = AF_INET;
	server_info->loc_addr.sin_addr.s_addr = INADDR_ANY;
	server_info->loc_addr.sin_port = htons(port);

	server_info->server_socket_desc = socket(AF_INET , SOCK_STREAM , 0);
};

int initialize_server_socket(struct server_info_struct *server_info){
    if (server_info->server_socket_desc == -1)
    {
        printf("xx Não foi possível criar um socket xx");
    }
    printf("++ Socket criado ++\n");

	if( bind(server_info->server_socket_desc, (struct sockaddr *)&server_info->loc_addr, sizeof(struct sockaddr)) < 0)
    {
        //print the error message
        perror("Bind() falhou. Erro: ");
        return 1;
    }
    puts("++ Bind foi realizado ++");
}

int handle_connection_threads(struct server_info_struct *server_info){
	listen(server_info->server_socket_desc , 3);

	puts("... Esperando conexoes ...");

    int sockaddr_in_size = sizeof(struct sockaddr_in);
	int client_sock_desc, *new_client_sock_desc;
	struct sockaddr_in rem_addr;
	
	while( client_sock_desc = accept(server_info->server_socket_desc, (struct sockaddr *)&rem_addr, (socklen_t*)&sockaddr_in_size)) {
		puts("\n++ CONEXAO FEITA ++");

		pthread_t connection_thread;
        new_client_sock_desc = malloc(sizeof *new_client_sock_desc);
        *new_client_sock_desc = client_sock_desc;

		if( pthread_create( &connection_thread , NULL ,  connection_handler , (void*) new_client_sock_desc) < 0)
        {
            perror("xx Nao foi possivel criar a thread xx");
            return 1;
        }
	}

	if (client_sock_desc < 0)
    {
        perror("Nao aceito");
        return 1;
    }
}

void *connection_handler(void *server_socket_desc){
    int sock = *(int*)server_socket_desc;
    int read_size;
    char *message; 
	aatp_msg recv_buffer;

    while( (read_size = recv(sock , &recv_buffer , sizeof(recv_buffer) , 0)) > 0 ){
		// SOLICITACAO DE TRINCAS -- CLIENTE
		if(recv_buffer.method == 'S'){

			printf("** RECEBIDA SOLICITACAO DE TRINCAS **\n\n");

			printf("<<-- Solicitação recebida\n");
			printf("|=============|\n");
			printf("|| CABEÇALHO || \n");
			printf("|-------------|\n");
			printf("|     %c %i     |\n",recv_buffer.method, recv_buffer.size);
			printf("|=============|\n\n");

			aatp_msg response_packet = {0};
			response_packet.method = 'R';
			response_packet.size = recv_buffer.size;
			memset(&response_packet.payload, 0, sizeof(response_packet));

			srand( (unsigned)time( NULL ));

			char random_aminoacids[20] = {'A','R','N','D','C','E','Q','G',
										  'H','I','L','K','M','F','P','S','T','W',
										  'Y','V'};
			int i = 0;

			printf("A.A. enviados     -->>\n");	
			printf("|====================|\n");
			printf("||     CABEÇALHO    || \n");
			printf("|--------------------|\n");
			printf("|         %c %i        |\n",response_packet.method, response_packet.size);


			printf("|====================|\n");
			printf("||       DADOS      || \n");
			printf("|--------------------|\n");


			for ( i = 0; i < recv_buffer.size; ++i) {
				int random_index = (rand() % 20);
				response_packet.payload[i] = random_aminoacids[random_index];
				printf("|          %c         |\n", response_packet.payload[i]);
			}

			printf("|====================|\n\n");


			//Send the message back to client
			write(sock , &response_packet , sizeof(response_packet));
			//printf("|| IP: %s\n", inet_ntoa( ((struct sockaddr_in *) &socket_desc)->sin_addr));
			
		}

		// RESPOSTA DE UMA SOLICITACAO -- SERVIDOR
		if(recv_buffer.method == 'R'){
			printf("** RECEBIDA RESPOSTA COM TRINCAS **\n");
		}
    }

    if(read_size == 0){
        puts("-- CONEXAO DESFEITA --\n");
        fflush(stdout);
    }
    else if(read_size == -1){
        perror("xx recv falhou xx");
    }

    free(server_socket_desc);
    close(sock);
    pthread_exit(NULL); 
}

void send_aminoacid_solicitation(struct server_info_struct *server_info){
	if (connect(server_info->server_socket_desc, (struct sockaddr *) &server_info->loc_addr, sizeof(server_info->loc_addr)) < 0) {
		perror("Conectando stream socket");
		exit(1);
	}

	char cat[] = { 0x13 };

	send(server_info->server_socket_desc, cat, sizeof(cat), 0);

	/* fechamento do socket remota */ 	
	close(server_info->server_socket_desc);
}

void *solicitation_sender_handler(){
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

	struct server_info_struct server_info;

	initialize_server_info(atoi(argv[1]),&server_info);
	initialize_server_socket(&server_info);

	//pthread_t solicitation_sender_thread;
	/*if( pthread_create( &solicitation_sender_thread , NULL ,  solicitation_sender_handler , (void*) loc_info->new_sock) < 0)
        {
            perror("xx Nao foi possivel criar a thread de envio de solicitacoes xx");
            return 1;
        }
	}*/
	send_aminoacid_solicitation(&server_info);
	handle_connection_threads(&server_info);

	printf("saiu");

    return 0;

	// =	
};


