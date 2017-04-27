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

 	header_t header;
 	char* buffer = malloc(file_size + sizeof(header_t));
 	int offset = 0;

 	if(buffer == NULL){
 		log_error(logger, "no pude reservar memoria para enviar archivo");
 		fclose(file);
 		return -1;
 	}

 	header.type = ENVIO_CODIGO;
 	header.length = file_size;

 	memcpy(buffer, &(header.type),sizeof(header.type)); offset+=sizeof(header.type);
 	memcpy(buffer + offset, &(header.length),sizeof(header.length)); offset+=sizeof(header.length);

 	if( fread(buffer + offset,file_size,1,file) < file_size){
 		log_error(logger, "No pude leer el archivo");
 		free(buffer);
 		fclose(file);
 		return -1;
 	}

 	/*Esto lo hago asi porque de la otra forma habría que reservar MAS espacio para
 	 * enviar el paquete */
 	if ( sendAll(kernel_fd, buffer, file_size + sizeof(header_t), 0) <=0 ){
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
	int* pidAsignado;

	if(!verificarExistenciaDeArchivo(param)){
		log_warning(logger, "no existe el archivo");
		printf("El archivo no se encuentra\n");
		return;
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
		printf("No pudo enviarse el archivo\n");
		return;
	}
	log_info(logger,"Archivo enviado correctamente");

	if(recibir_info(socket_cliente, &paquete_vacio, &operacion)==0){
		log_error(logger, "El kernel se desconecto");
		return;
	}

	switch(operacion){
	case PROCESO_RECHAZADO:
		printf("El kernel rechazo el proceso\n");
		log_error(logger, "El kernel rechazo el proceso");
		return;
		break;
	case PID_PROGRAMA:
		pidAsignado = (int*)paquete_vacio;
		break;
	default:
		printf("Se recibio una operacion invalida\n");
		break;
	}

	pthread_t threadPrograma;
	pthread_create(&threadPrograma, NULL, (void*)threadPrograma, socket_cliente);
	pthread_detach(threadPrograma);

	//creo y agrego proceso a la lista
	crearProceso(socket_cliente, (*pidAsignado), threadPrograma);

}

void crearProceso(int socketProceso, int pidAsignado, pthread_t threadPrograma){
	t_proceso* proc = malloc(sizeof(t_proceso));
	proc->socket = socketProceso;
	proc->pid = pidAsignado;
	proc->thread = threadPrograma;

	list_add(procesos, proc);
}

bool esNumero(char* string){
	int size = strlen(string);
	int i;

	for (i=0 ; i < size ; i++){
		if(!isdigit(string[i])) return false;
	}
	return true;
}

void threadPrograma(int socketProceso){

	int operacion;
	void* paquete;
	bool procesoActivo = true;

	bool* buscar(t_proceso* proc){
		return proc->socket == socketProceso ? true : false;
	}

	while(procesoActivo){

		if(recibir_info(socketProceso, &paquete, &operacion)==0){
			log_error(logger, "El kernel se desconecto");
			return;
		}else{

			switch (operacion) {
			case FINALIZAR_EJECUCION:
				procesoActivo = false;
				list_remove_and_destroy_by_condition(procesos, buscar, free);
				break;
			case IMPRIMIR_TEXTO_PROGRAMA:
				printf("%s\n", (char*)paquete);
				break;
			case IMPRIMIR_VARIABLE_PROGRAMA:
				printf("%d\n", (int)paquete);
				break;
			default:
				break;
			}

		}

	}

}

void finalizarPrograma(char* comando, char* param){

	if(!esNumero(param)){
		printf("Valor de pid invalido\n");
		return;
	}

	int pid = strtol(param, NULL, 10);

	bool buscarProceso(t_proceso* p){
		return p->pid == pid? true : false;
	}

	t_proceso* proceso = list_find(procesos, buscarProceso);
	if(proceso == NULL){
		printf("Ese proceso no se encuentra en el sistema\n");
		return;
	}

	t_proceso* proc = list_find(procesos, buscarProceso);
	//evaluar si debo avisar al kernel o si al desconectarse el socket el kernel lo maneje solo
	terminarProceso(proc);

	printf("Proceso finalizado\n");
}

void desconectarConsola(char* comando, char* param) {
	list_destroy_and_destroy_elements(procesos,terminarProceso);

	exit(1);
}

void terminarProceso(t_proceso* proc){
	pthread_cancel(proc->thread);
	free(proc);
}

void limpiarMensajes(char* comando, char* param) {
	//me doy asco por usar system
	system("clear");
}

int crearLog() {
	logger = log_create("logConsola","consola", 1, LOG_LEVEL_TRACE);
	if (logger) {
		return 1;
	} else {
		return 0;
	}
}
