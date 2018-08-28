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
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sqlite3.h>

#define PORT 2124 ///<portul folosit la upload
#define DATA_SIZE 60000
#define NAME_LENGTH 100
#define BAR_SIZE 110
#define WORD_LENGTH 50
#define LOCATION "downloads/" ///locatia by default
#define LOCATION_SIZE 1000
#define PATH_SIZE 300


int set_socket(int *socket_d,struct sockaddr_in *server,char *port,char *address);///<pregatim socketul pentru comunicare

long long array_to_lli(char *size_in_char);///<convertim un char * in long long 

void dissect_package(char *package,char *name,long long *size);///<extragem din pachetul primit numele si dimensiunea fisierului

int get_name_and_size(int socket_d,char *name,long long *size);///<citim de la peer pachetul ce contine numele si dimensiunea fisierului

int create_file(int *fout,char *name);///<cream fisierul pe care il va descarca de la peer

int receive_file(int socket_d, struct sockaddr_in server);///<descarcam fisierul de la peer

int client_p();///<initiem secventa de download

int decode(char word1[],char word2[],char word3[]);///<functia ce decodifica codul citit

int get_code(char code[],char address[],char port[]);///<citim codul pentru download

int change_location();///<schimbam locatia de download

void print_progress(long long downloaded,long long target,char name[]);///<Afisam progresul download-ului

static int callback(void *data, int argc, char **argv, char **ColName);///<functia ce citeste rezultatul interogarii din baza de date
///extragem numele din pathway-ul primit
void get_name_from_path(char *path);
