// Trivial Torrent

// TODO: some includes here  

#include "file_io.h"
#include "logger.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <errno.h>


static const uint32_t MAGIC_NUMBER = 0xde1c3230; 

static const uint8_t MSG_REQUEST = 0;
static const uint8_t MSG_RESPONSE_OK = 1;
static const uint8_t MSG_RESPONSE_NA = 2;

enum { RAW_MESSAGE_SIZE = 13 };

//FUNCIONES !!!!!
void init_buffer(uint8_t buffer[RAW_MESSAGE_SIZE])
{
    //pasar MAGIN_NUMBER to buffer
    uint32_t aux = MAGIC_NUMBER;
    buffer[0] = (uint8_t) aux;
    for (int i = 1; i < 4; i++)
    {
        aux = (aux >> 8);
        buffer[i] = (uint8_t) aux;
    }
    
    //pasar MSG_REQUEST to buffer
    buffer[4] = MSG_REQUEST;
}

void insert_num_block(uint8_t buffer[RAW_MESSAGE_SIZE], uint64_t block_number) //pasar BLOCK_NUMBER to buffer
{
    uint64_t aux = block_number;
    //[5,12]
    buffer[5] = (uint8_t) aux;
    for (int i = 6; i < RAW_MESSAGE_SIZE; i++)
    {
        aux = (aux >> 8);
        buffer[i] = (uint8_t) aux;
    }
}

uint32_t recuperar_Magic_Number(const uint8_t buffer[RAW_MESSAGE_SIZE])
{
    uint32_t aux = (uint32_t) buffer[3];
    for (int i = 2; i >= 0; i--)
    {
        aux = (aux << 8);
        aux |= (uint32_t) buffer[i];
        
    }
    return aux;
}

uint64_t recuperar_Block_Number(const uint8_t buffer[RAW_MESSAGE_SIZE])
{
    //[5,12]
    uint64_t aux = buffer[RAW_MESSAGE_SIZE - 1];
    for (int i = (RAW_MESSAGE_SIZE - 2); i >= 5; i--)
    {
        aux = (aux << 8);
        aux |= (uint64_t) buffer[i];
    }
    return aux;
}

void copiarBuffer(const uint8_t bufferRecv[RAW_MESSAGE_SIZE], uint8_t bufferSend[RAW_MESSAGE_SIZE])
{
	for(int i = 0; i< RAW_MESSAGE_SIZE; i++)
	{
		bufferSend[i] = bufferRecv[i];
	}
}

void copiarBufferTotal(const uint8_t bufferRecv[RAW_MESSAGE_SIZE], uint8_t bufferSend[RAW_MESSAGE_SIZE + MAX_BLOCK_SIZE], struct block_t blt)
{
	for(int i = 0; i < RAW_MESSAGE_SIZE; i++)
	{
		bufferSend[i] = bufferRecv[i];
	}
	for(int i = RAW_MESSAGE_SIZE; i< MAX_BLOCK_SIZE; i++)
	{
		bufferSend[i] = blt.data[i];
	}
} 

//Funciones para hacer pruebas :)
void printf_buffer(const uint8_t buffer[RAW_MESSAGE_SIZE])
{
    for (int i = 0; i < RAW_MESSAGE_SIZE; i++)
        printf("%d: %d \n", i, buffer[i]);
}
void printf_block(struct block_t blt)
{
    int x = 0;
    //for (int i = 0; i < blt.size; i++)
    for (int i = 0; i < 100; i++)
    {
        printf("%d: %d      ", i, blt.data[i]);
        if(x == 9)
        {
            printf("\n");
            x=0;
        }
        else
            x++;
    }
    printf("\n");

}


int main(int argc, char** argv) {

	set_log_level(LOG_DEBUG);
	log_printf(LOG_INFO, "Trivial Torrent (build %s %s) by %s", __DATE__, __TIME__, "J. MATA and G. PINEDA");

	/*****************************************************/
        				 // CLIENT                                
	/*****************************************************/
	if (argc < 3) {
		printf("Client mode \n");

		//REMOVE EXTENSION
		char downloaded_file[50];
		int flag = 0;

		for (int i = 0, j = 0; flag == 0; i++)
		{
			if (argv[1][i] == '.')
			{
				flag = 1;
				downloaded_file[j] = '\0';
			}
			else
			{
				downloaded_file[j] = argv[1][i];
				j++;
			}
		}

		//CREATE TORRENT
		struct torrent_t torrent;

		if (create_torrent_from_metainfo_file(argv[1], &torrent, downloaded_file) == -1)
		{
			//error handling...
			perror("Error in create_torrent_from_metainfo_file()");
			exit(0);
		}


		//WHILE

		int sockfd; //SOCKET

		struct sockaddr_in servaddr;
		struct block_t block;

		servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); //IP_addr
		servaddr.sin_family = AF_INET;

		uint8_t buffer_send[RAW_MESSAGE_SIZE];
		uint8_t buffer_recv[RAW_MESSAGE_SIZE];
		uint32_t magic_number_recv;
		uint64_t block_number_recv;
		uint64_t p = 0;
		uint64_t blocks_complets = 0;

		init_buffer(buffer_send); //inicializar buffer con MAGIC_NUMBER y MSG_REQUEST

		for (uint64_t i = 0; i < torrent.block_count; i++)
			if (torrent.block_map[i])
				blocks_complets++;

		while (p < torrent.peer_count && blocks_complets < torrent.block_count)   //RECORREMOS LOS PUERTOS; p => PEER_NUMBER
		{
			//INICIALIZAR SOCKET
			servaddr.sin_port = torrent.peers[p].peer_port;

			//ABRIR SOCKET
			sockfd = socket(AF_INET, SOCK_STREAM, 0);
			if (sockfd == -1)
			{
				log_printf(LOG_INFO, "Fallo en la creación del socket");
				exit(0);
			}

			//CONECTARSE AL SERVER
			if (connect(sockfd, (const struct sockaddr*) & servaddr, sizeof(servaddr)))
			{
				log_printf(LOG_INFO, "\nError al conectarse al server, peer #%d ", p);
				//perror("ERROR");
				//exit(0); //EXIT_FAILURE
			}
			else
			{
				log_printf(LOG_INFO, "\nConectado con éxito al PEER #%d: ", p);

				for (uint64_t b = 0; b < torrent.block_count; b++) //RECORREMOS LOS BLOQUES; b => BLOCK_NUMBER
				{
					if (!torrent.block_map[b]) //SI NO TENEMOS EL BLOQUE EN MEMORIA:
					{
						log_printf(LOG_INFO, "\n      Pedimos el bloque #%d: ", b);
						insert_num_block(buffer_send, b); //insertar en buffer el numero de bloque

						send(sockfd, buffer_send, RAW_MESSAGE_SIZE, MSG_EOR);
						log_printf(LOG_INFO, "      Petición {magic_number = %X, block_number = %d, message_code = %d}",
							MAGIC_NUMBER, b, MSG_REQUEST);

						recv(sockfd, buffer_recv, RAW_MESSAGE_SIZE, MSG_EOR);
						magic_number_recv = recuperar_Magic_Number(buffer_recv);
						block_number_recv = recuperar_Block_Number(buffer_recv);
						log_printf(LOG_INFO, "      Respuesta {magic_number = %X, block_number = %d, message_code = %d}",
							magic_number_recv, block_number_recv, buffer_recv[4]);

						if (buffer_recv[4] == MSG_RESPONSE_OK)
						{
							block.size = (uint64_t)recv(sockfd, block.data, MAX_BLOCK_SIZE, MSG_EOR);
							log_printf(LOG_INFO, "      Recibimos el bloque. #%d", b);
							log_printf(LOG_INFO, "      Leyendo %d bytes del payload.", block.size);

							if (store_block(&torrent, block_number_recv, &block))
							{
								// block was not stored correctly, either because of an invalid hash or because of an I/O error.
								log_printf(LOG_INFO, "Error al almacenar el bloque #%d", b);
								perror("ERROR");
								exit(0);
							}
							else
							{
								log_printf(LOG_INFO, "      Almacenado bloque. #%d", b);
								torrent.block_map[b] = 1;
								assert(torrent.block_map[b]);
								blocks_complets++;
							}
						}
						else if (buffer_recv[4] == MSG_RESPONSE_NA)
							log_printf(LOG_INFO, "      El server no dispone del bloque #%d", b);

					}
				}
			}
			close(sockfd); //CERRAR SOCKET
			p++; //INCREMENTAR PEER_NUMBER

		}

		if (blocks_complets == torrent.block_count)
			printf("\nMuy bien! Todos los bloques se han descargado :) \n");
		else
			printf("\nNo se han descargado todos los bloques :( \n");

		//Destruimos el torrent !!!!!!
		if (destroy_torrent(&torrent))
		{
			perror("\nError al destruir el torrent");
			exit(0); //EXIT_FAILURE
		}
	}



	/*****************************************************/
        				 // SERVIDOR                                
	/*****************************************************/
	else
	{
		printf("Server mode (port %s) \n", argv[2]);

		//REMOVE EXTENSION
		char downloaded_file[50];
		int flag = 0;

		for (int i = 0, j = 0; flag == 0; i++)
		{
			if (argv[3][i] == '.')
			{
				flag = 1;
				downloaded_file[j] = '\0';
			}
			else
			{
				downloaded_file[j] = argv[3][i];
				j++;
			}
		}

		//CREATE TORRENT
		struct torrent_t torrent;

		if (create_torrent_from_metainfo_file(argv[3], &torrent, downloaded_file) == -1)
		{
			//error handling...
			perror("¡Error in create_torrent_from_metainfo_file()!");
			exit(0);
		}

		//VARIABLES socket
		int sockfd = -1, sockfd_1;
		struct sockaddr_in servaddr;
		servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons((uint16_t)atoi(argv[2]));

		//ABRIR SOCKET
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd == -1)
		{
			perror("¡Fallo en la creación del socket!\n");
			exit(0);
		}
		log_printf(LOG_INFO, "\nSocket creado correctamente.");

		//NON-BLOCKING
		int a = 1;
  		if (ioctl(sockfd, FIONBIO, (char *)&a))
  		{
   			perror("¡Error ioctl!\n");
    		close(sockfd);
    		exit(0);
  		}

		//BIND
		if (bind(sockfd, &servaddr, sizeof(servaddr)) != 0)
		{
			perror("¡Fallo en el bind!\n");
			close(sockfd);
			exit(0);
		}
		log_printf(LOG_INFO, "Bind() ejecutado correctamente.");

		//LISTEN
		if (listen(sockfd, 10) != 0)
		{
			perror("¡Fallo en el listen!\n");
			close(sockfd);
			exit(0);
		}
		log_printf(LOG_INFO, "Listen() ejecutado correctamente.\n");


		//VARIABLES POLL
		struct pollfd fds[10]; 
		int nfds = 1;
		int size_fds; 
		int res_poll, res_recv; 
		int salir = 0, fin_conexion;
		struct block_t block;

		uint8_t buffer_send[RAW_MESSAGE_SIZE];
		uint8_t buffer_recv[RAW_MESSAGE_SIZE];
		uint64_t block_number_recv;
		uint32_t magic_number_recv;

		int return_send;

		//Inicializamos fds
		fds[0].fd = sockfd;
		fds[0].events = POLLIN;

		//BUCLE POLL
		while (!salir) {
			//LLAMAMOS A POLL()

			log_printf(LOG_INFO, "Esperando poll()...\n");
			res_poll = poll(fds, nfds, -1);

			if (res_poll < 0)
			{
				perror("¡Error en poll()!\n");
				exit(0);
			}

			//POLL CORRECTO
			size_fds = nfds;
			for (int i = 0; i < size_fds; i++) //Buscando fd leibles:
			{
				log_printf(LOG_INFO, "      Processing poll index %d {fd: %d, events: %d, revents: %d}", i, fds[i].fd, fds[i].events, fds[i].revents);
				if (fds[i].revents != POLLIN && fds[i].revents !=0)
				{
					perror("¡Error en revents!\n");
					exit(0);
				}
				
				if (fds[i].fd == sockfd) //SOCKET DEL SERVIDOR (PROGRAMA PRINCIPAL)
				{
					log_printf(LOG_INFO, "\nNueva conexión entrante.");
					sockfd_1 = accept(sockfd, NULL, NULL); //ACCEPT		
					
					if (sockfd_1 > 0)
					{				
						log_printf(LOG_INFO, "Conexión entrante aceptada.\n");
						
						//Añadimos a la lista de pollfd
						fds[nfds].fd = sockfd_1;
						fds[nfds].events = POLLIN;
						nfds++; 
					}
				}
				else //SOCKETS DE CONEXIONES ENTRANTES (TTORRENT)
				{
					fin_conexion = 0;
					//PETICIÓN
					res_recv = recv(fds[i].fd, buffer_recv, RAW_MESSAGE_SIZE, 0);

					if (res_recv < 0)
					{
						perror("      ¡Error al recibir del cliente!\n");
						exit(0);
					}
					else if (!res_recv) 
					{
						log_printf(LOG_INFO, "\n      Conexión cerrada :)\n");
						fin_conexion = 1;
					}
					else
					{
						if (buffer_recv[4] == MSG_REQUEST )
						{
							//ENCONTRAR NUMERO DE BLOQUE & MAGIC NUMBER
							block_number_recv = recuperar_Block_Number(buffer_recv);
							magic_number_recv = recuperar_Magic_Number(buffer_recv);
							copiarBuffer(buffer_recv, buffer_send);
							
							log_printf(LOG_INFO, "      Petición del cliente: {magic_number = %X, block_number = %d, message_code = %d}",
												magic_number_recv, block_number_recv, buffer_recv[4]);
												
							//ENVIAMOS RESPUESTA
							if(torrent.block_map[block_number_recv]) //BUSCAMOS SI TENEMOS EL BLOQUE DISPONIBLE:
							{
								//BLOQUE DISPONIBLE

								if (load_block(&torrent, block_number_recv, &block))
								{
									perror("      ¡Error al cargar el bloque!\n");
								}
								//ENVIAMOS BLOQUE
								else
								{
									//sleep
									
									buffer_send[4] = MSG_RESPONSE_OK;

									return_send = send(fds[i].fd, buffer_send, RAW_MESSAGE_SIZE, MSG_EOR); //ENVIAMOS RESPUESTA: MSG_RESPONSE_OK
									return_send += send(fds[i].fd, block.data, block.size, MSG_EOR); //ENVIAMOS BLOQUE

									log_printf(LOG_INFO, "      Mensaje enviado: {magic_number = %X, block_number = %d, message_code = %d}",
												magic_number_recv, block_number_recv, buffer_send[4]);
									log_printf(LOG_INFO, "      Tamaño del mensaje enviado: %d\n", return_send);
								}
							}
							//BLOQUE NO DISPONIBLE
							else 
							{
								log_printf(LOG_INFO, "      Bloque no disponible para enviar.\n");
								buffer_send[4] = MSG_RESPONSE_NA;
								send(fds[i].fd, buffer_send, RAW_MESSAGE_SIZE, MSG_EOR);//ENVIAMOS RESPUESTA: MSG_RESPONSE_NA
								log_printf(LOG_INFO, "      Mensaje enviado: {magic_number = %X, block_number = %d, message_code = %d}",
												magic_number_recv, block_number_recv, buffer_send[4]);
							}
						}
						else
						{
							perror("¡Mensaje recibido incorrecto!\n"); //NO ENTENDEMOS EL MENSAJE DEL CLIENTE
							exit(0);
						}
					}
					if(fin_conexion)
					{
						if(close(fds[i].fd) < 0)
						{
							log_printf(LOG_INFO, "¡Fallo en el close!");
							exit(EXIT_FAILURE);
						}
						fds[i].fd = -1;
						nfds--;
					}
				}	
			}  // VER CUANDO SE CIERRA EL SERVIDOR
		}
		if(close(sockfd) < 0)
		{
			log_printf(LOG_INFO, "¡Fallo en el close!");
			exit(EXIT_FAILURE);
		}
	}

	return 0;
}
