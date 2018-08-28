#include "lib_client.h"
#include "lib_server.h"

/**
* read(0,pause,10); < procesul pauzeaza pana la apasarea tastei enter
* printf("\033[2J\033[1;1H"); < sterge tot de pe ecran
*/
///meniul de download
int download_menu()
{
	char buff[COMMAND_SIZE],command;

	while(1)
	{
		printf("\033[2J\033[1;1H");
		printf("1.Insert code\n");
		printf("2.Set download directory\n");
		printf("3.Back\n");
		printf("Select: ");

		fflush(stdout);
		///citim comanda
		if(read(0,buff,COMMAND_SIZE)<=0)
		{
			perror("[Download]Eroare la citirea comenzii.\n");
			return errno;
		}

		command=buff[0];

	 	switch(command)
	 	{
	 		case '1':
	 			client_p();///<incepe secventa de download
	 			break;
	 		case '2':
	 			change_location();///<schimbam locatia
	 			break;
	 		case '3':
	 			return 1;///<inapoi la meniul anterior
	 			break;
	 		default: 
	 			printf("Wrong command.\n");
	 	}
 	}
	return 1;
}

///meniul de upload
int upload_menu()
{
	char buff[COMMAND_SIZE],command;

	while(1)
	{
		
		printf("\033[2J\033[1;1H");
		printf("1.Upload file\n");
		printf("2.View file streams\n");
		printf("3.Back\n");
		printf("Select: ");

		fflush(stdout);
		///citim comanda
		if(read(0,buff,COMMAND_SIZE)<=0)
		{
			perror("[Main]Eroare la citirea comenzii.\n");
			return errno;
		}

		command=buff[0];

	 	switch(command)
	 	{
	 		case '1':
	 			new_stream();///<cream noul stream
	 			break;
	 		case '2':
	 			view_streams();///afisam stream-urile
	 			break;
	 		case '3':
	 			return 1;///<inapoi la meniul anterior
	 			break;
	 		default: 
	 			printf("Wrong command.\n");
	 	}
 	}
	return 1;
}


///meniul principal
int main_menu()
{
	char buff[COMMAND_SIZE],command;

	while(1)
	{
		printf("\033[2J\033[1;1H");
		printf("1.Upload\n");
		printf("2.Download\n");
		printf("3.Exit\n");
		printf("Select: ");

		fflush(stdout);
		///citim comanda
		if(read(0,buff,COMMAND_SIZE)<=0)
		{
			perror("[Main]Eroare la citirea comenzii.\n");
			return errno;
		}

		command=buff[0];
		
	 	switch(command)
	 	{
	 		case '1':
	 			upload_menu();///<mergem la meniul de upload
	 			break;
	 		case '2':
	 			download_menu();///<mergem la meniul de download
	 			break;
	 		case '3':
	 			printf("Application is exiting.\n");///<inchidem aplicatia
	 			return 1;
	 			break;
	 		default: 
	 			printf("Wrong command.\n");
	 	}
 	}
	return 1;
}

int main()
{
	main_menu();///<intram in meniu
	return 0;
}