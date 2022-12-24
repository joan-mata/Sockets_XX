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

// TODO: hey!? what is this?

// https://en.wikipedia.org/wiki/Magic_number_(programming)#In_protocols
static const uint32_t MAGIC_NUMBER = 0xde1c3230; //decimal => 3726389808

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


int main(int argc, char **argv) {
    
	set_log_level(LOG_DEBUG);
	log_printf(LOG_INFO, "Trivial Torrent (build %s %s) by %s", __DATE__, __TIME__, "J. MATA and G. PINEDA");

    //CLIENT --->  ./ttorrent test_file.ttorrent
	if (argc < 3){
        printf("Client mode \n");

        //REMOVE EXTENSION
        char downloaded_file[50];
        int flag = 0;
        
        for (int i = 0, j=0; flag==0; i++)
        {
            if (argv[1][i]=='.')
            {
                flag = 1;
                downloaded_file[j]='\0';
            }
            else
            {
                downloaded_file[j]=argv[1][i];
                j++;
            }
        }
        
        //CREATE TORRENT
		struct torrent_t torrent;
		
		if(create_torrent_from_metainfo_file(argv[1], &torrent, downloaded_file)==-1)
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
        //memcpy(buffer_send, &MAGIC_NUMBER, 4);
        //memcpy(buffer_send + 4, &MSG_REQUEST, 4);

        for (uint64_t i = 0; i < torrent.block_count; i++)
            if(torrent.block_map[i])
                blocks_complets++;
        
        while(p < torrent.peer_count && blocks_complets < torrent.block_count)   //RECORREMOS LOS PUERTOS; p => PEER_NUMBER
        {
            //INICIALIZAR SOCKET
            servaddr.sin_port = torrent.peers[p].peer_port;
            
            //ABRIR SOCKET
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if(sockfd == -1)
            {
                log_printf(LOG_INFO, "Fallo en la creación del socket");
                exit(0);
            }
            
            //CONECTARSE AL SERVER
            if(connect(sockfd, (const struct sockaddr *) &servaddr, sizeof(servaddr)))
            {
                log_printf(LOG_INFO, "\nError al conectarse al server, peer #%d ", p);
                //perror("ERROR");
                //exit(0); //EXIT_FAILURE
            }
            else
            {
                log_printf(LOG_INFO, "\nConectado con éxito al PEER #%d: ", p);
                    
                for(uint64_t b = 0; b < torrent.block_count; b++) //RECORREMOS LOS BLOQUES; b => BLOCK_NUMBER
                {
                        if(!torrent.block_map[b]) //SI NO TENEMOS EL BLOQUE EN MEMORIA:
                        {
                            log_printf(LOG_INFO, "\n      Pedimos el bloque #%d: ", b);
                            insert_num_block(buffer_send, b); //insertar en buffer el numero de bloque
                            //memcpy(buffer_send + 5, &b, 4);

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
                                block.size = (uint64_t) recv(sockfd, block.data, MAX_BLOCK_SIZE, MSG_EOR);
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
                            else if(buffer_recv[4] == MSG_RESPONSE_NA)
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
        if(destroy_torrent(&torrent))
        {
            perror("\nError al destruir el torrent");
            exit(0); //EXIT_FAILURE
        }
    }

    //SERVIDOR  ---> ./ttorrent -l 8080 test_file.ttorrent
	else
    {
        printf("Server mode (port %s) \n", argv[2]);
       //int create_torrent_from_metainfo_file ( argv[3], tt_server, downloaded_file_name);
    }
    
	// Parse command line
    
    
     

	// TODO: some magical lines of code here that call other functions and do various stuff.

	

	return 0;
}
