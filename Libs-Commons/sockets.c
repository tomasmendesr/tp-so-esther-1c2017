/*
 * sockets.c
 *
 *  Created on: 28/04/2014
 *      Author: utnso
 */

#include "sockets.h"

/**
 * @NAME: getSocket
 * @DESC: Crea un socket y retorna su descriptor. Retorna -1 en caso de error.
 */
int getSocket(void) {
	int yes = 1;
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		close(sockfd);
		return -1;
	}

	return sockfd;
}

/**
 * @NAME: bindSocket
 * @DESC: Bindea el socket a la dirección ip y puerto pasados por parámetro.
 * Retorna -1 en caso de error.
 */
int bindSocket(int sockfd, char *addr, char *port) {
	struct sockaddr_in my_addr;

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(atoi(port));
	my_addr.sin_addr.s_addr = inet_addr(addr);
	memset(&(my_addr.sin_zero), '\0', 8);

	return bind(sockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr));
}

/**
 * @NAME: listenSocket
 * @DESC: Pone a la escucha el socket pasado por parámetro con el backlog pasado por parámetro.
 * Retorna -1 en caso de error.
 */
int listenSocket(int sockfd, int backlog) {
	return listen(sockfd, backlog);
}

/**
 * @NAME: acceptSocket
 * @DESC: Acepta una conexion al socket enviado por parámetro y retorna el nuevo descriptor de socket.
 * Retorna -1 en caso de error.
 */
int acceptSocket(int sockfd) {
	struct sockaddr_in their_addr;
	int sin_size = sizeof(struct sockaddr_in);

	return accept(sockfd, (struct sockaddr *) &their_addr,
			(socklen_t *) &sin_size);
}

/**
 * @NAME: connectSocket
 * @DESC: Conecta el socket a la dirección y puerto pasados por parámetro.
 * Retorna -1 en caso de error.
 */
int connectSocket(int sockfd, char *addr, char *port) {
	struct sockaddr_in their_addr;

	their_addr.sin_family = AF_INET;
	their_addr.sin_port = htons(atoi(port));
	their_addr.sin_addr.s_addr = inet_addr(addr);
	memset(&(their_addr.sin_zero), '\0', 8);

	return connect(sockfd, (struct sockaddr*) &their_addr,
			sizeof(struct sockaddr));
}

/**
 * @NAME: sendSocket
 * @DESC: Serializa el header y el string de datos y lo envía al scoket pasado por parámetro.
 * Retorna la cantidad de bytes enviados o -1 en caso de error.
 */
int sendSocket(int sockfd, header_t *header, void *data) {
	int bytesEnviados, offset = 0, tmp_len = 0;
	void *packet = malloc(sizeof(header_t) + header->length);

	memcpy(packet, &header->type, tmp_len = sizeof(int8_t));
	offset = tmp_len;

	memcpy(packet + offset, &header->length, tmp_len = sizeof(header->length));
	offset += tmp_len;

	memcpy(packet + offset, data, tmp_len = header->length);
	offset += tmp_len;

	bytesEnviados = sendallSocket(sockfd, packet, offset);//El offset representa la cantidad total de bytes del paquete a enviar
	free(packet);

	return bytesEnviados;
}

/**
 * @NAME: createServer
 * @DESC: Crea y devuelve un socket para ser utilizado como servidor.
 * En caso de error retorna -1.
 */
int createServer(char *addr, char *port, int backlog) {
	int sockfd = getSocket();

	if (bindSocket(sockfd, addr, port) == -1) {
		perror("bind");
		close(sockfd);
		return -1;
	}

	if (listenSocket(sockfd, backlog) == -1) {
		perror("listen");
		close(sockfd);
		return -1;
	}

	return sockfd;
}

int createServer2(char *addr, char *port, int backlog) {

	struct addrinfo hints;
	struct addrinfo *serverInfo;

	memset(&hints,0,sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;

	getaddrinfo(NULL,port,&hints,&serverInfo);

	int sockfd = socket(serverInfo->ai_family,serverInfo->ai_socktype,serverInfo->ai_protocol);

	//int sockfd = getSocket();

	if (bindSocket(sockfd, addr, port) == -1) {
		perror("bind");
		close(sockfd);
		return -1;
	}

	if (listenSocket(sockfd, backlog) == -1) {
		perror("listen");
		close(sockfd);
		return -1;
	}

	return sockfd;
}

/**
 * @NAME: createClient
 * @DESC: Crea y devuelve un socket para ser utilizado como cliente.
 * En caso de error retorna -1.
 */
int createClient(char *addr, char *port) {
	int sockfd = getSocket();

	if (connectSocket(sockfd, addr, port) == -1) {
		perror("connect");
		close(sockfd);
		return -1;
	}

	return sockfd;
}

/**
 * @NAME: sendallSocket
 * @DESC: procura enviar todo el paquete entero. Devuelve la cantidad de bytes enviados o -1 en caso de error.
 *
 */
int sendallSocket(int socket, void* paquete_a_enviar, int tamanio_paquete) {

	int total = 0;
	int bytesleft = tamanio_paquete;
	int n;

	while(total < tamanio_paquete) {
		n = send(socket, paquete_a_enviar+total, bytesleft, 0);
		if(n == -1) {break; }
		total += n;
		bytesleft -= n;
	}

	return n==-1?-1:total;

}

int enviarHandshake(int sockfd, int headerType) {
	header_t header;
	header.type = headerType;
	header.length = 0;

	return sendSocket(sockfd, &header, '\0');
}

char *program_serializer(char *codigo_programa) {

	uint16_t longitud;
	longitud = strlen(codigo_programa);
	char *programa_ya_serializado = malloc(longitud + sizeof(longitud));

	memcpy(programa_ya_serializado, &longitud, sizeof(longitud));
	memcpy(programa_ya_serializado + sizeof(longitud), codigo_programa, longitud);

	return programa_ya_serializado;

}

/*
 * @NAME: recibir_paquete
 *
 * @DESC: Recibe un paquete por el socket que le llega por parametro, devuelve la cantidad de bytes recibidos, 0 si se cerro el socket o -1 si hubo algun error.
 *
 */
int recibir_paquete(int socket, void** paquete, int* tipo){

	int bytes_recibidos;
	void* buffer;
	header_t cabecera;

	bytes_recibidos = recv(socket, &cabecera, sizeof(header_t), MSG_WAITALL);

	if(bytes_recibidos == 0) return 0;
	if(bytes_recibidos == -1) return -1;

	*tipo = cabecera.type;
	buffer = malloc(cabecera.length);

	bytes_recibidos = recv(socket, buffer, cabecera.length, MSG_WAITALL);

	if(bytes_recibidos == 0) return 0;
	if(bytes_recibidos == -1) return -1;

	*paquete = buffer;

	return bytes_recibidos;

}

/*
 * @NAME: recibir_string
 *
 * @DESC: Recibe un paquete que contiene un string, utiliza la funcion recibir_paquete y le agrega el caracter nulo (\0) al final.
 */
int recibir_string(int socket,void** puntero_buffer,int* tipo){
	int tamanio;

	if((tamanio=recibir_paquete(socket,puntero_buffer,tipo))>0){
		agregar_caracter_nulo(*puntero_buffer, tamanio);
		return tamanio+1;
	}
	return tamanio;
}

/*
 * @NAME: agregar_caracter_nulo
 *
 * @DESC: Agrega el caracter nulo (\0) al final de un stream de bytes recibido.
 */
int agregar_caracter_nulo(void* stream, int tamanio){

	void* otro_puntero;

	otro_puntero=stream;
	otro_puntero=realloc(otro_puntero,tamanio+1);
	char aux='\0';
	memcpy(otro_puntero+tamanio,&aux,1);
	stream=otro_puntero;

	return tamanio+1;

}

/*
 * @NAME: deserializar_string
 *
 * @DESC: Recibe un puntero a un stream de bytes, deserializa un string y devuelve la cantidad total de bytes leidos del stream.
 */
int deserializar_string(void* paquete, char** string){

	int bytes_totales_leidos = 0;
	int bytes_string;
	void* puntero_buffer = paquete;

	memcpy(&bytes_string, puntero_buffer, sizeof(int));

	puntero_buffer += sizeof(int);

	string = malloc(bytes_string);

	memcpy(*string, puntero_buffer, bytes_string);

	agregar_caracter_nulo(*string, bytes_string);

	bytes_totales_leidos = sizeof(int) + bytes_string;

	return bytes_totales_leidos;

}

void* pcb_serializer(pcb* self, int16_t *length){

	void *serialized = malloc(sizeof(pcb));
	int offset = 0, tmp_size = 0;

	memcpy(serialized, &self->id, tmp_size = sizeof(self->id));
	offset += tmp_size;

	memcpy(serialized + offset, &self->codePointer, tmp_size = sizeof(self->codePointer));
	offset += tmp_size;

	memcpy(serialized+ offset, &self->stackPointer, tmp_size = sizeof(self->stackPointer));
	offset += tmp_size;

	memcpy(serialized+ offset, &self->stackContextPointer, tmp_size = sizeof(self->stackContextPointer));
	offset += tmp_size;

	memcpy(serialized+ offset, &self->indexCodePointer, tmp_size = sizeof(self->indexCodePointer));
	offset += tmp_size;

	memcpy(serialized+ offset, &self->labelIndexPointer, tmp_size = sizeof(self->labelIndexPointer));
	offset += tmp_size;

	memcpy(serialized+ offset, &self->programCounter, tmp_size = sizeof(self->programCounter));
	offset += tmp_size;

	memcpy(serialized+ offset, &self->tamanioContexto, tmp_size = sizeof(self->tamanioContexto));
	offset += tmp_size;

	memcpy(serialized+ offset, &self->tamanioIndiceEtiquetas, tmp_size = sizeof(self->tamanioIndiceEtiquetas));
	offset += tmp_size;

	*length = offset;

	return serialized;
}

pcb* pcb_deserializer(int socketfd) {
	pcb* self = malloc(sizeof(pcb));
	recv(socketfd, &self->id, sizeof(self->id), MSG_WAITALL);
	recv(socketfd, &self->codePointer, sizeof(self->codePointer), MSG_WAITALL);
	recv(socketfd, &self->stackPointer, sizeof(self->stackPointer), MSG_WAITALL);
	recv(socketfd, &self->stackContextPointer, sizeof(self->stackContextPointer), MSG_WAITALL);
	recv(socketfd, &self->indexCodePointer, sizeof(self->indexCodePointer), MSG_WAITALL);
	recv(socketfd, &self->labelIndexPointer, sizeof(self->labelIndexPointer), MSG_WAITALL);
	recv(socketfd, &self->programCounter, sizeof(self->programCounter), MSG_WAITALL);
	recv(socketfd, &self->tamanioContexto, sizeof(self->tamanioContexto), MSG_WAITALL);
	recv(socketfd, &self->tamanioIndiceEtiquetas, sizeof(self->tamanioIndiceEtiquetas), MSG_WAITALL);
	return self;
}

void* variable_serializer(t_variable *self, int16_t *length){

	void *serialized = malloc(sizeof(char) + sizeof(u_int32_t));
	int offset = 0, tmp_size = 0;

	memcpy(serialized, &self->nombre, tmp_size = sizeof(self->nombre));
	offset = tmp_size;

	memcpy(serialized + offset, &self->valor, tmp_size = sizeof(self->valor));
	offset += tmp_size;

	*length = offset;

	return serialized;
}

t_variable* variable_deserializer(int socketfd){
	t_variable* self = malloc(sizeof(t_variable));
	recv(socketfd, &self->nombre, sizeof(self->nombre), MSG_WAITALL);
	recv(socketfd, &self->valor, sizeof(self->valor), MSG_WAITALL);
	return self;
}

void* codeIndex_serializer(codeIndex *self, int16_t *length){

	void *serialized = malloc(sizeof(codeIndex));
	int offset = 0, tmp_size = 0;

	memcpy(serialized, &self->tamanio, tmp_size = sizeof(self->tamanio));
	offset = tmp_size;

	memcpy(serialized + offset, &self->offset, tmp_size = sizeof(self->offset));
	offset += tmp_size;

	*length = offset;

	return serialized;
}

codeIndex* codeIndex_deserializer(int socketfd){
	codeIndex* self = malloc(sizeof(codeIndex));
	recv(socketfd, &self->offset, sizeof(self->offset), MSG_WAITALL);
	recv(socketfd, &self->tamanio, sizeof(self->tamanio), MSG_WAITALL);
	return self;
}

void* requestUMV_serializer(t_request_umv* self,int16_t * length){
	void *serialized = malloc(12); //TODO: ¿Harcodeado?
	int offset = 0, tmp_size = 0;

	memcpy(serialized, &self->base, tmp_size = sizeof(self->base));
	offset = tmp_size;

	memcpy(serialized + offset, &self->offset, tmp_size = sizeof(self->offset));
	offset += tmp_size;

	memcpy(serialized + offset, &self->tamanio, tmp_size = sizeof(self->tamanio));
		offset += tmp_size;

	*length = offset;

	return serialized;
}

t_request_umv* requestUMV_deserializer(int socketfd){
	t_request_umv* self = malloc(sizeof(t_request_umv));
	recv(socketfd, &self->base, sizeof(self->base), MSG_WAITALL);
	recv(socketfd, &self->offset, sizeof(self->offset), MSG_WAITALL);
	recv(socketfd, &self->tamanio, sizeof(self->tamanio), MSG_WAITALL);
	return self;
}

void* envioUMV_serializer(t_envio_umv* self,int16_t * length){

	void *serialized = malloc(sizeof(t_request_umv) + self->tamanio);
	int offset = 0, tmp_size = 0;

	memcpy(serialized, &(self->base), tmp_size = sizeof(self->base));
	offset = tmp_size;

	memcpy(serialized + offset, &(self->offset), tmp_size = sizeof(self->offset));
	offset += tmp_size;

	memcpy(serialized + offset, &(self->tamanio), tmp_size = sizeof(self->tamanio));
		offset += tmp_size;

	memcpy(serialized + offset, self->buffer, tmp_size = self->tamanio);
		offset += tmp_size;
	*length = offset;

	return serialized;
}

t_envio_umv* envioUMV_deserializer(int socketfd){
	t_envio_umv* self = malloc(sizeof(t_envio_umv));
	recv(socketfd, &self->base, sizeof(self->base), MSG_WAITALL);
	recv(socketfd, &self->offset, sizeof(self->offset), MSG_WAITALL);
	recv(socketfd, &self->tamanio, sizeof(self->tamanio), MSG_WAITALL);
	self->buffer = malloc(self->tamanio);
	recv(socketfd, self->buffer, self->tamanio, MSG_WAITALL);
	return self;

}

char *paqueteEnviarAEjecutar_serializer(u_int16_t quantum, uint32_t retardo_quantum,pcb *pcb_proceso) {

	char *serialized = malloc(sizeof(quantum) + sizeof(retardo_quantum) + sizeof(pcb));
	int offset = 0, tmp_size = 0;

	memcpy(serialized, &quantum, tmp_size = sizeof(quantum));
	offset = tmp_size;

	memcpy(serialized + offset, &retardo_quantum, tmp_size = sizeof(retardo_quantum));
	offset += tmp_size;

	memcpy(serialized + offset, pcb_proceso, tmp_size = sizeof(pcb));
	offset += tmp_size;

	return serialized;

}

t_segmento* segmento_deserializer(int socketfd) {
	t_segmento* self = malloc(sizeof(t_segmento));
	recv(socketfd, &self->id, sizeof(self->id), MSG_WAITALL);
	recv(socketfd, &self->tamanio, sizeof(self->tamanio), MSG_WAITALL);
	return self;
}

void* segmento_serializer(t_segmento *self, int16_t *length){

	void *serialized = malloc(sizeof(char) + sizeof(u_int32_t));
	int offset = 0, tmp_size = 0;

	memcpy(serialized, &self->id, tmp_size = sizeof(self->id));
	offset = tmp_size;

	memcpy(serialized + offset, &self->tamanio, tmp_size = sizeof(self->tamanio));
	offset += tmp_size;

	*length = offset;

	return serialized;
}

header_t crear_cabecera(int codigo, int length){

	header_t cabecera;

	cabecera.type = codigo;
	cabecera.length = length;

	return cabecera;

}

int enviar_paquete_vacio(int codigo_operacion, int socket){

	header_t cabecera = crear_cabecera(codigo_operacion, 1);
	int bytes_enviados = sendSocket(socket, &cabecera, "0");

	return bytes_enviados;
}

int enviar_paquete_vacio_a_cpu(int codigo_operacion, int socket){

	int bytes_enviados;
	header_t cabecera;
	cabecera.type = codigo_operacion;
	cabecera.length = 0;

	bytes_enviados = sendSocket(socket, &cabecera, '\0');

	return bytes_enviados;

}