/*
 * funcionesFs.h
 *
 *  Created on: 7/4/2017
 *      Author: utnso
 */

#ifndef FUNCIONESFS_H_
#define FUNCIONESFS_H_

#include <stdio.h>
#include <stdlib.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/sockets.h>
#include <commons/log.h>
#include <commons/bitarray.h>

#define configuracionFS "../confFileSystem.init"
#define MAX_LEN_PUERTO 6
#define IP "127.0.0.1"
#define BACKLOG 10
#define METADATA_PATH "Metadata"
#define METADATA_ARCHIVO "/Metadata.bin"
#define BITMAP_ARCHIVO "/Bitmap.bin"
#define ARCHIVOS_PATH "Archivos"
#define BLOQUES_PATH "Bloques"

//TADS
typedef struct{
	char* puertoEscucha;
	char* punto_montaje;
	int tamanio_bloque;
	int cantidad_bloques;
}t_config_FS;

typedef struct
{
	char* path;
	int offset;
	int size;
} pedido_obtener_datos;

typedef struct
{
	char* path;
	int offset;
	int size;
	char* buffer;
}pedido_guardar_datos;

//Prototipos
t_config_FS* levantarConfiguracion(char* archivo);
void destruirConfiguracionFS(t_config_FS* conf);
void esperarConexionKernel();
void crearConfig(int argc, char* argv[]);

void inicializarMetadata();
void procesarMensajesKernel();
void mkdirRecursivo(char* path);
int buscarBloqueLibre();
char** obtenerNumeroBloques(char* path);
int obtenerNumBloque(char* path, int offset);

//Operaciones
bool validarArchivo(char* path);
void crearArchivo(void* package);
void borrarArchivo(void* package);
void guardarDatos(void* package);
void obtenerDatos(void* package);

//Variables Globales
t_config_FS* conf;
int socketEscucha;
int socketConexionKernel;
t_log* logger;
t_bitarray* bitarray;

#endif /* FUNCIONESFS_H_ */
