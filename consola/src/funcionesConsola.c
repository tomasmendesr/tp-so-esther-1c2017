#include "funcionesConsola.h"

void crearConfig(int argc, char* argv[]) {
	char* pathConfig = string_new();

	if (argc>1)string_append(&pathConfig, argv[1]);
		else string_append(&pathConfig, configuracionConsola);
	if (verificarExistenciaDeArchivo(pathConfig)) {
		config = levantarConfiguracionConsola(pathConfig);
	} else {
		log_info(logger, "No Pudo levantarse el archivo de configuracion");
		exit(EXIT_FAILURE);
	}
	printf("Configuracion levantada correctamente\n");
	return;
}

t_config_consola* levantarConfiguracionConsola(char * archivo) {

	t_config_consola* config = malloc(sizeof(t_config_consola));
	t_config* configConsola;

	configConsola = config_create(archivo);

	config->ip_Kernel = malloc(
			strlen(config_get_string_value(configConsola, "IP_KERNEL")) + 1);
	strcpy(config->ip_Kernel,
			config_get_string_value(configConsola, "IP_KERNEL"));

	config->puerto_Kernel = malloc(
			strlen(config_get_string_value(configConsola, "PUERTO_KERNEL")) + 1);
	strcpy(config->puerto_Kernel,
			config_get_string_value(configConsola, "PUERTO_KERNEL"));

	config_destroy(configConsola);

	return config;
}

/* Esta funcion es genial, la probe y anda bien
 * Pero me di cuenta de que el codigo se podia hacer un poco mas amigable
 * si usaba una funcion de socket que hice ayer (refactorizando el codigo de Tomi)
 * Podemos dejar cualquiera de las dos, es lo mismo
 * Lo unico que cambie es la manera de mandar y recibir el contenido del archivo
 * Saludos!
 */

//int enviarArchivo(int kernel_fd, char* path){
//
//	//Verifico existencia archivo (Aguante esta funcion loco!)
// 	if( !verificarExistenciaDeArchivo(path) ){
// 		log_error(logger, "no existe el archivo");
// 		return -1;
// 	}
//
// 	FILE* file;
// 	int file_fd, file_size;
// 	struct stat stats;
//
// 	//Abro el archivo y le saco los stats
// 	file = fopen(path, "r");
// 	//esto nunca deberia fallar porque ya esta verificado, pero por las dudas
// 	if(file == NULL){
// 		log_error(logger, "no pudo abrir archivo");
// 		return -1;
// 	}
// 	file_fd = fileno(file);
//
// 	fstat(file_fd, &stats);
// 	file_size = stats.st_size;
//
// 	header_t header;
// 	char* buffer = malloc(file_size + sizeof(header_t));
// 	int offset = 0;
//
// 	if(buffer == NULL){
// 		log_error(logger, "no pude reservar memoria para enviar archivo");
// 		fclose(file);
// 		return -1;
// 	}
//
// 	header.type = ENVIO_CODIGO;
// 	header.length = file_size;
//
// 	memcpy(buffer, &(header.type), offset += sizeof(header.type));
// 	memcpy(buffer + offset, &(header.length), sizeof(header.length));
// 	offset += sizeof(header.length);
//
// 	if( fread(buffer + offset,file_size,1,file) < file_size){
// 		log_error(logger, "No pude leer el archivo");
// 		free(buffer);
// 		fclose(file);
// 		return -1;
// 	}
//
// 	/*Esto lo hago asi porque de la otra forma habría que reservar MAS espacio para
// 	 * enviar el paquete */
// 	if ( sendAll(kernel_fd, buffer, file_size + sizeof(header_t), 0) <=0 ){
// 		log_error(logger, "Error al enviar archivo");
// 		free(buffer);
// 		fclose(file);
// 		return -1;
// 	}
//
// 	free(buffer);
// 	fclose(file);
// 	return 0;
//}

int enviarArchivo(int kernel_fd, char* path){

	//Verifico existencia archivo (Aguante esta funcion loco!)
 	if( !verificarExistenciaDeArchivo(path) ){
 		log_error(logger, "no existe el archivo");
 		return -1;
 	}

 	FILE* file;
 	int file_fd, file_size;
 	struct stat stats;

 	//Abro el archivo y le saco los stats
 	file = fopen(path, "r");
 	//esto nunca deberia fallar porque ya esta verificado, pero por las dudas
 	if(file == NULL){
 		log_error(logger, "no pudo abrir archivo");
 		return -1;
 	}
 	file_fd = fileno(file);

 	fstat(file_fd, &stats);
 	file_size = stats.st_size;

 	char* buffer = malloc(file_size);

 	if(buffer == NULL){
 		log_error(logger, "no pude reservar memoria para enviar archivo");
 		fclose(file);
 		return -1;
 	}

 	if( fread(buffer,file_size,1,file) < file_size){
 		log_error(logger, "No pude leer el archivo");
 		free(buffer);
 		fclose(file);
 		return -1;
 	}

 	if ( enviar_info(kernel_fd,ENVIO_CODIGO,file_size,buffer) == -1 ){
 		log_error(logger, "Error al enviar archivo");
 		free(buffer);
 		fclose(file);
 		return -1;
 	}

 	free(buffer);
 	fclose(file);
 	return 0;
}

//funciones interfaz
void levantarInterfaz() {
	//creo los comandos y el parametro
	comando* comandos = malloc(sizeof(comando) * 4);

	strcpy(comandos[0].comando, "start");
	comandos[0].funcion = iniciarPrograma;
	strcpy(comandos[1].comando, "stop");
	comandos[1].funcion = finalizarPrograma;
	strcpy(comandos[2].comando, "disconnect");
	comandos[2].funcion = desconectarConsola;
	strcpy(comandos[3].comando, "clean");
	comandos[3].funcion = limpiarMensajes;

	interface_thread_param* params = malloc(sizeof(interface_thread_param));
	params->comandos = comandos;
	params->cantComandos = 4;

	//Lanzo el thread
	pthread_attr_t atributos;
	pthread_attr_init(&atributos);
	pthread_create(&threadInterfaz, &atributos, (void*)interface, params);

	return;
}

void iniciarPrograma(char* comando, char* param) {

	int socket_cliente;

	if(verificarExistenciaDeArchivo(param))
		printf("Su proceso se inicializo");
	else{
		log_error(logger, "no existe el archivo");
		exit(1);
	}
	socket_cliente = createClient(config->ip_Kernel, config->puerto_Kernel);
	if (socket_cliente != -1) {
		printf("Cliente creado satisfactoriamente.\n");
	}else{
		perror("No se pudo crear el cliente");
	}
	enviar_paquete_vacio(HANDSHAKE_PROGRAMA, socket_cliente);
	int operacion = 0;
	void* paquete_vacio;

	recibir_info(socket_cliente, &paquete_vacio, &operacion);

	if (operacion == HANDSHAKE_KERNEL) {
		printf("Conexion con Kernel establecida! :D \n");
		printf("Se procede a mandar el archivo: %s\n", param);
	} else {
		printf("El Kernel no devolvio handshake :( \n");
	}

	if((enviarArchivo(socket_cliente, param))==-1){
		log_error(logger,"No se pudo mandar el archivo");
		exit(1);
	}
	log_info(logger,"Archivo enviado correctamente");

	if(recibir_info(socket_cliente, &paquete_vacio, &operacion)<0){
		log_error(logger, "El kernel se desconecto");
		exit(1);
	}

	t_param_consola param_consola;

	if(operacion == PID_PROGRAMA){
		param_consola.fd=socket_cliente;
		param_consola.pid=*(int*)paquete_vacio;
	}

	pthread_t threadPrograma;
	pthread_attr_t atributos;
	pthread_attr_init(&atributos);
	pthread_create(&threadPrograma, &atributos, (void*)impresora, &param_consola);
	pthread_detach(threadPrograma);
}

void finalizarPrograma(char* comando, char* param){
	printf("finalizarPrograma\n");
}

void desconectarConsola(char* comando, char* param) {
	//Aca va a tener que ir toda la logica de limpiar variables finalizar proceso o algo
	//AL menos que se la prueba del cierre Total de los programas.
	exit(1);
}

void limpiarMensajes(char* comando, char* param) {
	printf("limpiarMensajes");
}

int crearLog() {
	logger = log_create(getenv("../logConsola"),"consola", 1, 0);
	if (logger) {
		return 1;
	} else {
		return 0;
	}
}

void impresora(void* param){

	int operacion;
	void* paquete_vacio;
	t_param_consola*param_consola=param;

	for(;;){
		if(recibir_info(param_consola->fd, &paquete_vacio, &operacion)<0){
				log_error(logger, "El kernel se desconecto");
				exit(1);
		}
		switch(operacion){
		/* hay que definir como manda las ordenes el kernel */
			case IMPRIMIR_VARIABLE_PROGRAMA:
				printf("%d %d\n",param_consola->pid,*(int*)paquete_vacio);
				break;
			case IMPRIMIR_TEXTO_PROGRAMA:
				printf("%d %s\n",param_consola->pid,(char*)paquete_vacio);
				break;
		}
	}
}
