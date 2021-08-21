#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <unistd.h>
#define MAXRCVLEN 1000
#define PORTNUM 110


int init(const char* serwer)
{

	char buffer[MAXRCVLEN + 1];

	int  mysocket;
	struct sockaddr_in dest;
	int len;
	struct hostent* hostinfo;


	hostinfo = gethostbyname(serwer);

	if (hostinfo == 0)
		return -1;

	mysocket = socket(AF_INET, SOCK_STREAM, 0);

	memset(&dest, 0, sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_addr = *(struct in_addr*)hostinfo->h_addr;
	dest.sin_port = htons(PORTNUM);

	if (connect(mysocket, (struct sockaddr*)&dest, sizeof(struct sockaddr)) < 0)
		return -1;
	return mysocket;

}
bool logowanie(int socket, const char* login, const char* haslo)
{
	char buffer[MAXRCVLEN + 1];

	int len;
	char komenda[] = "USER ";
	char pass[] = "PASS ";
	strcat(komenda, login);
	strcat(komenda, "\r\n");
	send(socket, komenda, strlen(komenda), 0);
	//printf("%s\n",komenda);
	len = recv(socket, buffer, MAXRCVLEN, 0);
	strcat(buffer, "\r\n");
	buffer[len] = '\0';
	//printf("login %s\n",buffer);
	if (strncmp(buffer, "+OK", 3) != 0)
		return false;

	strcpy(komenda, pass);
	strcat(komenda, haslo);
	strcat(komenda, "\r\n");
	send(socket, komenda, strlen(komenda), 0);
	//printf("%s\n",komenda);

	memset(buffer, 0, sizeof(buffer));
	len = recv(socket, buffer, MAXRCVLEN, 0);
	strcat(buffer, "\r\n");
	buffer[len] = '\0';
	printf("haslo %s\n", buffer);
	if (strncmp(buffer, "-ERR", 4) == 0)
		return false;
	return true;
}
void wyloguj(int socket)
{
	char komenda[] = "QUIT\r\n";
	int len;
	char buffer[MAXRCVLEN + 1];
	send(socket, komenda, strlen(komenda), 0);
	len = recv(socket, buffer, MAXRCVLEN, 0);

	if (strncmp(buffer, "+OK", 3) != 0)
		printf("Wystąpiły problemy z poprawnym wylogowaniem.\n");//else
	 //   printf("Wylogowano poprawnie.\n");  
}

void finito(int socket)
{
	close(socket);
}
void Czytaj(char* buff, int len, FILE* o2) //zapisanie uidl do pliku baza.uidlnew
{
	int i; bool s = false;
	for (i = 0; i < len; i++)
	{
		if (buff[i] == 32)
			s = true;

		if ((s) && (buff[i] != 32) && (buff[i] != '\r'))
			fputc(buff[i], o2);
		if (buff[i] == '\n')
			s = false;
	}
}
int CompareFiles(FILE* o1, FILE* o2)
{
	char buf[100]; int size = 100;
	char buf_old[100];
	int unew = 0;
	fseek(o2, 0, SEEK_SET);
	bzero(buf, 100);
	while (fscanf(o2, "%s\n", buf) > 0) //wyszukuje w bazie baza.uidlnew
	{
		unew++;
		bzero(buf_old, 100);
		fseek(o1, 0, SEEK_SET);
		//   printf("%s", buf);
		while (fscanf(o1, "%s\n", buf_old) > 0) //przeszukuje cały blik baza.uidl w poszukiwaniu 
		{                                 //danego uidl jeśli nie ma to znaczy że jest nowy email
			if (strcmp(buf, buf_old) == 0)
			{
				unew--;
				break;
			}
			// printf("stary uidl %s\nnowy  uidl %s",buf,buf_old);
		}
	}
	return unew;
}
int GetUIDL(int socket) //zwraca true gdy jest nowa wiadomość i false gdy nie ma
{
	char komenda[] = "UIDL\r\n";
	char* out1 = "baza.uidl";
	char* out2 = "baza.uidlnew";
	FILE* o1, * o2; int ret;
	o1 = fopen(out1, "rb");
	o2 = fopen(out2, "w+");
	int len, i;
	char buffer[MAXRCVLEN + 1];
	send(socket, komenda, strlen(komenda), 0);
	do {
		len = 0;
		len = recv(socket, buffer, MAXRCVLEN, 0);
		buffer[len] = '\0';
		Czytaj(buffer, len, o2);
		if (buffer[len - 3] == '.')break;
	} while (len != 0);
	ret = CompareFiles(o1, o2);
	fclose(o1);
	fclose(o2);
	if (ret > 0)
	{
		unlink(out1);
		rename(out2, out1);
	}
	return ret;
}
void SendNOP(int socket)
{
	char komenda[] = "NOP\r\n";
	int len;
	char buffer[MAXRCVLEN + 1];
	send(socket, komenda, strlen(komenda), 0);
	len = recv(socket, buffer, MAXRCVLEN, 0);
	buffer[len] = '\0';
}