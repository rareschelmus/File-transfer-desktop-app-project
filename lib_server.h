#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/tcp.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sqlite3.h>

/* portul folosit */
#define PORT 2124
#define TRACKER_PORT 8079
#define DATA_SIZE 60000
#define NAME_SIZE 200
#define COMMAND_SIZE 10
#define CODE_SIZE 100
#define N_STREAMS 15
#define ON 1
#define OFF 0
#define N_PEERS 15

/**
 structura ce salveaza :
 * pid-ul copilui ce face streamul de fisiere
 * id-ul pe care i-l da parintele
 * portul la care face stream
 * daca e folosit
 * numele fisierului
 * codul generat 
 */

struct c_stream {
	pid_t pid;
	int id;
	int port;
	bool in_use;
	char file_name[NAME_SIZE];
	char code[CODE_SIZE];
};


extern int errno;



void set_server(struct sockaddr_in *server,struct sockaddr_in *from,int port);///<setam socketul pentru server

int send_name_and_size(int socket_d, int client, char *file, long long size);///<trimitem la peer numele si dimensiunea fisierului

long long get_size(char *file);///<obtinem dimensiunea fisierului

int open_file(int *fin, char *file);///<deschidem fisierul

int send_file(int socket_d,int client,char *file);///<trimitem fisierul

int stream_server(int child_stream);///< server-ul stream-ului

int new_stream();///<cream un stream de fisiere nou

int view_streams();///<afiseaza stream-urile active

int kill_stream(int stream_id);///<inchidem un stream activ

int insert_stream(pid_t pid, char file_name[NAME_SIZE]);///<inseram un stream nou in vectorul de stream-uri

void delete_pid(pid_t p);///<stergem pid-ul procesului copil ce a terminat de trimis un fisier 
