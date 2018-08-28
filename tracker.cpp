//#include "lib_tracker.h" 
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
/* portul folosit */
#define PORT_RX 8079
#define PORT_TX 8080
#define PACK_SIZE 500

/* codul de eroare returnat de anumite apeluri */
extern int errno;

int info_pipe[2]; ///<cream un pipe pentru comunicarea intre procese
int count; 
char data[100*PACK_SIZE]="",pack[PACK_SIZE];
pid_t tx_pid;
char temp[50];
char size_metrics[10][4]={"B","KB","MB","GB"};



///functie de conversie de la char* la long long
long long array_to_lli(char *size_in_char)
{
    long long size=0,rank=10;
    int i,length;

    length=strlen(size_in_char);

    for(i=0;i<length;i++)
        size=size*rank+(size_in_char[i]-'0');

    return size;
}

///pregatim dimensiunea pentru afisarea in B,KB,MB sau GB
void convert_size(char size[])
{
    int len;
    char decimal[2];

    len=strlen(size);

    if(len/4==0)
        strcat(size," B");
    else
        if(len/4>0)
        {
            decimal[0]=size[len%3];
            decimal[1]='\0';
            size[len%3]='\0';
            strcat(size,",");
            strcat(size,decimal);
            strcat(size," ");
            strcat(size,size_metrics[len/3]);
        }
}

///functie pentru gestioneaza rezultatele de la interogari 
static int callback(void *data, int argc, char **argv, char **ColName)
{
    int i;
    for(i=0; i<argc; i++)
        strcpy(temp,argv[0]);///<copiem rezultatul in variabila globala
    return 0;
}

///separam primi 16 biti si ultimi 16 biti din IP
void dissect_ip(char ip[], char ip_part1[], char ip_part2[])
{
    char *tmp;

    tmp=strtok(ip,".");
    strcpy(ip_part1,tmp);
    strcat(ip_part1,".");

    tmp=strtok(NULL,".");
    strcat(ip_part1,tmp);

    tmp=strtok(NULL,".");
    strcpy(ip_part2,tmp);
    strcat(ip_part2,".");

    tmp=strtok(NULL,".");
    strcat(ip_part2,tmp);

}

///generam codul pentru ip si port
int generate_code(char code[],char ip[],char port[])
{
    sqlite3 *db;
    char *error_msg = 0;
    char sql[200];
    const char* data = "";
    char ip_part1[10],ip_part2[10];
    ///deschidem baza de date
    if( sqlite3_open("cod.db", &db) )
    {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return(0);
    }
    ///separam in doua parti ip-ul
    dissect_ip(ip,ip_part1,ip_part2);

    ///pregatim interogarea pentru primii 16 biti din ip
    strcpy(sql,"select encoding from code where ip_half='");
    strcat(sql,ip_part1);
    strcat(sql,"';");
    
    ///executam interogarea
    if( sqlite3_exec(db, sql, callback, (void*)data, &error_msg) != SQLITE_OK )
    {
        fprintf(stderr, "SQL error: %s\n", error_msg);
        sqlite3_free(error_msg);
    }
   
    strcpy(code,temp);///<copiem in code primul cuvant din cod
    strcat(code," ");

    ///pregatim interogarea pentru ultimii 16 biti din ip
    strcpy(sql,"select encoding from code where ip_half='");
    strcat(sql,ip_part2);
    strcat(sql,"';");

    ///executam interogarea
    if( sqlite3_exec(db, sql, callback, (void*)data, &error_msg) != SQLITE_OK )
    {
        fprintf(stderr, "SQL error: %s\n", error_msg);
        sqlite3_free(error_msg);
    }
    
    strcat(code,temp);///<concatenam al doilea cuvant rezultat din interogare in code
    strcat(code," ");

    ///pregatim interogarea pentru port
    strcpy(sql,"select encoding from code where id='");
    strcat(sql,port);
    strcat(sql,"';");

    ///executam interogarea
    if( sqlite3_exec(db, sql, callback, (void*)data, &error_msg) != SQLITE_OK )
    {
        fprintf(stderr, "SQL error: %s\n", error_msg);
        sqlite3_free(error_msg);
    }
    
    strcat(code,temp);///<concatenam a al treilea cuvant codificat

    ///inchidem baza de date
    sqlite3_close(db);
    return 0;
}

///obtinem ip-ul clientului
int get_ip(int client,char ip[])
{
    socklen_t len;
    struct sockaddr_storage addr;
    char ip_string[INET6_ADDRSTRLEN];
    int port;

    len = sizeof(addr);
    getpeername(client, (struct sockaddr*)&addr, &len);///<extragem informatii despre client

    struct sockaddr_in *s = (struct sockaddr_in *)&addr;
    port = ntohs(s->sin_port);
    inet_ntop(AF_INET, &s->sin_addr, ip_string, sizeof (ip_string));///<extragem adresa clientului 

    strcpy(ip,ip_string);
    return 1;
}

///extragem din pachetul primit portul, numele si dimensiunea fisierului
void dissect_pack(char info_pack[],char name[],char size[],char port[])
{
    char *tmp;

    tmp =strtok(info_pack," ");
    strcpy(name,tmp);

    tmp =strtok(NULL," ");
    strcpy(size,tmp);
    convert_size(size);///<convertim dimeniunea in B, KB, MB sau GB

    tmp =strtok(NULL," ");
    strcpy(port,tmp);   
}

/**pregatim pachetul pentru a-l incarca pe pagina
* va contine numele, dimensiunea convertita si codul
*/
int prepare_pack(int client,char info_pack[], char peer_ip[])
{
    char size[20],name[200],port[5],code[200];

    if(get_ip(client,peer_ip)!=1)
        return 0;

    dissect_pack(info_pack,name,size,port);
    
    generate_code(code,peer_ip,port);

    strcpy(info_pack,"Name: ");
    strcat(info_pack,name);
    strcat(info_pack,"\n");
    strcat(info_pack,"Size: ");
    strcat(info_pack,size);
    strcat(info_pack,"\n");
    strcat(info_pack,"Code: ");
    strcat(info_pack,code);
    strcat(info_pack,"\n\n");

    //combine
    return 1;
}

/**primim de la client pe portul 8079 informatiile 
* le pregatim si le trimitem la procesul parinte prin pipe
*/
int receive_info(int socket_d,int client)
{
    char info_pack[PACK_SIZE],peer_ip[20];
    int length;

    get_ip(client,peer_ip);///<obtinem ip-ul clientului

    ///citim pachetul cu portul, numele si dimesiunea fisierului
    if((length=read(client,info_pack,PACK_SIZE))<=0)
    {
        perror("[RX] Eroare la read.\n");
        return errno;
    }

    ///pregatim pahetul pentru afisare
    prepare_pack(client,info_pack,peer_ip);

    ///scriem in pipe pachetul pregatit
    if(write(info_pipe[1],info_pack,strlen(info_pack))<=0)
    {
        perror("[RX] Eroare la write.\n");
        return errno;
    }
    ///inchidem clientul
    close(client);

    ///trimitem semnal la procesul parinte pentru a citi pachetul din pipe 
    kill(tx_pid,SIGUSR1);

    return 1;
}

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

///procedura gestioneaza clientii ce trimit pachete cu informatii despre stream-uri
int receive_stream_info()
{
    struct sockaddr_in server;  ///< structura folosita de server
    struct sockaddr_in from;    
    int socket_d;         ///<descriptorul de socket
    pid_t pid; 

    ///inchidem capatul de read al pipe-ului 
    close(info_pipe[0]);

    ///cream sochetul
    if ((socket_d = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("[server]Eroare la socket().\n");
      return errno;
    }
    ///setam optiunea SO_REUSEADDR
    int on=1;
    setsockopt(socket_d,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));

    ///setam serverul
    set_server(&server,&from,PORT_RX);
  
    /// atasam socketul
    if (bind (socket_d, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
        perror ("[server]Eroare la bind().\n");
        return errno;
    }

    /// punem serverul sa asculte daca vin clienti sa se conecteze
    if (listen (socket_d, 5) == -1)
    {
        perror ("[server]Eroare la listen().\n");
        return errno;
    }

    ///servim clientii in mod concurent
    while (1)
    {
        int client;
        socklen_t length=sizeof(from);

        /// acceptam un client
        client = accept (socket_d, (struct sockaddr *) &from, &length);

        /// eroare la acceptarea conexiunii de la un client
        if (client < 0)
        {
            perror ("[server]Eroare la accept().\n");
            continue;
        }
        ///cream un proces copil ce sa gestioneze primirea pachetelor
        if((pid=fork())<0)
        {
            perror("[RX] Eroare la fork.");
            return errno;
        }
        else
            if(pid==0)
            {
                receive_info(socket_d,client);///<primeste informatiile de la client
                close(client);///<inchidem clientul
                exit(1);
            }
        /// am terminat cu acest client, inchidem conexiunea 
        close (client);///<inchidem clientul 
    }
}

///funcie de gestiune a semnalelor ce vin de la rpcesul copil cand trimite un pachet pentru afisare
void signal_pack_handler()
{
    int len;

    if(len=(read(info_pipe[0],pack,PACK_SIZE))<=0)
        perror("[SIG] Eroare la read.\n");
    strcat(data,pack);///<concatenam pachetul in date

}
///functie ce serveste clientii ce se conecteaza prin browser
int serve_client(int client)
{
    char msg[200];      ///<mesajul primit de la client 
    char msgrasp[32000]=" ";        ///<mesaj de raspuns pentru client
    char payload[32000];

    bzero (msg, 200);
    /// citirea mesajului din broswer
    if (read (client, msg, 200) <= 0)
    {
        perror ("[server]Eroare la read() de la client.\n");
        return errno;       
    }
    ///pregatim pachetul pentru browser
    bzero(msgrasp,sizeof(msgrasp));
    snprintf(payload, sizeof(payload), "%s\n",data); 
    snprintf(msgrasp, sizeof(msgrasp), "HTTP/1.1 200 OK\nServer: myhttp\nContent-Length: %lu\nConnection: close\nContent-Type: text\n\n%s", strlen(payload), payload);
    
    /// trimitem pachetul clientului in browser
    if (write (client, msgrasp, strlen(msgrasp) + 1) <= 0)
    {
        perror ("[server]Eroare la write() catre client.\n");
        return errno;       /* continuam sa ascultam */
    }
    return 1;
}

///servim clientii ce se conecteaza la portul 8080 prin browser
int streaming_page()
{
    struct sockaddr_in server;  ///< structura folosita de server
    struct sockaddr_in from;    

    int socket_d;         ///<descriptorul de socket 
    pid_t pid;

    ///<inchidem capatul de write al pipe-ului
    close(info_pipe[1]);

    ///gestionam semnalele primite de la procesul copil
    signal(SIGUSR1,(void(*)(int))signal_pack_handler);
    /// crearea unui socket 
    if ((socket_d = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("[server]Eroare la socket().\n");
      return errno;
    }
    ///setam optiunea SO_REUSEADDR
    int on=1;
    setsockopt(socket_d,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
    ///setam serverul
    set_server(&server,&from,PORT_TX);
    
    /// atasam socketul 
    if (bind (socket_d, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
        perror ("[server]Eroare la bind().\n");
        return errno;
    }

    /// punem serverul sa asculte daca vin clienti sa se conecteze 
    if (listen (socket_d, 5) == -1)
    {
        perror ("[server]Eroare la listen().\n");
        return errno;
    }

    /// servim in mod concurent clientii 
    while (1)
    {
        int client;
        socklen_t length=sizeof(from);

        /// acceptam un client (stare blocanta pina la realizarea conexiunii)
        client = accept (socket_d, (struct sockaddr *) &from, &length);

        /// eroare la acceptarea conexiunii de la un client 
        if (client < 0)
        {
            perror ("[server]Eroare la accept().\n");
            continue;
        }

        ///cream un proces copil pentru a servi un client
        if((pid=fork())<0)
        {
            perror("[TX] Eroare la fork.\n");
            return errno;
        }
        else 
            if(pid==0)
            {
                serve_client(client);
                close(client);
                close(socket_d);
                exit(1);
            }
        
        /// am terminat cu acest client, inchidem conexiunea 
        close (client);
    }   
}


int main ()
{
    ///cream un pipe
    if(pipe(info_pipe)<0)
    {
        perror("Eroare la pipe.\n");
        return errno;
    }

    /**cream un proces copil ce se va ocupa de servirea clientilor ce se conecteaza prin browser
    * iar procesul parinte va gestiona primirea informatiilor despre stream-uri
    */
    if((tx_pid=fork())<0)
    {
        perror("Eroare la fork.\n");
        return errno;
    }
    else
        if(tx_pid==0)
        {
            streaming_page();
            close(info_pipe[0]);
            exit(1);
        }
    printf("Tracker is up and running.\n");
    receive_stream_info();
    close(info_pipe[1]);
    return 0;
}	      	
