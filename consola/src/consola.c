/*
 ============================================================================
 Name        : consola.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */


#include "funcionesConsola.h"

int main(int argc, char** argv){

	crearLog();

	crearConfig(argc,argv);

	levantarInterfaz();

	pthread_join(threadInterfaz,NULL);
	return EXIT_SUCCESS;
}
