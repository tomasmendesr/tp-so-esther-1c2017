/*
 * kernel.h
 *
 *  Created on: 4/4/2017
 *      Author: utnso
 */

#ifndef KERNEL_H_
#define KERNEL_H_

#include <stdio.h>
#include <stdlib.h>
#include <commons/sockets.h>
#include <string.h>
#include <commons/collections/queue.h>
#include <commons/collections/dictionary.h>
#include <commons/log.h>
#include <stdbool.h>
#include <commons/config.h>
#include <pthread.h>
#include <commons/collections/list.h>

typedef struct {
        int puerto_PROG;
        int puerto_CPU;
        char* ip_Memoria;
        int puerto_Memoria;
        char* ip_FS;
        int puerto_FS;
        int quantum;
        int quantum_Sleep;
        char* algoritmo;
        int grado_MultiProg;
        t_dictionary* semaforos;
        t_dictionary* variablesGlobales;
        int stack_Size;

} t_config_kernel;

t_config_kernel* levantarConfiguracionKernel(char* archivo_conf);
t_dictionary* crearDiccionarioConValue(char** array, char** valores);
t_dictionary* crearDiccionario(char** array);

//Funciones de interfaz
void levantarInterfaz();
void listProcesses(char* comando, char* param);
void processInfo(char* comando, char* param);
void getTablaArchivos(char* comando, char* param);
void gradoMultiprogramacion(char* comando, char* param);
void killProcess(char*,char*);
void stopPlanification(char*,char*);



#endif /* KERNEL_H_ */
