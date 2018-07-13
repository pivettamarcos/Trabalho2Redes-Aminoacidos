#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
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

#define PROTEINSIZE 609

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

typedef struct{
    char growing_sequence[PROTEINSIZE];
    char complete_sequence[PROTEINSIZE];
    int reader_head;
    int num_solicitations;
} shared_data_struct;
shared_data_struct shared_data;

clock_t code_time_start;
clock_t code_time_end;

pthread_mutex_t shared_data_mutex = PTHREAD_MUTEX_INITIALIZER;

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
	printf("\n===IPs no arquivo de texto:===\n\n");
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
	printf("\n==============================\n");	

    fclose(fp);
    if (line)
        free(line);

	// ===================================================================
}

void *outcoming_connection_handler(void *client_connection_arg){
    int read_size;
    char *message; 
    int max_num_sol = 20000, cont_sol = 0;

	msg_procotol_struct recv_buffer;

    client_connection* client_connectionf = (client_connection*)client_connection_arg;

    struct sockaddr_in rem_addr;
    rem_addr.sin_family = AF_INET; /* familia do protocolo*/
	rem_addr.sin_addr.s_addr = client_connectionf->rem_hostname; /* endereco IP local */
	rem_addr.sin_port = htons(client_connectionf->port); /* porta local  */
    if (connect(client_connectionf->socket_desc, (struct sockaddr *) &rem_addr, sizeof(rem_addr)) < 0) {
		perror("Conectando stream socket");
		exit(1);
	}
        
    int z = 0;

    FILE *fp = NULL;
    fp=fopen("seq.txt", "a");
    
    while(1){
        cont_sol++;
        if(cont_sol > max_num_sol){
            pthread_mutex_unlock(&shared_data_mutex);
            close(client_connectionf->socket_desc);
            pthread_exit(NULL); 
        }

        msg_procotol_struct outcoming_message = {0};
        outcoming_message.method = 'S';
        outcoming_message.size = 5;
        memset(&outcoming_message.payload, 0, sizeof(outcoming_message.payload));

        send(client_connectionf->socket_desc, &outcoming_message, sizeof(outcoming_message), 0);

        printf("-----------------\n");
        printf("|  S >> ||  %i  || \n", outcoming_message.size);
        printf("-----------------\n");

        read_size = recv(client_connectionf->socket_desc , &recv_buffer , sizeof(recv_buffer) , 0);
        
        if(recv_buffer.method == 'R'){
            int i = 0;

            msg_procotol_struct response_packet = {0};
            response_packet.method = 'R';
            response_packet.size = recv_buffer.size;

            for ( i = 0; i < response_packet.size-1; ++i) { printf("---"); };
            printf("-----------------\n");
            printf("|  << R ||  %i  || ",response_packet.size);

            for ( i = 0; i < response_packet.size; ++i) {
                printf("%c ", recv_buffer.payload[i]);
            }
            printf("|\n");
            for ( i = 0; i < response_packet.size-1; ++i) { printf("---"); };
            printf("-----------------\n");

            //Verifica match
            for ( i = 0; i < response_packet.size; ++i) {
                if(shared_data.reader_head >= PROTEINSIZE-1){
                    pthread_mutex_unlock(&shared_data_mutex);
                    close(client_connectionf->socket_desc);
                    pthread_exit(NULL); 
                }

                // XX SEÇÃO CRITICA
                pthread_mutex_lock(&shared_data_mutex);

                shared_data.num_solicitations++;

                if( shared_data.complete_sequence[shared_data.reader_head] == recv_buffer.payload[i]){
                    shared_data.growing_sequence[shared_data.reader_head] = recv_buffer.payload[i];
                    printf("\n|| MATCH da letra %c ||\n||  %s  ||\n||\n\n", recv_buffer.payload[i], shared_data.growing_sequence);

                    //GRAVA NO ARQUIVO   
                    fprintf(fp, "%c", recv_buffer.payload[i]);

                    shared_data.reader_head++;
                    if(shared_data.reader_head >= PROTEINSIZE-1){
                        pthread_mutex_unlock(&shared_data_mutex);
                        close(client_connectionf->socket_desc);
                        pthread_exit(NULL); 
                    }
                }else{
                }

                pthread_mutex_unlock(&shared_data_mutex);
                // XX SEÇÃO CRITICA
            }
        }
    }

    fclose(fp);
}

int handle_outcoming_connections(size_t *num_connections, client_info_struct *client_socket_str, client_connection* client_connections){
    int i = 0;
    pthread_t outcoming_connection_threads[*num_connections];
    for(i = 0; i < *num_connections; i++){
       /* new_client_sock_desc = malloc(sizeof *new_client_sock_desc);
        *new_client_sock_desc = client_sock_desc;*/
        if( pthread_create( &outcoming_connection_threads[i] , NULL ,  outcoming_connection_handler , (void*)&client_connections[i]) < 0){
            {
                perror("xx Nao foi possivel criar a thread xx");
                return 1;
            }
        }
    }

    for (int i = 0; i < *num_connections; i++){
       pthread_join(outcoming_connection_threads[i], NULL);
    }
}

void initialize_shared_data(){
    shared_data.reader_head = 0;
    shared_data.num_solicitations = 0;

    //Preenche a sequência completa
    FILE *fp = NULL;
    fp=fopen("col_seq.txt", "r+");
    int i = 0;
    char sequence_buffer[PROTEINSIZE];

    fgets(sequence_buffer,PROTEINSIZE, fp);
    printf("\nInicializou a sequência completa: %s\n", sequence_buffer);
    strcpy(shared_data.complete_sequence, sequence_buffer);

    fclose(fp);
}

void client_fork_handler(client_info_struct *client_socket_str){
    /* Envia solicitações e escuta resposta */

    size_t num_connections;
    client_connection client_connections[100];

    code_time_start = clock();

    initialize_shared_data();
    define_client_connections(client_socket_str->port, &num_connections, client_connections);
    handle_outcoming_connections(&num_connections, client_socket_str, client_connections);

    code_time_end = clock();

    printf("\nA SEQUỄNCIA ESTÁ COMPLETA\n");
    printf("\n||  %s  ||\n", shared_data.growing_sequence);
    printf("\nNúmero total de solicitações: %i \n",shared_data.num_solicitations);

    double time_spent = (double)(code_time_end - code_time_start) / CLOCKS_PER_SEC;
    printf("\nTempo decorrido: %f \n", time_spent);
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

			char random_aminoacids[20] = {'A','R','N','D','C','E','Q','G',
										  'H','I','L','K','M','F','P','S','T','W',
										  'Y','V'};
			int i = 0;

            for ( i = 0; i < recv_buffer.size-1; ++i) { printf("---"); };
            printf("-----------------\n");
			printf("|  R >> ||  %i  || ",response_packet.size);

			for ( i = 0; i < recv_buffer.size; ++i) {
				int random_index = (rand() % 20);
				response_packet.payload[i] = random_aminoacids[random_index];
				printf("%c ", response_packet.payload[i]);
			}
            printf("|\n");
            for ( i = 0; i < recv_buffer.size-1; ++i) { printf("---"); };
            printf("-----------------\n");

			//Send the message back to client
			write(sock , &response_packet , sizeof(response_packet));
			//printf("|| IP: %s\n", inet_ntoa( ((struct sockaddr_in *) &socket_desc)->sin_addr));
			
		}
    }

    if(read_size == 0){
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

    int sockaddr_in_size = sizeof(struct sockaddr_in);
	int client_sock_desc, *new_client_sock_desc;
	struct sockaddr_in rem_addr;
	
	while( client_sock_desc = accept(server_socket_str->server_socket_desc, (struct sockaddr *)&server_socket_str->loc_addr, (socklen_t*)&sockaddr_in_size)) {
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
    srand( (unsigned)time( NULL ));

	//Verifica se foi colocado argumento da porta
    if (argc != 2) {
		printf("Digite a porta para iniciar a aplicação \n");
		exit(1);
	}

    //INICIALIZA OS SOCKETS
    server_info_struct server_info_str;
    client_info_struct client_info_str;
    createAndBindSockets(atoi(argv[1]), &client_info_str, &server_info_str);

    //INICIALIZA PROCESSOS
    pid_t child_a, child_b, wpid;
    int status = 0;

    child_a = fork();

    if (child_a == 0) {
        printf("\n:: SERVIDOR INICIADO ::\n");
        server_fork_handler(&server_info_str);
    
    } else {
        child_b = fork();

        if (child_b == 0) {
            printf("\n:: DIGITE 'C' PARA INICIAR O CLIENTE ::\n");
            printf("Entrada: ");
            int c = getchar();
            if(c != EOF){
                while(c != 'c'){
                    while((getchar()) != '\n');
                    printf("Entrada: ");
                    c = getchar();
                }
                client_fork_handler(&client_info_str);
            }
        } else {
            while ((wpid = wait(&status)) > 0);
        }
    }
}