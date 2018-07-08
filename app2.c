
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h> 
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

typedef struct {
	struct sockaddr_in loc_addr;
	int server_socket_desc;
} server_info_struct;

typedef struct {
	struct sockaddr_in rem_addr;
    int port;
} client_info_struct;

typedef struct {
	uint8_t size;
	char method;
	char payload[5];
} msg_procotol_struct;

typedef struct {
	int socket_desc;
    in_addr_t rem_hostname;
    int port;
} client_connection;

// CLIENTE ==============
void define_client_connections(int port, size_t *num_connections, client_connection *client_connections){
	//COLOCA IPS DO ARQUIVO DE TEXTO EM UM ARRAY ==========================
	FILE *fp;
    char str[2000];
    char* filename = "ipList.txt";

	ssize_t read;
	char * line = NULL;
	size_t len = 0;

    size_t num_ips = 0;

	fp = fopen(filename, "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);

	// ===========

	size_t currentIndex = 0;
	printf("===IPs no arquivo de texto:===\n\n");
    while ((read = getline(&line, &len, fp)) != -1) {
        printf("-IP fixo: ");
        client_connections[num_ips] = (client_connection){socket(AF_INET, SOCK_STREAM, 0), inet_addr(line)};
        	
        if (client_connections[num_ips].socket_desc < 0) {
            perror("Criando stream socket");
            exit(1);
        }

		client_connections[num_ips].rem_hostname = inet_addr(line);
        client_connections[num_ips].port = port;

		printf("%x \n", client_connections[num_ips].rem_hostname);
		printf(": FD %i \n", client_connections[num_ips].socket_desc);

        num_ips++;

    }
    *num_connections = num_ips;
	printf("==============================\n");	

    fclose(fp);
    if (line)
        free(line);

	// ===================================================================
}

void *outcoming_connection_handler(void *client_connection_arg){
    client_connection* client_connectionf = (client_connection*)client_connection_arg;

    printf("conectandoss rem %i",client_connectionf->socket_desc);
    struct sockaddr_in rem_addr;
    rem_addr.sin_family = AF_INET; /* familia do protocolo*/
	rem_addr.sin_addr.s_addr = client_connectionf->rem_hostname; /* endereco IP local */
    printf("port %d\n", client_connectionf->port);
	rem_addr.sin_port = htons(client_connectionf->port); /* porta local  */
    printf("enviando");
    if (connect(client_connectionf->socket_desc, (struct sockaddr *) &rem_addr, sizeof(rem_addr)) < 0) {
		perror("Conectando stream socket");
		exit(1);
	}
        

	msg_procotol_struct outcoming_message = {0};
	outcoming_message.method = 'S';
	outcoming_message.size = 2;
	memset(&outcoming_message.payload, 0, sizeof(outcoming_message.payload));

	char cat[] = { "R" };
	send(client_connectionf->socket_desc, &outcoming_message, sizeof(outcoming_message), 0);
    printf("enviou");
}

int handle_outcoming_connections(size_t *num_connections, client_info_struct *client_socket_str, client_connection* client_connections){
    int i = 0;
    for(i = 0; i < *num_connections; i++){
        pthread_t outcoming_connection_thread;
       /* new_client_sock_desc = malloc(sizeof *new_client_sock_desc);
        *new_client_sock_desc = client_sock_desc;*/
        if( pthread_create( &outcoming_connection_thread , NULL ,  outcoming_connection_handler , (void*)&client_connections[i]) < 0){
            {
                perror("xx Nao foi possivel criar a thread xx");
                return 1;
            }
        }
    }
    while(1){};

    
    //if( pthread_create( &incoming_connection_thread , NULL ,  incoming_connection_handler , (void*) new_client_sock_desc /*(void *) client_sock_desc)*/) < 0){
     /*   {
            perror("xx Nao foi possivel criar a thread xx");
            return 1;
        }
    }
    if (connect(client_socket_str->client_socket_desc, (struct sockaddr *) &client_socket_str->rem_addr, sizeof(client_socket_str->rem_addr)) < 0) {
		perror("Conectando stream socket");
		exit(1);
	}

	aatp_msg m = {0};
	m.method = 'S';
	m.size = 2;
	memset(&m.payload, 0, sizeof(m.payload));

	char cat[] = { "R" };

	send(rem_sockfd, &m, sizeof(m), 0);*/
}

void client_fork_handler(client_info_struct *client_socket_str){
    /* Envia solicitações e escuta resposta */
    printf("\n\nClient fd\n\n");

    size_t num_connections;
    client_connection client_connections[100];

    define_client_connections(client_socket_str->port, &num_connections, client_connections);
    printf("sd %i",client_connections[0].socket_desc);
    handle_outcoming_connections(&num_connections, client_socket_str, client_connections);
}
// ======================


// SERVIDOR ==============
void *incoming_connection_handler(void *client_sock_desc){
    int sock = *(int*)client_sock_desc;
    int read_size;
    char *message; 
	msg_procotol_struct recv_buffer;

    while( (read_size = recv(sock , &recv_buffer , sizeof(recv_buffer) , 0)) > 0 ){
		// SOLICITACAO DE TRINCAS -- CLIENTE
		if(recv_buffer.method == 'S'){

			printf("-----------------\n");
			printf("|  << S ||  %i  || \n", recv_buffer.size);
			printf("-----------------\n");

			msg_procotol_struct response_packet = {0};
			response_packet.method = 'R';
			response_packet.size = recv_buffer.size;
			memset(&response_packet.payload, 0, sizeof(response_packet));

			srand( (unsigned)time( NULL ));

			char random_aminoacids[20] = {'A','R','N','D','C','E','Q','G',
										  'H','I','L','K','M','F','P','S','T','W',
										  'Y','V'};
			int i = 0;

            for ( i = 0; i < recv_buffer.size; ++i) { printf("---"); };
            printf("-----------------\n");
			printf("|  R >> ||  %i  || ",response_packet.size);

			for ( i = 0; i < recv_buffer.size; ++i) {
				int random_index = (rand() % 20);
				response_packet.payload[i] = random_aminoacids[random_index];
				printf("%c ", response_packet.payload[i]);
			}
            printf("|\n");
            for ( i = 0; i < recv_buffer.size; ++i) { printf("---"); };
            printf("-----------------\n");

			//Send the message back to client
			write(sock , &response_packet , sizeof(response_packet));
			//printf("|| IP: %s\n", inet_ntoa( ((struct sockaddr_in *) &socket_desc)->sin_addr));
			
		}
    }

    if(read_size == 0){
        puts("--\n");
        fflush(stdout);
    }
    else if(read_size == -1){
        perror("xx recv falhou xx");
    }

    free(client_sock_desc);
    close(sock);
    pthread_exit(NULL); 
}

int handle_incoming_connections(server_info_struct *server_socket_str){
	listen(server_socket_str->server_socket_desc , 3);

	puts("... Esperando conexões ...");

    int sockaddr_in_size = sizeof(struct sockaddr_in);
	int client_sock_desc, *new_client_sock_desc;
	struct sockaddr_in rem_addr;
	
	while( client_sock_desc = accept(server_socket_str->server_socket_desc, (struct sockaddr *)&server_socket_str->loc_addr, (socklen_t*)&sockaddr_in_size)) {
		puts("\n++");

		pthread_t incoming_connection_thread;
        new_client_sock_desc = malloc(sizeof *new_client_sock_desc);
        *new_client_sock_desc = client_sock_desc;

		if( pthread_create( &incoming_connection_thread , NULL ,  incoming_connection_handler , (void*) new_client_sock_desc /*(void *) client_sock_desc)*/) < 0)
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

void server_fork_handler(server_info_struct *server_socket_str){
    /* Escuta por solicitações e envia resposta*/
    printf("\n\nServer fd %i",server_socket_str->server_socket_desc);

    handle_incoming_connections(server_socket_str);
}
// ======================


int createAndBindSockets(int port, client_info_struct *client_info_str, server_info_struct *server_info_str){
    server_info_str->server_socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    server_info_str->loc_addr.sin_family = AF_INET;
	server_info_str->loc_addr.sin_addr.s_addr = INADDR_ANY;
	server_info_str->loc_addr.sin_port = htons(port);
    //
    client_info_str->port = port;
    //

    if (server_info_str->server_socket_desc == -1)
        printf("xx Não foi possível criar o socket do servidor xx");
    
    printf("++ Socket do servidor criado ++\n");
    
    printf("++ Socket do cliente criado ++\n");

    //Realiza bind dos sockets
	if( bind(server_info_str->server_socket_desc, (struct sockaddr *)&server_info_str->loc_addr, sizeof(struct sockaddr)) < 0)
    {
        //print the error message
        perror("Bind() do servidor falhou. Erro: ");
        return 1;
    }

    /*if( bind(client_socket_fd, (struct sockaddr *)&server_info->loc_addr, sizeof(struct sockaddr)) < 0)
    {
        //print the error message
        perror("Bind() do cliente falhou. Erro: ");
        return 1;
    }*/
    puts("++ Bind dos sockets foi realizado ++");
}

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
	//Verifica se foi colocado argumento da porta
    if (argc != 2) {
		printf("Digite a porta para iniciar a aplicação \n");
		exit(1);
	}

    //INICIALIZA OS SOCKETS
    //struct server_info_struct server_info;
    server_info_struct server_info_str;
    client_info_struct client_info_str;
    createAndBindSockets(atoi(argv[1]), &client_info_str, &server_info_str);

    //INICIALIZA PROCESSOS
    pid_t child_a, child_b;
    char fd[2];  // Pipe para envio do aminoácido

    child_a = fork();

    if (child_a == 0) {
        client_fork_handler(&client_info_str);
    } else {
        child_b = fork();

        if (child_b == 0) {
            //server_fork_handler(&server_info_str);
        } else {
            printf("app");
            while(1){}
        }
    }
}