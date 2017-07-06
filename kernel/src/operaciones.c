#include "operaciones.h"

void trabajarMensajeConsola(int32_t socketConsola){

	int tipo_mensaje; //Para que la funcion recibir_string lo reciba
	void* paquete;
	int check = recibir_paquete(socketConsola, &paquete, &tipo_mensaje);


	if(check <= 0){
		log_warning(logger, "Se cerro el socket %d (Consola)", socketConsola);
		verificarProcesosConsolaCaida(socketConsola);
		close(socketConsola);
		if(paquete)free(paquete);
		FD_CLR(socketConsola, &master);
		FD_CLR(socketConsola, &setConsolas);
	}else{
		FD_SET(socketConsola, &setConsolas);
		procesarMensajeConsola(socketConsola, tipo_mensaje, paquete);
	}

}

void procesarMensajeConsola(int32_t consola_fd, int32_t mensaje, char* package){
	proceso_en_espera_t* nuevoProceso;

	switch(mensaje){
	case HANDSHAKE_PROGRAMA:
		enviar_paquete_vacio(HANDSHAKE_KERNEL,consola_fd);
		log_info(logger,"Handshake con consola");
		break;
	case ENVIO_CODIGO:
		log_info(logger, "Recibo codigo");
		nuevoProceso = crearProcesoEnEspera(consola_fd, package);
		sem_wait(&mutex_cola_new);
		queue_push(colaNew, nuevoProceso);
		sem_post(&mutex_cola_new);
		crearInfoEstadistica(nuevoProceso->pid, consola_fd);
		crearEntradaArchivoProceso(nuevoProceso->pid);
		log_info(logger,"Proceso agregado a la cola New");
		planificarLargoPlazo();
	break;
	case FINALIZAR_PROGRAMA:
		finalizarPrograma(consola_fd,*(int*)package);
		break;
	default: log_warning(logger,"Se recibio un codigo de operacion invalido de consola. %d", mensaje);
	break;
	}
}

void trabajarMensajeCPU(int socketCPU){

	int tipo_mensaje; //Para que la funcion recibir_string lo reciba
	void* paquete;
	int check = recibir_paquete(socketCPU, &paquete, &tipo_mensaje);

	//Chequeo de errores
	if(check <= 0){
		log_warning(logger,"Se cerro el socket %d (cpu)", socketCPU);
		verificarProcesosEnCpuCaida(socketCPU);
		close(socketCPU);
		if(paquete)free(paquete);
		FD_CLR(socketCPU, &master);
		FD_CLR(socketCPU, &setCPUs);
	}else{
		procesarMensajeCPU(socketCPU, tipo_mensaje, paquete);
		FD_SET(socketCPU, &setCPUs);
	}

}

void procesarMensajeCPU(int socketCPU, int mensaje, char* package){
	switch(mensaje){
	case HANDSHAKE_CPU:
		log_info(logger,"Conexion con nueva CPU establecida");
		enviar_paquete_vacio(HANDSHAKE_KERNEL,socketCPU);
		enviarTamanioStack(socketCPU);
		enviarQuantumSleep(socketCPU);
		agregarNuevaCPU(listaCPUs, socketCPU);
		break;
	case SEM_SIGNAL:
		realizarSignal(socketCPU, package);
		break;
	case SEM_WAIT:
		realizarWait(socketCPU, package);
		break;
	case LEER_VAR_COMPARTIDA:
		leerVarCompartida(socketCPU, package);
		break;
	case ASIG_VAR_COMPARTIDA:
		asignarVarCompartida(socketCPU, package);
		break;
	case RESERVAR_MEMORIA:
		reservarMemoria(socketCPU, package);
		break;
	case LIBERAR_MEMORIA:
		liberarMemoria(socketCPU, package);
		break;
	case ABRIR_ARCHIVO:
		abrirArchivo(socketCPU, package);
		break;
	case CERRAR_ARCHIVO:
		cerrarArchivo(socketCPU, package);
		break;
	case BORRAR_ARCHIVO:
		borrarArchivo(socketCPU, package);
		break;
	case ESCRIBIR:
		escribir(package, socketCPU);
		break;
	case LEER_ARCHIVO:
		leerArchivo(socketCPU, (t_lectura*) package);
		break;
	case MOVER_CURSOR:
		moverCursor(socketCPU, (t_cursor*) package);
		break;
	/* CPU DEVUELVE EL PCB */
	case FIN_PROCESO:
		finalizacion_proceso(package, socketCPU);
		break;
	case FIN_EJECUCION:
		finalizacion_quantum(package,socketCPU);
		break;
	/* ERRORES */
	case STACKOVERFLOW:
		finalizacion_stackoverflow(package, socketCPU);
		break;
	case ERROR_MEMORIA:
	case SEMAFORO_NO_EXISTE:
	case GLOBAL_NO_DEFINIDA:
	case NULL_POINTER:
	case ARCHIVO_INEXISTENTE:
	case FALLA_RESERVAR_RECURSOS:
	case SEGMENTATION_FAULT:
	case SIN_ESPACIO_FS:
		finalizacion_error(package, socketCPU, mensaje);
		break;
	default:
		log_warning(logger,"Se recibio el codigo de operacion invalido de CPU. %d", mensaje );
	}
}

void leerVarCompartida(int32_t socketCPU, char* variable){
	if(dictionary_has_key(config->variablesGlobales, variable)){
			int32_t valor = leerVariableGlobal(config->variablesGlobales, variable);
			header_t* header = malloc(sizeof(header_t));
			header->type = VALOR_VAR_COMPARTIDA;
			header->length = sizeof(valor);
			sendSocket(socketCPU, header, &valor);
			free(header);
	}else{
		log_error(logger, "Error al leer var compartida %s. No se encontro", variable);
		enviar_paquete_vacio(GLOBAL_NO_DEFINIDA, socketCPU);
	}
}

void asignarVarCompartida(int32_t socketCPU, void* buffer){
	char* variable;
	int32_t valor, sizeVariable;
	memcpy(&sizeVariable, buffer, 4);
	variable = malloc(sizeof(sizeVariable));
	memcpy(variable, buffer+4, sizeVariable);
	if(dictionary_has_key(config->variablesGlobales, variable)){
		memcpy(&valor, buffer+4+sizeVariable, 4);
		escribirVariableGlobal(config->variablesGlobales, variable, valor);
		free(variable);
		aumentarEstadisticaPorSocketAsociado(socketCPU, estadisticaAumentarOpPriviligiada);
		enviar_paquete_vacio(ASIG_VAR_COMPARTIDA_OK, socketCPU);
	}
	else{
		log_error(logger, "No se encontro la variable compartida %s. Se finaliza ejecucion", variable);
		enviar_paquete_vacio(GLOBAL_NO_DEFINIDA, socketCPU);
	}
}

void realizarSignal(int32_t socketCPU, char* key){
	aumentarEstadisticaPorSocketAsociado(socketCPU, estadisticaAumentarOpPriviligiada);
	if(dictionary_has_key(config->semaforos, key)){
		if(dictionary_get(config->semaforos, key) == NULL){
			log_error(logger, "El valor de %s es NULL", key);
			 enviar_paquete_vacio(NULL_POINTER, socketCPU);
			 return;
		}
		else{
			int32_t valor = semaforoSignal(config->semaforos, key);
			if(valor <= 0){
				desbloquearProceso(key);
				log_info(logger, "Desbloqueo un proceso");
			}
			enviar_paquete_vacio(SIGNAL_OK, socketCPU);
		}
	}
	else{
		log_error(logger ,"No se encontro el semaforo %s. Se finaliza la ejecucion", key);
		enviar_paquete_vacio(SEMAFORO_NO_EXISTE, socketCPU);
	}
}

void realizarWait(int32_t socketCPU, char* key){
	int32_t resultado;
	aumentarEstadisticaPorSocketAsociado(socketCPU, estadisticaAumentarOpPriviligiada);

	if(dictionary_has_key(config->semaforos,key)){
		if(dictionary_get(config->semaforos, key) == NULL){
			log_error(logger, "El valor de %s es NULL", key);
			 enviar_paquete_vacio(NULL_POINTER, socketCPU);
			 return;
		}
		else{
			int32_t valor = semaforoWait(config->semaforos, key);
			if(valor < 0) resultado = WAIT_DETENER_EJECUCION;
			else resultado = WAIT_SEGUIR_EJECUCION;

			enviar_paquete_vacio(resultado, socketCPU);

			if(resultado == WAIT_DETENER_EJECUCION){ //recibo el pcb

				int32_t tipo_mensaje;
				void* paquete;
				recibir_paquete(socketCPU, &paquete, &tipo_mensaje);
				if(tipo_mensaje == PROC_BLOCKED){
					t_pcb* pcbRecibido = deserializar_pcb(paquete);
					bloquearProceso(key, pcbRecibido);
					desocupar_cpu(socketCPU);
					log_info(logger, "Bloqueo proceso %d", pcbRecibido->pid);
					free(paquete);
				}
				else{
					log_error(logger, "Se esperaba el mensaje PROC_BLOCKED y se recibio otro");
				}
			}
		}
	}else{
		log_error(logger ,"No se encontro el semaforo %s. Se finaliza la ejecucion", key);
		enviar_paquete_vacio(SEMAFORO_NO_EXISTE, socketCPU);
	}
}

void finalizarPrograma(int32_t consola_fd, int32_t pid){
	info_estadistica_t* info = buscarInformacion(pid);
	info->matarSiguienteRafaga = true;
	info->exitCode = FINALIZAR_DESDE_CONSOLA;
	log_info(logger, "Se termina la ejecucion del proceso #%d por comando STOP (Consola)", pid);
}

void finalizacion_segment_fault(void* paquete_from_cpu, int32_t socket_cpu){
	t_pcb* pcbRecibido =  deserializar_pcb(paquete_from_cpu);
	log_error(logger, "Finaliza el proceso #%d por segmentation fault", pcbRecibido->pid);
	pcbRecibido->exitCode = ERROR_MEMORIA;
	terminarProceso(pcbRecibido, socket_cpu);
}

void finalizacion_error(void* paquete_from_cpu, int32_t socket_cpu, int32_t exitCode){
	t_pcb* pcbRecibido = deserializar_pcb(paquete_from_cpu);
	log_error(logger, "Finaliza el proceso #%d por error", pcbRecibido->pid);
	pcbRecibido->exitCode = exitCode;
	terminarProceso(pcbRecibido, socket_cpu);
}

void terminarProceso(t_pcb* pcbRecibido, int32_t socket_cpu){
	//modifico informacion estadistica
	estadisticaAumentarRafaga(pcbRecibido->pid);
	log_info(logger, "Proceso %d agregado a la cola de FINISHED", pcbRecibido->pid);
	estadisticaCambiarEstado(pcbRecibido->pid, FINISH);

	//pongo pcb en cola finish
	queue_push(colaFinished, pcbRecibido);

	//libero la cpu
	desocupar_cpu(socket_cpu);

	//aviso a consola que termino el proceso
	info_estadistica_t * info = buscarInformacion(pcbRecibido->pid);

	header_t* header=malloc(sizeof(header_t));
	if(!(pcbRecibido->exitCode == DESCONEXION_CONSOLA)){
		header->type=FINALIZAR_EJECUCION;
		header->length=sizeof(pcbRecibido->exitCode);
		sendSocket(info->socketConsola,header,&(pcbRecibido->exitCode));
	}

	header->type = FINALIZAR_PROGRAMA;
	header->length = sizeof(pcbRecibido->pid);
	void* paquete;
	int resultado;
	pthread_mutex_lock(&mutex_memoria_fd);
	sendSocket(socketConexionMemoria,header,&pcbRecibido->pid);
	recibir_paquete(socketConexionMemoria, &paquete,&resultado);
	pthread_mutex_lock(&mutex_memoria_fd);

	cantProcesosSistema--;
	freePCB(pcbRecibido);
	free(header);
	planificarLargoPlazo();
}

void finalizacion_stackoverflow(void* paquete_from_cpu, int32_t socket_cpu){
	t_pcb* pcbRecibido = deserializar_pcb(paquete_from_cpu);
	log_error(logger, "Finaliza el proceso %d por stack overflow", pcbRecibido->pid);
	pcbRecibido->exitCode = SUPERO_TAMANIO_PAGINA;
	terminarProceso(pcbRecibido, socket_cpu);
}

void finalizacion_quantum(void* paquete_from_cpu, int32_t socket_cpu) {
	t_pcb* pcb_recibido =  deserializar_pcb(paquete_from_cpu);
	log_debug(logger, "Fin de quantum proceso %d", pcb_recibido->pid);

	estadisticaAumentarRafaga(pcb_recibido->pid);

	info_estadistica_t* info = buscarInformacion(pcb_recibido->pid);

	if(info->matarSiguienteRafaga){
		pcb_recibido->exitCode = info->exitCode;
		terminarProceso(pcb_recibido, socket_cpu);
		return;
	}else{
		// Se debe actualizar el PCB. Para ello, directamente se lo elimina de EXEC y se ingresa en READY el pcb recibido (que resulta ser el pcb actualizado del proceso).
		sem_wait(&mutex_cola_exec);
		//TODO: Remover PCB()
		sem_post(&mutex_cola_exec);

		// Se encola el pcb del proceso en READY.
		sem_wait(&mutex_cola_ready);
		queue_push(colaReady, pcb_recibido); // La planificación del PCP es Round Robin, por lo tanto lo inserto por orden de llegada.
		sem_post(&mutex_cola_ready);

		estadisticaCambiarEstado(pcb_recibido->pid, READY);

		//Aumento el semanforo de procesos en ready
		sem_post(&sem_cola_ready);
	}
	// Se desocupa la CPU
	desocupar_cpu(socket_cpu);
}

void finalizacion_proceso(void* paquete_from_cpu, int32_t socket_cpu_asociado) {
	t_pcb* pcbRecibido = deserializar_pcb(paquete_from_cpu);
	log_debug(logger, "Finalizar proceso %d por fin de ejecucion", pcbRecibido->pid);

	pcbRecibido->exitCode = FINALIZO_BIEN;
	terminarProceso(pcbRecibido, socket_cpu_asociado);
}

t_puntero verificarEspacio(void* paquete, int32_t cant){

	uint32_t offset = 0;
	meta_bloque metadata;

	while(offset<pagina_size){
		do{
			memcpy(&metadata, paquete + offset, sizeof(meta_bloque));
			offset += sizeof(meta_bloque) + metadata.size;
		}
		while(metadata.used);

		if(metadata.size >= cant + sizeof(meta_bloque) ){
			offset -= metadata.size;
			return offset;
		}
	}
	return 0;
}

reserva_memoria* obtenerReserva(uint32_t pid, uint32_t cant){
	uint32_t ind;
	reserva_memoria* reserva;

	for(ind=0; ind<list_size(mem_dinamica); ind++){
		reserva = list_get(mem_dinamica,ind);
		if(reserva->pid == pid && reserva->size >= cant + sizeof(meta_bloque))
			return reserva;
	}
	return NULL;
}

int buscarSiguiente(t_bloque* block, t_list* list, uint32_t pagina){
	int t;
	t_bloque* bloque;
	for(t=0; t<list->elements_count; t++){
		bloque=list_get(list, t);
		if(bloque->pos / pagina_size == pagina &&
				bloque->pos == block->pos + block->size + sizeof(meta_bloque))return t;
	}
	return -1;
}

void mostrarReserva(void* paquete){
	uint32_t offset = 0;
	meta_bloque metadata;
	char* bloques = string_new();

	while(offset<pagina_size){
		memcpy(&metadata, paquete + offset, sizeof(meta_bloque));
		string_append_with_format(&bloques,offset == 0?"%d %s":" | %d %s",
				metadata.size,metadata.used?"U":"L");
		offset += sizeof(meta_bloque) + metadata.size;
	}
	log_info(logger,"%s",bloques);
}

int buscarAnterior(t_bloque* block, t_list* list, uint32_t pagina){
	int t;
	t_bloque* bloque;
	for(t=0; t<list->elements_count; t++){
		bloque=list_get(list, t);
		if(bloque->pos / pagina_size == pagina &&
				block->pos == bloque->pos + bloque->size + sizeof(meta_bloque))return t;
	}
	return -1;
}

int32_t buscarEntrada(uint32_t pid){
	t_entrada_datos* item;
	int32_t k;
	for(k=0; k<bloques->elements_count; k++){
		item = list_get(bloques,k);
		if(item->pid == pid) return k;
	}
	return -1;
}

int buscarReserva(int32_t pid, int32_t pag){
	int32_t i;
	reserva_memoria* reserva;

	for(i=0; i<list_size(mem_dinamica); i++){
		reserva = list_get(mem_dinamica, i);
		if(reserva->pid == pid && reserva->pag == pag)
			return i;
	}
	return -1;
}

bool siguienteLibre(void* paquete, int32_t posicion){
	meta_bloque metadata,siguiente;
	memcpy(&metadata, paquete + posicion, sizeof(meta_bloque));
	int offset = posicion + metadata.size + sizeof(meta_bloque);
	memcpy(&siguiente, paquete + offset, sizeof(meta_bloque));
	return !siguiente.used;
}

int posAnterior(void* paquete, int32_t posicion){
	meta_bloque metadata;
	int offset = 0, posAnterior;

	while(offset < posicion){
		posAnterior = offset;
		memcpy(&metadata, paquete + offset, sizeof(meta_bloque));
		offset += metadata.size + sizeof(meta_bloque);
	}
	if(offset == 0) return -1;
	else
		return posAnterior;
}

bool anteriorLibre(void* paquete, int32_t posicion){
	meta_bloque metadata;
	int offset = posAnterior(paquete,posicion);

	if(offset != -1){
		memcpy(&metadata, paquete + offset, sizeof(meta_bloque));
		return !metadata.used;
	}
	else return 0;
}

void reservarMemoria(int32_t socket, char* paquete){
	int offset;
	t_puntero posicion;
	header_t* header = malloc(sizeof(header_t));
	int32_t resultado;
	int32_t pid, cant;
	meta_bloque metadata;
	reserva_memoria* reserva;
	pedido_mem pedido_memoria;
	t_pedido_iniciar* pedido;
	t_pedido_bytes bytes;
	void* package;
	size_t tamano = sizeof(uint32_t)*3;
	memcpy(&pedido_memoria, paquete, tamano);
	free(paquete);

	if(pedido_memoria.cant > pagina_size - sizeof(meta_bloque)*2){
		enviar_paquete_vacio(FALLA_RESERVAR_RECURSOS,socket);
		return;
	}
	sem_wait(&mutex_dinamico);
	reserva = obtenerReserva(pid=pedido_memoria.pid,cant=pedido_memoria.cant);
	sem_post(&mutex_dinamico);

	while(reserva != NULL){

		bytes.pid = reserva->pid;
		bytes.pag = reserva->pag;
		bytes.offset = 0;
		bytes.size = pagina_size;

		if(solicitudBytes(socketConexionMemoria, &bytes, &package) != RESPUESTA_BYTES){
			log_error(logger,"Segmentation fault");
			enviar_paquete_vacio(SEGMENTATION_FAULT,socket);
			return;
		}

		offset = verificarEspacio(package,cant=pedido_memoria.cant);

		if(offset != 0){
			int posicion = reserva->pag * pagina_size + offset;
			memcpy(&metadata, package + offset - sizeof(meta_bloque), sizeof(meta_bloque));
			int tamReserva = metadata.size;

			metadata.used = true;
			metadata.size = cant;
			reserva->size -= pedido_memoria.cant+sizeof(meta_bloque);
			reserva->cant++;
			memcpy(package + offset - sizeof(meta_bloque), &metadata, sizeof(meta_bloque));

			metadata.used = false;
			metadata.size = tamReserva - cant - sizeof(meta_bloque);
			memcpy(package + offset + cant, &metadata, sizeof(meta_bloque));

			if(grabarBytes(socketConexionMemoria, &bytes, package) != OP_OK){
				log_error(logger,"Segmentation fault");
				enviar_paquete_vacio(SEGMENTATION_FAULT,socket);
				return;
			}

			header->type = RESERVAR_MEMORIA_OK;
			header->length = sizeof(t_puntero);
			sendSocket(socket, header, &posicion);

			log_debug(logger,"Proceso #%d reserva con exito posicion %d",pid,posicion);
			mostrarReserva(package);
			free(package);
			free(header);
			return;
		}
		sem_wait(&mutex_dinamico);
		reserva = obtenerReserva(pid,cant);
		sem_post(&mutex_dinamico);
	}

	package = malloc(pagina_size);

	pedido = malloc(sizeof(t_pedido_iniciar));
	pedido->pid = pedido_memoria.pid;
	pedido->cant_pag = 1;
	header = malloc(sizeof(header_t));
	header->type = ASIGNAR_PAGINAS;
	header->length = sizeof(t_pedido_iniciar);

	pthread_mutex_lock(&mutex_memoria_fd);
	sendSocket(socketConexionMemoria, header, pedido);
	recibir_paquete(socketConexionMemoria, &paquete, &resultado);
	pthread_mutex_unlock(&mutex_memoria_fd);
	free(pedido);

	if(resultado != OP_OK){
		log_error(logger,"Memoria se quedo sin espacio");
		enviar_paquete_vacio(FALLA_RESERVAR_RECURSOS,socket);
		free(header);
		return;
	}

	info_estadistica_t* info = buscarInformacion(pedido_memoria.pid);
	reserva = malloc(sizeof(reserva_memoria));
	reserva->pag = pedido_memoria.pagBase + info->cantPaginasHeap + config->stack_Size;
	reserva->size = pagina_size - sizeof(meta_bloque)*2 - pedido_memoria.cant;
	reserva->pid = pedido_memoria.pid;
	reserva->cant = 1;

	sem_wait(&mutex_dinamico);
	list_add(mem_dinamica, reserva);
	sem_post(&mutex_dinamico);

	metadata.used = true;
	metadata.size = pedido_memoria.cant;
	memcpy(package, &metadata, sizeof(meta_bloque));
	bytes.pid = pedido_memoria.pid;
	bytes.pag = reserva->pag;
	bytes.offset = 0;
	bytes.size = sizeof(meta_bloque);

	if(grabarBytes(socketConexionMemoria,&bytes,&metadata) != OP_OK){
		log_error(logger,"Segmentation fault");
		enviar_paquete_vacio(SEGMENTATION_FAULT,socket);
		free(header);
		return;
	}

	int metaSize = metadata.size;
	metadata.used = false;
	metadata.size = pagina_size - sizeof(meta_bloque)*2 - pedido_memoria.cant;
	memcpy(package + metaSize + sizeof(meta_bloque), &metadata, sizeof(meta_bloque));
	bytes.pid = pedido_memoria.pid;
	bytes.pag = reserva->pag;
	bytes.offset = sizeof(meta_bloque)+pedido_memoria.cant;
	bytes.size = sizeof(meta_bloque);

	if(grabarBytes(socketConexionMemoria,&bytes,&metadata) != OP_OK){
		log_error(logger,"Segmentation fault");
		enviar_paquete_vacio(SEGMENTATION_FAULT,socket);
		return;
	}

	header->type = RESERVAR_MEMORIA_OK;
	header->length = sizeof(uint32_t);
	posicion = bytes.pag * pagina_size + sizeof(meta_bloque);
	sendSocket(socket, header, &posicion);
	free(header);

	estadisticaAlocarBytes(pedido_memoria.pid, pedido_memoria.cant);
	aumentarEstadisticaPorSocketAsociado(socket, estadisticaAumentarAlocar);
	aumentarEstadisticaPorSocketAsociado(socket, estadisticaAumentarOpPriviligiada);
	info->cantPaginasHeap++;

	log_debug(logger, "Proceso #%d reserva con exito posicion %d",
			pedido_memoria.pid,posicion);
	mostrarReserva(package);
	free(package);
}

void liberarMemoria(int32_t socket, char* paquete){
	t_pedido_bytes pedido;
	header_t header;
	int32_t tipo;
	int32_t pid, posicion;
	meta_bloque metadata;
	int32_t tamano = sizeof(uint32_t);
	reserva_memoria* reserva;

	int posReserva;
	memcpy(&pid, paquete, tamano);
	memcpy(&posicion, paquete+tamano, tamano);
	free(paquete);

	sem_wait(&mutex_dinamico);
	posReserva = buscarReserva(pid, posicion / pagina_size);
	sem_post(&mutex_dinamico);

	if(posReserva == -1){
		log_error(logger,"No se puede liberar la memoria");
		enviar_paquete_vacio(NULL_POINTER,socket);
		return;
	}
	sem_wait(&mutex_dinamico);
	reserva = list_get(mem_dinamica, posReserva);
	sem_post(&mutex_dinamico);

	if(reserva->cant > 0){
		pedido.pid = pid;
		pedido.pag = posicion/pagina_size;
		pedido.offset = 0;
		pedido.size = pagina_size;

		if(solicitudBytes(socketConexionMemoria, &pedido ,&paquete) != RESPUESTA_BYTES){
			log_error(logger,"Segmentation fault");
			enviar_paquete_vacio(SEGMENTATION_FAULT,socket);
			return;
		}

		int offset = posicion % pagina_size - sizeof(meta_bloque);
		memcpy(&metadata, paquete + offset, sizeof(meta_bloque));

		if(metadata.used == false){
			log_error(logger,"El puntero no esta apuntando a memoria valida");
			enviar_paquete_vacio(NULL_POINTER,socket);
			return;
		}

		metadata.used = false;
		int bytesLiberados = metadata.size;

		memcpy(paquete + offset, &metadata, sizeof(meta_bloque));
		memset(paquete + posicion % pagina_size, '\0', metadata.size);
		reserva->size += metadata.size;
		reserva->cant--;

		if(reserva->cant == 0){
			sem_wait(&mutex_dinamico);
			free(list_remove(mem_dinamica,posReserva));
			sem_post(&mutex_dinamico);

			header.type = LIBERAR_PAGINA;
			header.length = sizeof(t_pedido_iniciar);

			t_pedido_iniciar liberar;
			liberar.pid = pid;
			liberar.cant_pag = reserva->pag;

			pthread_mutex_lock(&mutex_memoria_fd);
			sendSocket(socketConexionMemoria, &header, &liberar);
			recibir_paquete(socketConexionMemoria, &paquete, &tipo);
			pthread_mutex_unlock(&mutex_memoria_fd);

			if(tipo != OP_OK){
				log_error(logger,"Segmentation fault");
				enviar_paquete_vacio(SEGMENTATION_FAULT,socket);
				return;
			}
		}
		else{
			int offsetEscritura = offset;
			if(anteriorLibre(paquete, offset)){

				int tamano = sizeof(meta_bloque);
				int offPrevio = posAnterior(paquete, offset);
				meta_bloque metadata;

				memcpy(&metadata, paquete + offset, sizeof(meta_bloque));
				tamano += metadata.size;
				memcpy(&metadata, paquete + offPrevio, sizeof(meta_bloque));
				tamano += metadata.size;

				metadata.used = false;
				metadata.size = tamano;

				memcpy(paquete + offPrevio, &metadata, sizeof(meta_bloque));
				reserva->size += sizeof(meta_bloque);
				offsetEscritura = offPrevio;
			}

			if(siguienteLibre(paquete, offsetEscritura)){
				int tamano = sizeof(meta_bloque);
				meta_bloque metadata;

				memcpy(&metadata, paquete + offsetEscritura, sizeof(meta_bloque));
				tamano += metadata.size;
				int offset = offsetEscritura + sizeof(meta_bloque) + metadata.size;
				memcpy(&metadata, paquete + offset, sizeof(meta_bloque));
				tamano += metadata.size;

				metadata.used = false;
				metadata.size = tamano;
				memcpy(paquete + offsetEscritura, &metadata, sizeof(meta_bloque));
				reserva->size += sizeof(meta_bloque);
			}
			pedido.pid = pid;
			pedido.pag = posicion / pagina_size;
			pedido.offset = 0;
			pedido.size = pagina_size;

			if(grabarBytes(socketConexionMemoria, &pedido, paquete) != OP_OK){
				log_error(logger,"Segmentation fault");
				enviar_paquete_vacio(SEGMENTATION_FAULT,socket);
				return;
			}
			mostrarReserva(paquete);
		}
		aumentarEstadisticaPorSocketAsociado(socket, estadisticaAumentarOpPriviligiada);
		aumentarEstadisticaPorSocketAsociado(socket, estadisticaAumentarLiberar);
		estadisticaLiberarBytes(pid, bytesLiberados);

		log_info(logger,"Proceso #%d libera con exito posicion: %d", pid, posicion);
		enviar_paquete_vacio(LIBERAR_MEMORIA_OK, socket);
		return;
	}
}

void desocupar_cpu(int32_t socket_asociado) {

	sem_wait(&mutex_lista_CPUs);
	cpu_t *cpu = obtener_cpu_por_socket_asociado(socket_asociado);
	if(cpu != NULL){
			log_info(logger, "Se desocupa cpu %d\n", socket_asociado);
			cpu->disponible = true;
			sem_post(&semCPUs_disponibles); // Aumento el semaforo contador de cpus disponibles.
	}
	sem_post(&mutex_lista_CPUs);
}

/*
 * @NAME: obtener_cpu_por_socket_asociado
 * @DESC: Devuelve un puntero al cpu asociado a soc_asociado, si no lo encuentra devuelve NULL.
 */
cpu_t *obtener_cpu_por_socket_asociado(int32_t soc_asociado){

	cpu_t *cpu_asociado = NULL;

	t_link_element *nodo = listaCPUs->head;

	while(nodo != NULL && cpu_asociado == NULL){
		cpu_t *cpu_aux = (cpu_t*) nodo->data;
		if(cpu_aux->socket == soc_asociado) {
			cpu_asociado = cpu_aux;
		} else {
			nodo = nodo->next;
		}
	}
	return cpu_asociado;
}

void abrirArchivo(int32_t socketCpu, void* package){
	uint32_t sizeDireccion,pid;
	memcpy(&pid,package,sizeof(uint32_t));
	memcpy(&sizeDireccion,package + sizeof(uint32_t),sizeof(uint32_t));
	char* direccion = package + (sizeof(uint32_t) * 2);
	t_banderas* banderas = package + sizeof(uint32_t) * 2  + sizeDireccion;

	char* permisos = string_new();
	if(banderas->creacion) string_append(&permisos, "C");
	if(banderas->escritura) string_append(&permisos, "E");
	if(banderas->lectura) string_append(&permisos, "L");

	int fd = agregarArchivo_aProceso(pid, direccion, permisos);
	header_t header;
	int tipo;
	void* paquete;

	if(!banderas->creacion){

		header.type = VALIDAR_ARCHIVO;
		header.length = strlen(direccion) + 1;

		sem_wait(&mutex_fs);
		sendSocket(socketConexionFS, &header, direccion);
		recibir_paquete(socketConexionFS, &paquete, &tipo);
		sem_post(&mutex_fs);

		if(tipo == ARCHIVO_EXISTE){
			header.type = ABRIR_ARCHIVO_OK;
			header.length = sizeof(int);
			sendSocket(socketCpu, &header, &fd);
			return;
		}else{
			enviar_paquete_vacio(ARCHIVO_INEXISTENTE, socketCpu);
		}
	}


	//mando mensaje a fs

	header.length = string_length(direccion)+1;
	header.type = CREAR_ARCHIVO;

	sem_wait(&mutex_fs);

	sendSocket(socketConexionFS, &header, direccion);

	recibir_paquete(socketConexionFS, &paquete, &tipo);

	sem_post(&mutex_fs);

	if(tipo == ABRIR_ARCHIVO_OK){
		header.type = ABRIR_ARCHIVO_OK;
		header.length = sizeof(int);
		sendSocket(socketCpu, &header, &fd);
	}
	else
		enviar_paquete_vacio(tipo, socketCpu);
}

void borrarArchivo(int32_t socketCpu, void* package){
	int pid = *(int*) package;
	int fd = *(int*) (package + sizeof(int));

	t_archivo* archivo = buscarArchivo(pid, fd);
	char* path = buscarPathDeArchivo(archivo->globalFD);

	header_t header;
	header.type = BORRAR_ARCHIVO;
	uint32_t size = strlen(path) + 1;
	header.length = size;

	if(!archivoPuedeSerBorrado(archivo->globalFD)){
		enviar_paquete_vacio(IMPOSIBLE_BORRAR_ARCHIVO, socketCpu);
		return;
	}

	sem_wait(&mutex_fs);

	sendSocket(socketConexionFS, &header, path);

	int32_t tipo;
	void* paquete;
	recibir_paquete(socketConexionFS, &paquete, &tipo);

	sem_post(&mutex_fs);

	int32_t respuesta;
	if(tipo == BORRAR_ARCHIVO_OK) respuesta = BORRAR_ARCHIVO_OK;
	else respuesta = ARCHIVO_INEXISTENTE;

	enviar_paquete_vacio(respuesta, socketCpu);

}

void cerrarArchivo(int32_t socketCpu, void* package){
	int pid = *(int*) package;
	int fd = *(int*) (package + sizeof(int));
	eliminarFd(fd, pid);
	enviar_paquete_vacio(CERRAR_ARCHIVO_OK, socketCpu);
}

void escribir(void* paquete, int32_t socketCpu){
	uint32_t fd = *(uint32_t*) paquete;
	int pid = *(int*) (paquete + sizeof(uint32_t));
	uint32_t sizeEscritura = *(uint32_t*) (paquete + sizeof(uint32_t) + sizeof(int));
	void* escritura = paquete + sizeof(int)  + sizeof(uint32_t) * 2;
	info_estadistica_t * info = buscarInformacion(pid);
	header_t header;

	if(fd == 1){
		int32_t sizePedido = sizeof(int) + strlen(escritura) + 1;

		header.type=IMPRIMIR_POR_PANTALLA;
		header.length= sizePedido;

		char* buffer = malloc(sizePedido);
		memcpy(buffer, &pid, sizeof(int));
		memcpy(buffer+sizeof(int), escritura, strlen(escritura) + 1);

		sendSocket(info->socketConsola, &header, buffer);
		enviar_paquete_vacio(ESCRITURA_OK, socketCpu);
	}else{
		t_archivo* archivo = buscarArchivo(pid, fd);
		if(archivo == NULL){
			log_error(logger, "No se encontro el archivo para escribir");
			enviar_paquete_vacio(ARCHIVO_INEXISTENTE, socketCpu);
			return;
		}
		int32_t offsetEscritura = archivo->cursor;
		char* path = buscarPathDeArchivo(fd);
		uint32_t sizeTotal = 3 * sizeof(int) + strlen(path) + 1 + sizeEscritura;
		void * buffer = malloc(sizeTotal);
		int32_t offset = 0;
		int sizePath = strlen(path) + 1;


		memcpy(buffer, &offsetEscritura, sizeof(int));
		offset += sizeof(int);
		memcpy(buffer+offset, &sizeEscritura, sizeof(int));
		offset += sizeof(int);
		memcpy(buffer+offset, &sizePath, sizeof(int));
		offset += sizeof(int);
		memcpy(buffer+offset, path, sizePath);
		offset += sizePath;
		memcpy(buffer+offset, escritura, sizeEscritura);

		header.length = sizeTotal;
		header.type = GUARDAR_DATOS;

		sem_wait(&mutex_fs);

		sendSocket(socketConexionFS, &header, buffer);

		free(buffer);

		void* paquete;
		int32_t tipo;

		recibir_paquete(socketConexionFS, &paquete, &tipo);

		sem_post(&mutex_fs);

		if(tipo == ESCRITURA_OK){
			enviar_paquete_vacio(ESCRITURA_OK, socketCpu);
		}else{
			enviar_paquete_vacio(tipo, socketCpu);
		}

	}
}

void leerArchivo(int socketCpu, t_lectura* lectura){
	t_archivo* archivo = buscarArchivo(lectura->pid, lectura->descriptor);
	if(archivo == NULL){
		log_error(logger, "No se encontro el archivo");
		enviar_paquete_vacio(ARCHIVO_INEXISTENTE, socketCpu);
		return;
	}
	int offsetPedidoLectura = archivo->cursor;
	char* path = buscarPathDeArchivo(lectura->descriptor);
	uint32_t sizePath = strlen(path) + 1;
	uint32_t sizeTotal = sizeof(uint32_t) * 3 + sizePath;
	void* buffer = malloc(sizeTotal);

	uint32_t offset = 0;
	memcpy(buffer, &offsetPedidoLectura, sizeof(uint32_t));
	offset += sizeof(uint32_t);
	memcpy(buffer+offset, &(lectura->size), sizeof(uint32_t));
	offset += sizeof(uint32_t);
	memcpy(buffer+offset, &sizePath, sizeof(uint32_t));
	offset += sizeof(uint32_t);
	memcpy(buffer+offset, path, sizePath);

	header_t header;
	header.length = sizeTotal;
	header.type = OBTENER_DATOS;

	sem_wait(&mutex_fs);

	sendSocket(socketConexionFS, &header, buffer);

	void* paquete;
	int32_t tipo;

	recibir_paquete(socketConexionFS, &paquete, &tipo);

	sem_post(&mutex_fs);

	if(tipo == LECTURA_OK){
		header.length = lectura->size;
		header.type = LECTURA_OK;
		sendSocket(socketCpu, &header, paquete);
	}else{
		log_error(logger, "File system no pudo leer el archivo");
		enviar_paquete_vacio(ARCHIVO_INEXISTENTE, socketCpu);
	}

}

void moverCursor(int32_t socketCPU, t_cursor* cursor){ // TODO con esto alcanza?
	t_archivo* archivo = buscarArchivo(cursor->pid, cursor->descriptor);
	if(archivo == NULL){
		log_error(logger, "No se encontro el archivo para escribir");
		enviar_paquete_vacio(ARCHIVO_INEXISTENTE, socketCPU);
		return;
	}

	archivo->cursor = cursor->posicion;
	enviar_paquete_vacio(MOVER_CURSOR_OK, socketCPU);
}


info_estadistica_t* buscarInformacionPorSocketConsola(int32_t socketConsola){

	bool buscar(info_estadistica_t* info){
		return info->socketConsola == socketConsola ? true : false;//&& (info->estado != FINISH) ? true : false;
	}

	return list_find(listadoEstadistico, buscar);
}

void quitarDeMemoriaDinamica(int pid){
	int i;
	for(i=0; i<list_size(mem_dinamica); i++){
		sem_wait(&mutex_dinamico);
		reserva_memoria* reserva = list_get(mem_dinamica, i);
		if(reserva->pid == pid){
			list_remove(mem_dinamica, i);
			free(reserva);
		}
		sem_post(&mutex_dinamico);
	}
}
