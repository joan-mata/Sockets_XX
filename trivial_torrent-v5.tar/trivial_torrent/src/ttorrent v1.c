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

//memspy(buffer_send, &MAGIC_NUMBER, 4);
//memspy(buffer_send + 4, &MAGIC_NUMBER, 4);
//

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
    buffer[RAW_MESSAGE_SIZE - 1] = MSG_REQUEST;
    //buffer[4] = MSG_REQUEST;

}

void insert_num_block(uint8_t buffer[RAW_MESSAGE_SIZE], uint64_t block_number) //pasar BLOCK_NUMBER to buffer
{
    uint64_t aux = block_number;
    /*
    //[5,12]
    buffer[5] = (uint8_t) aux;
    for (int i = 6; i < RAW_MESSAGE_SIZE; i++)
    {
        aux = (aux >> 8);
        buffer[i] = (uint8_t) aux;
    }
    */
    //[4,11]
    buffer[4] = (uint8_t) aux;
    for (int i = 5; i < RAW_MESSAGE_SIZE - 1; i++)
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
    /*
    //[5,12]
    uint64_t aux = buffer[RAW_MESSAGE_SIZE - 1];
    for (int i = (RAW_MESSAGE_SIZE - 2); i >= 5; i--)
    {
        aux = (aux << 8);
        aux |= (uint64_t) buffer[i];
    }
    return aux;
    */
    //[4,11]
    uint64_t aux = buffer[RAW_MESSAGE_SIZE - 2];
    for (int i = (RAW_MESSAGE_SIZE - 3); i >= 4; i--)
    {
        aux = (aux << 8);
        aux |= (uint64_t) buffer[i];
    }
    return aux;
    
}
/*
//NUESTRO
MAGIC_NUMBER [0,3]
BLOCK_NUMBER [4,11]
MSG_REQUEST/RESPONSE[12]

//PROFE
MAGIN_NUMBER [0,3]
MSG_REQUEST/RESPONSE[4]
BLOCK_NUMBER[4,12]
 */

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
    for (int i = 0; i < 30; i++)
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




//INFORMACIÓN SOBRE LOS PRARÁMETROS QUE LE PASAMOS AL MAIN
    //argc = número de argumentos del comando => min = 1
    //argv = array de púnteros a caracteres
    //argv[0] = ./ttorrent
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
        int p = 0, blocks_complets = 0, size = 0;
        struct block_t block;
        uint8_t buffer_send[RAW_MESSAGE_SIZE], buffer_recv[RAW_MESSAGE_SIZE];
        uint32_t magic_number_recv;
        uint64_t block_number_recv;
        
        init_buffer(buffer_send); //inicializar buffer con MAGIC_NUMBER y MSG_REQUEST
        
        for (int i = 0; i < torrent.block_count; i++)
            if(torrent.block_map[i])
                blocks_complets++;
        
        while(p < torrent.peer_count && blocks_complets < torrent.block_count)   //RECORREMOS LOS PUERTOS; p => PEER_NUMBER
        {
            //INICIALIZAR SOCKET
            servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); //IP_addr
            servaddr.sin_port = torrent.peers[p].peer_port;
            servaddr.sin_family = AF_INET;
            
            //ABRIR SOCKET
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if(sockfd == -1)
            {
                printf("Fallo en la creación del socket");
                exit(0);
            }
            
            //CONECTARSE AL SERVER
            if(connect(sockfd, (struct sockaddr_in *) &servaddr, sizeof(servaddr)))
            {
                printf("\nError al conectarse al server, peer #%d \n", p);
                //perror("ERROR");
                //exit(0); //EXIT_FAILURE
            }
            else
            {
                printf("\nConectado con éxito al PEER #%d: \n", p);
                    
                for(int b = 0; b < torrent.block_count; b++) //RECORREMOS LOS BLOQUES; b => BLOCK_NUMBER
                {
                        if(!torrent.block_map[b]) //SI NO TENEMOS EL BLOQUE EN MEMORIA:
                        {
                            printf("\n      Pedimos el bloque #%d: \n", b);
                            insert_num_block(buffer_send, b); //insertar en buffer el numero de bloque

                            //buffer_send[4] = 0;
                            //buffer_send[11] = b;
                            
                            send(sockfd, buffer_send, RAW_MESSAGE_SIZE, MSG_EOR);
                            log_printf(LOG_INFO, "      Petición {magic_number = %X, block_number = %d, message_code = %d}",
                                       MAGIC_NUMBER, b, MSG_REQUEST);
                            
                            recv(sockfd, buffer_recv, RAW_MESSAGE_SIZE, MSG_EOR);
                            magic_number_recv = recuperar_Magic_Number(buffer_recv);
                            block_number_recv = recuperar_Block_Number(buffer_recv);
                            log_printf(LOG_INFO, "      Respuesta {magic_number = %X, block_number = %d, message_code = %d}",
                                       magic_number_recv, block_number_recv, buffer_recv[RAW_MESSAGE_SIZE - 1]);
                            
                            //printf_buffer(buffer_send);
                            printf_buffer(buffer_recv);
                            //printf("size: %d \n", block.size);
                            //printf_block(block);
                            
                            if (buffer_recv[RAW_MESSAGE_SIZE - 1] == MSG_RESPONSE_OK)
                            {
                                block.size = recv(sockfd, block.data, MAX_BLOCK_SIZE, MSG_EOR);
                                printf("      Recibimos el bloque. #%d \n", b);
                                printf("      Leyendo %d bytes del payload. \n", block.size);
                                
                                if (store_block(&torrent, b, &block))
                                {
                                    // block was not stored correctly, either because of an invalid hash or because of an I/O error.
                                    printf("Error al almacenar el bloque #%d \n", b);
                                    perror("ERROR");
                                    exit(0);
                                }
                                else
                                {
                                    printf("      Almacenado bloque. #%d \n", b);
                                    torrent.block_map[b] = 1;
                                    assert(torrent.block_map[b]);
                                    blocks_complets++;
                                }
                            }
                            else if(buffer_recv[RAW_MESSAGE_SIZE - 1] == MSG_RESPONSE_NA)
                                printf("      El server no dispone del bloque #%d \n", b);
                            
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

        struct torrent_t tt_server;
        struct block_t b_server;

       //int create_torrent_from_metainfo_file ( argv[3], tt_server, downloaded_file_name);
    }
    
	// Parse command line
    
    
     

	// TODO: some magical lines of code here that call other functions and do various stuff.

	

	return 0;
}
