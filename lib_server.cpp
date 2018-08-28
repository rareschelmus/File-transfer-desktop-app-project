#include "lib_server.h"


///vector in care pastram toate datele fiecarui stream
c_stream stream[N_STREAMS];
pid_t peers[N_PEERS];
int peers_count;
char pause_s[10];

///declaram global socketurile pentru a le putea accesa si din functiile ce lucreaza cu semnale
int socket_d,client;


///setam socketul pentru server
void set_server(struct sockaddr_in *server,struct sockaddr_in *from,int port)
{
	///pregatim structurile sockaddr_in
	bzero(&(*server), sizeof(*server));
	bzero(&(*from),sizeof(*from));

	///umplem structura server
	server->sin_family=AF_INET;
	///acceptam orice adresa
	server->sin_addr.s_addr=htonl(INADDR_ANY);
	///stabilim portul
	server->sin_port=htons(port);
}

///trimitem la peer numele si dimensiunea fisierului
int send_name_and_size(int socket_d, int client, char *file, long long size)
{
	int flag=1;
	char package[DATA_SIZE];

	///pregatim pachetul
	sprintf(package,"%s %lld",file,size);
	///trimitem pachetul
	if(write(client,package,DATA_SIZE)<=0) {
		perror("[server] Eroare la trimiterea dimensiunii fisierului.\n");
		read(0,pause_s,10);
		printf("\033[2J\033[1;1H");
		return errno;
	}

	return 1; 
}

///obtinem dimensiunea fisierului
long long get_size(char *file)
{
	struct stat stats;
	stat(file,&stats);
	return stats.st_size;
}

///deschidem fisierul
int open_file(int *fin, char *file)
{
	if(((*fin)=open(file,O_RDONLY,0666))<=0)
	{
		perror("[server] Eroare la deschiderea fisierului.\n");
		read(0,pause_s,10);
		printf("\033[2J\033[1;1H");
		return errno;	
	}

	return 1;
}

///trimite tracker-ului portul ,numele si dimensiunea fisierului 
int update_tracker(char file[],long long size,int stream_port)
{
	int socket_d;			///< descriptorul de socket
	struct sockaddr_in server;	///< structura folosita pentru conectare 
	char info[1000];		

	/// cream socketul 
	if ((socket_d = socket (AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror ("Eroare la socket().\n");
		return errno;
	}
	/// umplem structura folosita pentru realizarea conexiunii cu serverul 
	/// familia socket-ului 
	server.sin_family = AF_INET;
	/// adresa IP a serverului 
	server.sin_addr.s_addr = inet_addr("0.0.0.0");
	/// portul de conectare */
	server.sin_port = htons (TRACKER_PORT);

	/// ne conectam la server 
	if (connect (socket_d, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
	{
		perror ("[client]Eroare la connect().\n");
		return errno;
	}

	///pregatim pachetul cu informatii
	sprintf(info,"%s %lld %d",file,size,stream_port);

	/// trimiterea pachetului la tracker
	if (write (socket_d, info, 1000) <= 0)
	{
		perror ("[client]Eroare la write() spre server.\n");
		return errno;
	}
	///inchidem conexiunea
	close (socket_d);
}

///functia de gestiune a semnalului de inchidere a unui proces copil ce trimite un fisier
void child_signal_handler()
{	
	///inchidem conexiunea cu clientul
	close(client);
	close(socket_d);
	///inchidem procesul
	kill(getpid(),SIGKILL);
}

///trimite fisierul din stream unui client
int send_file(int socket_d,int client,char *file)
{
	long long size=0,bytes_sent=0;
	long len;
	int fin;
	char data[DATA_SIZE];
	//obtinem dimeniunea fisierului

	signal(SIGTERM,(void(*)(int))child_signal_handler);

	size=get_size(file);///<obtinem dimensiunea fisierului 

	///trimitem dimensiunea si numele fisierului
	send_name_and_size(socket_d,client,file,size);

	///deschidem fisierul
	open_file(&fin,file);

	///trimitem fisierului la client
	while(bytes_sent<size)
	{
		///citim octeti din fisier
		if((len=read(fin,data,DATA_SIZE))<=0)
		{
			perror("[server] Eroare la citirea din fisier.\n");
			return errno;
		}

		///scriem octetii cititi din fisier in client
		if(write(client,data,len)<=0)
		{
			perror("[server] Eroare la trimiterea bitilor.\n");
			return errno;
		}

		bytes_sent=bytes_sent+len;///<facem update la count-ul de octeti
	}
	return 1;
}

///stergem pid-ul procesului copil ce a terminat de trimis un fisier
void delete_pid(pid_t p)
{
	int i,j;
	for(i=0;i<peers_count;i++)
		if(peers[i]==p)
		{
			for(j=i;j<peers_count-1;j++)
				peers[j]=peers[j+1];
			peers_count--;
			break;
		}
}

/**functia de gestiune a semnalelor:
* cand un proces copil se termina, parintele primeste semnalul
* si sterge din lista de pid-uri pe copilul care a finalizat
*/
void peer_signal_handler()
{
	pid_t pid;
    int status;    

    while ((pid=waitpid(-1, &status, WNOHANG)) != -1)
    {
    	if(pid!=0)
        	delete_pid(pid);
    }
}

/**functia de gestiune a semnalelor::
* cand un stream este inchis, functia inchide toate procesele
* copil care inca trimit fisiere si apoi inchide conexiunea
*/
void stream_signal_handler()
{
	int i;
	for(i=0;i<peers_count;i++)
		kill(peers[i],SIGTERM);		

	///inchidem conexiunea
	close(client);
	close(socket_d);

	///inchidem procesul
	kill(getpid(),SIGKILL);
}

///server-ul stream-ului
int stream_server(int id_stream)
{
	struct sockaddr_in server,from;

	///sunt activate semnalele
	signal(SIGCHLD,(void(*)(int))peer_signal_handler);
	signal(SIGTERM,(void(*)(int))stream_signal_handler);

	if((socket_d=socket(AF_INET,SOCK_STREAM,0))==-1)
	{
	    perror ("[server]Eroare la socket().\n");
	    return errno;
	}
	///setam optiunea SO_REUSEADDR
	int on=1;
  	setsockopt(socket_d,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));

  	///pregatim structurile sockaddr_in
	set_server(&server,&from,stream[id_stream].port);
	///atasam socketul
	if(bind(socket_d,(struct sockaddr*) &server,sizeof(struct sockaddr))==-1)
	{
	    perror ("[server]Eroare la bind().\n");
	    return errno;
	}

	///punem serverul sa asculte clientii
	if(listen(socket_d, 5)==-1)
  	{
    	perror ("[server]Eroare la listen().\n");
    	return errno;
  	}

  	///servim clientii
  	while(1)
  	{
  		pid_t pid;
  		socklen_t length=sizeof(from);

  		///acceptam un client
  		client=accept(socket_d,(struct sockaddr*) &from,&length);

  		///verificam conexiunea cu clientul
  		if(client<0)
  		{
			perror ("[server]Eroare la accept().\n");
			continue;
  		}
  		///cream copil ce serveste clientul
  		if((pid=fork())<0)
  		{
  			perror("[server] Eroare la fork().\n");
  			return errno;
  		}
  		else
  			if(pid==0)
  			{
  				send_file(socket_d,client,stream[id_stream].file_name);
  				close(client);
  				close(socket_d);
  				exit(0);
  			}
  		///facem update la numarul de copii ce trimit fisiere
  		peers[peers_count++]=pid;
  		close(client);
  	}
}

///inseram un stream nou in vectorul de stream-uri
int insert_stream(pid_t pid, char file_name[NAME_SIZE])
{
	int i;
	///cautam un stream liber
	for(i=0;i<N_STREAMS;i++)
		if(stream[i].in_use==OFF)
			break;

	if(i==N_STREAMS)
		return -1;

	stream[i].id=i;
	stream[i].port=PORT+i;///< atribuim un port neutilizat la stream
	stream[i].pid=pid;
	stream[i].in_use=ON;
	strcpy(stream[i].file_name,file_name);

	return i;
}

///cream un stream de fisiere nou
int new_stream()	
{
	char file_name[NAME_SIZE];
	int len;
	pid_t pid;

	printf("\033[2J\033[1;1H");
	printf("Insert the name of the file or insert 000 to go back.\n");
	printf("File: ");
	fflush(stdout);
	///citim numele fisierului
	if((len=read(0,file_name,NAME_SIZE))<=0)
	{
		perror("[Upload file name]Eroare la citirea numelui fisierului.\n");
		read(0,pause_s,10);
		printf("\033[2J\033[1;1H");
		return errno;
	}

	file_name[len-1]='\0';

	if(strcmp(file_name,"000")==0)
		return 1;
	fflush(stdout);
	///verificam daca este fisier
	if(access(file_name,F_OK)==-1)
	{
		printf("Fisier inexistent.\n");
		read(0,pause_s,10);
		printf("\033[2J\033[1;1H");
		return 0;
	}
	else
		///cream un proces copil ce va gestiona streamul
		if((pid = fork())<0)
		{
			perror("[New_stream]eroare la fork.\n");
			return errno;			
		}
		else 
			if(pid == 0)
			{
				int id_stream;
				long long size=get_size(file_name);

				id_stream=insert_stream(pid,file_name);///<inregistram noul stream
				update_tracker(file_name,size,stream[id_stream].port);///<trimitem informatii la tracker
				stream_server(id_stream);///<streamul incepe a rula		
				exit(1);
			}
			else
			{
				printf("Stream uploaded.\n");
				insert_stream(pid,file_name);///<inregistram stream in parinte
				read(0,pause_s,10);
				printf("\033[2J\033[1;1H");
			}
	return 1;
}

///afiseaza stream-urile active
int view_streams()
{
	char command[10];
	int id,i,len;
	printf("\033[2J\033[1;1H");
	printf("Active streams:\n");
	for(i=0;i<N_STREAMS;i++)
		if(stream[i].in_use==ON)
			printf("%d:%s\n",i,stream[i].file_name);

	printf("Select stream to delete or 000 go back: ");
	fflush(stdout);
	///citim comanda
	if((len=read(0,command,10))<=0)
	{
		perror("Eroare la read.\n");
		read(0,pause_s,10);
		printf("\033[2J\033[1;1H");
		return errno;
	}
	command[len-1]='\0';
	if(strcmp(command,"000")==0)
		return 1;

	command[1]='\0';
	id=atoi(command);
	
	if(stream[id].in_use==ON)
	{
		kill_stream(id); ///<inchidem stream-ul
		printf("Stream %s was successfully killed.\n",command);
	}
	else
		printf("This stream doesn't exist.\n");


	read(0,pause_s,10);
	printf("\033[2J\033[1;1H");

	return 1;
}

///inchidem un stream activ
int kill_stream(int stream_id)
{   
	kill(stream[stream_id].pid,SIGTERM);///<inchidem stream-ul
	stream[stream_id].in_use=OFF;///<setam stream-ul ca neutilizat
	return 1;
}









