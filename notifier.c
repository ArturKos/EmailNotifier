#include "notifier.h"
#include <stdlib.h>
#include <ncurses.h>
#include <stdio.h>
#include <time.h>


int nowe = 0;
char esc;
int main(int argc, char* argv[])
{
	int gniazdo;
	if (argc != 4)
	{
		printf("%s \n", "Program wymaga podania 3 argumentow.");
		printf("%s \n", "notifier adres_serwera nazwa_konta hasło");
		return 1;
	}


	initscr(); // init curses
	noecho();

	clear();
	printw("Program powiadamia o nowych wiadomosciach w skrzynce e-mail\n");
	printw("Dowolny klawisz konnczy dzialanie programu.\n");
	nodelay(stdscr, TRUE);
	int czas = 59;
	for (;;) {
		if ((esc = getch()) == ERR) {
			sleep(1);
			//  if(czas=5) SendNOP(gniazdo);


			if (czas++ >= 59)
			{
				clear();
				printw("Program powiadamia o nowych wiadomosciach w skrzynce e-mail\n");
				printw("Dowolny klawisz konnczy dzialanie programu.\n");

				gniazdo = init(argv[1]); // próba połączenia z wybranym serwerem

				if (gniazdo < 0)
				{ //nie udało się połączyć z serwerem
					printw("%s %s\n", "Nie mogę się połączyć z wybranym serwerem:", argv[1]);
					return 1;
				}
				printw("%s %s\n", "Logowanie do serwera:", argv[1]);
				printw("%s", "Sprawdzam poczte...\n");
				if (logowanie(gniazdo, argv[2], argv[3])) //próba logowania do serwera
				{ //udane logowanie
				 // printw ("Pomyślnie zalogowano.\n Sprawdzam wiadomości...\n");
			 //  printw("Minelo : %d sekund\n",czas);
					if ((nowe = GetUIDL(gniazdo)) > 0) { //zwraca true gdy jest nowa wiadomość i false w przeciwnym wypadku
						printw("Otrzymano %d nowych wiadomosci!\n", nowe);
						//   system("play wiadomosc.wav &> /dev/null");
					}
					else { printw("Brak nowych wiadomosci.\n"); }

				}
				else
				{ // nie udało się zalogować
					printw("%s\n", "Logowanie zakonczylo się niepowodzeniem. Sprawdz login i haslo.\n");
					return 1;
				}
				czas = 0;
				wyloguj(gniazdo);// wylogowanie z serwera
				finito(gniazdo); // zamknięcie gniazda

			}
		}
		else {
			break;
		}
	}

	endwin(); // end curses mode

   //  system("play wiadomosc.wav &> /dev/null");


	return 0;
}