#include "lib_client.h"

char temp[50];
char location[LOCATION_SIZE]=LOCATION; ///< locatia de download
char pause_c[10];

///schimbam locatia de download
int change_location()
{
	char text[LOCATION_SIZE];
	struct stat path_stat;
	int len;

	printf("\033[2J\033[1;1H");
	printf("Insert location: ");
	fflush(stdout);

	///citim locatia
	if((len=read(0,text,LOCATION_SIZE))<=0)
	{
		perror("Eroare la citirea locatiei.\n");
		read(0,pause_c,10);
		printf("\033[2J\033[1;1H");
		return errno;
	}

	text[len-1]='\0';

	///Verificam daca locatia citita este director
    stat(text, &path_stat);
    if(S_ISDIR(path_stat.st_mode))
   	{
   		strcpy(location,text);
   		strcat(location,"/");
   		printf("Directory %s is set as download location.\n",location);
   		read(0,pause_c,10);
		printf("\033[2J\033[1;1H");
   	}
   	else
   	{
   		printf("The directory doesn't exist.\n");
   		read(0,pause_c,10);
		printf("\033[2J\033[1;1H");
   	}

	return 1;
}

///convertim un char * in long long 
long long array_to_lli(char *size_in_char)
{
	long long size=0,rank=10;
	int i,length;

	length=strlen(size_in_char);

	for(i=0;i<length;i++)
		size=size*rank+(size_in_char[i]-'0');

	return size;
}

/// extragem din pachetul primit numele si dimensiunea fisierului
void dissect_package(char *package,char *name,long long *size)
{
	char *size_in_char;

	char* tmp=strtok(package," ");
	strcpy(name, tmp);
	size_in_char=strtok(NULL," ");

	(*size)=array_to_lli(size_in_char);///<facem conversia din char* in long long
}


///citim de la peer pachetul ce contine numele si dimensiunea fisierului
int get_name_and_size(int socket_d,char *name,long long *size)
{

	char package[DATA_SIZE],filename[NAME_LENGTH];	

	///citim din socket pachetul
	if(read(socket_d,package,DATA_SIZE)<=0)
	{
		perror("[client] Eroare la citirea numelui.\n");
		return errno;
	}
	
	dissect_package(package,name,size);
	return 1;
}

///extragem numele din pathway-ul primit
void get_name_from_path(char *path)
{
	char *tmp,name[100];

	tmp=strtok(path,"/");
	while(tmp)
	{
		strcpy(name,tmp);
		tmp=strtok(NULL,"/");
	}
	strcpy(path,name);

}
///cream fisierul pe care il va descarca de la peer
int create_file(int *fout,char *name)
{
	char pathway[PATH_SIZE];

	///cream pathway-ul fisierului
	if(strchr(name,'/'))
		get_name_from_path(name);
	strcpy(pathway,location);
	strcat(pathway,name);
	printf("%s\n",pathway);
	
	if(((*fout)=open(pathway,O_CREAT|O_WRONLY,0666))<=0)///<creez fisierul cu permisiunile de rw
	{
		perror("[client] Eroare la crearea/deschiderea fisierului.\n");
		read(0,pause_c,10);
		printf("\033[2J\033[1;1H");
		return errno;
	}

	return 1;
}


///Afisam progresul download-ului
void print_progress(long long downloaded,long long target,char name[])
{
	int percentage;
	char progress_bar[BAR_SIZE];
	long double a=downloaded,b=target;
	
	percentage=int(100*(a/b));///<calculam procentul progresului
	memset(progress_bar,'#',percentage); ///<cream bara de progres
	progress_bar[percentage]='\0';

	///Afisam informatiile si bara de progres 
	printf("\033[2J\033[1;1H");
	printf("%s is downloading:\n",name);
	printf("Progress %d%%: %s\n",percentage,progress_bar);
}



///descarcam fisierul de la peer
int receive_file(int socket_d, struct sockaddr_in server)
{
	long long size,bytes_received=0;
	long len;
	int fout;
	char name[300],pack[DATA_SIZE];
	printf("\033[2J\033[1;1H");
	///cititm numele si dimensiunea fisierului
	get_name_and_size(socket_d,name,&size);
	printf("File:%s \nSize: %lld B \n",name,size);
	read(0,pause_c,10);
	printf("\033[2J\033[1;1H");
		
	create_file(&fout,name);///<cream fisierul

	///citim pachetele de la peer
	while(bytes_received<size)
	{
		///citim din socket octeti
		if((len=read(socket_d,pack,DATA_SIZE))<=0)
		{
			perror("[client] Eroare la citirea pack-ului.\n");
			read(0,pause_c,10);
			printf("\033[2J\033[1;1H");
			return errno;
		}

		///scriem octetii in fisier
		if(write(fout,pack,len)<=0)
		{
			perror("[client] Eroare la scrierea in fisier a pack-ului.\n");
			read(0,pause_c,10);
			printf("\033[2J\033[1;1H");
			return errno;	
		}

		//printf("[client] Got %ld bytes.\n",len);

		print_progress(bytes_received,size,name);///<afisam progresul

		bytes_received=bytes_received+len;///<facem update la counter-ul de octeti cititi
	}

	printf("File downloaded.\n");
	read(0,pause_c,10);
	printf("\033[2J\033[1;1H");
	

	return 1;
}

///functia ce citeste rezultatul interogarii din baza de date
static int callback(void *data, int argc, char **argv, char **ColName)
{
    int i;
    fprintf(stderr, "%s: ", (const char*)data);
    for(i=0; i<argc; i++)
        strcpy(temp,argv[0]);///<copiem rezultatul in temp
    return 0;
}

///functia ce decodifica codul citit
int decode(char word1[],char word2[],char word3[])
{
	sqlite3 *db;///<baza de date
    char *error_msg = 0;
    char sql[200];
    const char* data = "Callback function called";
    
    ///deschidem baza de date
    if(sqlite3_open("cod.db", &db))
    {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    
    ///Preatim interogarea, decodificam primi 16 biti ai adresei peer-ului 
    strcpy(sql,"select ip_half from code where encoding='");
    strcat(sql,word1);
    strcat(sql,"';");
    
    ///Executam interogarea
    if( sqlite3_exec(db, sql, callback, (void*)data, &error_msg) != SQLITE_OK )
    {
        fprintf(stderr, "SQL error: %s\n", error_msg);
        sqlite3_free(error_msg);
        return 0;
    }
    
    strcpy(word1,temp);///<copiem rezultatul interogarii in word1

    ///Preatim interogarea, decodificam ultimi 16 biti ai adresei peer-ului 
    strcpy(sql,"select ip_half from code where encoding='");
    strcat(sql,word2);
    strcat(sql,"';");

    ///Executam interogarea
    if( sqlite3_exec(db, sql, callback, (void*)data, &error_msg) != SQLITE_OK )
    {
        fprintf(stderr, "SQL error: %s\n", error_msg);
        sqlite3_free(error_msg);
        return 0;
    }
    
    strcpy(word2,temp);///<copiem rezultatul interogarii in word2

    ///Preatim interogarea, decodificam portul peer-ului peer-ului 
    strcpy(sql,"select id from code where encoding='");
    strcat(sql,word3);
    strcat(sql,"';");

    ///Executam interogarea
    if( sqlite3_exec(db, sql, callback, (void*)data, &error_msg) != SQLITE_OK )
    {
        fprintf(stderr, "SQL error: %s\n", error_msg);
        sqlite3_free(error_msg);
        return 0;
    }
    
    strcpy(word3,temp);///<copiem rezultatul interogarii in word3

    sqlite3_close(db);///<inchidem baza de date

    return 1;
}

///citim codul pentru download
int get_code(char code[],char address[],char port[])
{
	int len;
	char word1[WORD_LENGTH],word2[WORD_LENGTH],word3[WORD_LENGTH];	

	printf("\033[2J\033[1;1H");
	printf("Write the code.\n");
	printf("Word 1: ");
	fflush(stdout);
	///citim primul cuvant
	if((len=read(0,word1,WORD_LENGTH))<=0)
	{
		perror("[get_code] Eroare la read.\n");
		read(0,pause_c,10);
		printf("\033[2J\033[1;1H");
		return errno;
	}
	word1[len-1]='\0';
	
	printf("Word 2: ");
	fflush(stdout);
	///citim al doilea cuvant
	if((len=read(0,word2,WORD_LENGTH))<=0)
	{
		perror("[get_code] Eroare la read.\n");
		read(0,pause_c,10);
		printf("\033[2J\033[1;1H");
		return errno;
	}
	word2[len-1]='\0';

	printf("Word 3: ");
	fflush(stdout);
	///citim al treilea cuvant
	if((len=read(0,word3,WORD_LENGTH))<=0)
	{
		perror("[get_code] Eroare la read.\n");
		read(0,pause_c,10);
		printf("\033[2J\033[1;1H");
		return errno;
	}
	word3[len-1]='\0';

	decode(word1,word2,word3);

	///compunem adresa si portul din codul descifrat
	strcpy(address,word1);
	strcat(address,".");
	strcat(address,word2);

	strcpy(port,word3);

	return 1;
}

///pregatim socketul pentru comunicare
int set_socket(int *socket_d,struct sockaddr_in *server,char *port,char *address)
{
	/// cream socketul 
	int port_int;

	if (((*socket_d)= socket (AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror ("Eroare la socket().\n");
	  	return errno;
	}

	port_int=atoi(port);
	
	///umplem structura folosita pentru realizarea conexiunii cu serverul 
	/// familia socket-ului 
	server->sin_family = AF_INET;
	/// adresa IP a serverului 
	server->sin_addr.s_addr = inet_addr(address);
	/// portul de conectare 
	server->sin_port = htons (port_int);

	return 1;
}

///initiem secventa de download
int client_p()
{
	int socket_d;			// descriptorul de socket
	struct sockaddr_in server;	// structura folosita pentru conectare 
	char port[10],address[20],code[150];

	get_code(code,address,port);///<citim codul

	set_socket(&socket_d,&server,port,address);///<setam socketul
	///ne conectam la server 
	if (connect (socket_d, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
	{
	  	perror ("[client]Eroare la connect().\n");
	  	return errno;
	}
 
	receive_file(socket_d,server);///<descarcam fisierul
	/// inchidem conexiunea
	close (socket_d);
}