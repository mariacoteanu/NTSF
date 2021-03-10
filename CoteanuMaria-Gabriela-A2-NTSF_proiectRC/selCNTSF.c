/* cliTCPIt.c - Exemplu de client TCP
   Trimite un numar la server; primeste de la server numarul incrementat.
         
   Autor: Lenuta Alboaie  <adria@infoiasi.ro> (c)2009
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <sqlite3.h>

/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* portul de conectare la server*/
int port;

ludo[14][14]; /*tabla de joc*/

//declaram functiile
void initiazaTabla();
void PrintTabla();
void StergePion(int i, int j);
void actualizeazaTabla(int tura, int i, int j);
void printTabel(sqlite3 *db, char *user);


static int callback(void *beUsed, int argc, char **argv, char **ColmName) 
{
   int i;
   for(i = 0; i<argc; i++) {
      printf("%s = %s\n", ColmName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");
   return 0;
}

int main (int argc, char *argv[])
{
  int sd;			// descriptorul de socket
  struct sockaddr_in server;	// structura folosita pentru conectare 
  // mesajul trimis
  int nr=0;
  char buf[10];

  sqlite3 *db;
  char *errmsg = 0;
  char *tabel;

  int bdd = sqlite3_open("username.db", &db);

    if(bdd)
    {
      printf("Eroare la open baza de date: %s\n", sqlite3_errmsg(db));
      return 0;
    }
    else
    {
      printf("Succes la deschiderea bazei de date\n");
    }

  /* exista toate argumentele in linia de comanda? */
  if (argc != 3)
    {
      printf ("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
      return -1;
    }

  /* stabilim portul */
  port = atoi (argv[2]);

  /* cream socketul */
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("Eroare la socket().\n");
      return errno;
    }

  /* umplem structura folosita pentru realizarea conexiunii cu serverul */
  /* familia socket-ului */
  server.sin_family = AF_INET;
  /* adresa IP a serverului */
  server.sin_addr.s_addr = inet_addr(argv[1]);
  /* portul de conectare */
  server.sin_port = htons (port);
  
  /* ne conectam la server */
  if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
  {
    perror ("[client]Eroare la connect().\n");
    return errno;
  }



  //prima data ofera un username - il trimite serverului -- apoi la baza de date
  //primeste nr sau de ordine -- al cata-lea jucator e
  int nrjucatori, on;
  char user[20];
  int nrp;
  int ok =0;
  int zar;
  char stare[2];
  int curr; 
  int i, j;
  int rdd;
  int tura;
  int inc;
  char stare2[2];
  int castigator; 

  
    
  printf("Bine ai venit sa te joci NU TE SUPARA FRATE!\n");
  printf("Introduceti un username: \n");
  setbuf(stdin,NULL);
  fgets(user, sizeof(user), stdin);
  printf("Ti-ai ales username-ul : %s\n", user);
  if(write(sd, &user, strlen(user)) <= 0)
  {
    perror("Eroare write() username client!\n");
  }
  

  if (read (sd, &nr,sizeof(int)) < 0)
  {
    perror ("[client]Eroare la read() de la server.\n");
  }
  printf("Tu esti jucatorul %d. Cand e tura ta, scrie numarul si poti da cu zarul!\n", nr);

 
  //aflam cati jucatori sunt
  if(write(sd, &nr, sizeof(nr)) <= 0)
  {
    perror("Eroare write() jucatori client!");
  }
  if(read(sd, &nrjucatori, sizeof(int)) <=0)
  {
    perror("Eroare read() jucatori client!");
    printf("Erai singurul jucator conectat! Te rog sa revii\n");
    close(sd);
    return 0;
  }
  
  printf("Sunteti %d jucatori!\n", nrjucatori);


  initiazaTabla();
  PrintTabla();

  tura = 1;

  while(1)
  {
    
    if(tura == nr)
    {
      printf("Acum e randul tau! \n");

      do
      {
        printf("Pentru a da cu zarul, scrie care jucator esti ! \n");
            
        fgets(stare, sizeof(stare), stdin);
        curr = atoi(stare);
        if(curr != nr)
        {
          printf("Scrie corect care jucator esti!! \n");
        }
      }while(curr != nr);

      zar = WrRdZar(sd, nr);

      if(zar > 10)
      {
        castigator = zar - 10;
        if(castigator == nr)
        {
          printf("AI CASTIGAT!! \n");
        }
        else
        {
          printf("NU TE SUPARA FRATE, DAR NU AI CASTIGAT!\n");
        }
        printTabel(db, user);
        sleep(5);
        close(sd); 
        return 0;    
      }
        
      printf("Ai dat %d.\n", zar);

      while(zar == 6)
      {
        
      do{
      
        do{
          printf("Ai 4 pioni. Cu care vrei sa continui? \n");
          setbuf(stdin,NULL);
          fgets(stare2, sizeof(stare2), stdin);
          nrp = atoi(stare2);
            
          if(nrp <1 || nrp>4)
          {
            printf("Ai de ales un numar intre 1 si 4! \n");
          }
          else ok = 1;

        }while(ok == 0);
        ok = 0;

        if(write(sd, &nrp, sizeof(int)) <= 0)
        {
          perror("Eroare write pion.");
        }
        if(read(sd, &nrp, sizeof(int)) <= 0)
        {
          perror("Eroare read pion");
        }
       
       if(nrp == 5) // asta e un caz de eroare, pentru while-ul din functia din server
        {
          printf("Alege un pion nou in joc, neterminat!\n");
        }

      }while(nrp == 5);  
        //urmeaza doua read si doua write-uri
        //pentru a primi i si j la care este mutat pionul!
        Primesteij(sd, tura, nrp);
          
         //ca sa mai dea odata cu zarul pt ca a fost un 6 inainte.
        do
        {
          printf("Pentru a da cu zarul, scrie care jucator esti ! \n");
            
          fgets(stare, sizeof(stare), stdin);
          curr = atoi(stare);
          if(curr != nr)
          {
            printf("Scrie corect care jucator esti!! \n");
          }
        }while(curr != nr);

        //daca a fost un 6, are voie sa mai dea odata, se reia procedeul, daca nu, muta pionul cat e
        zar = WrRdZar(sd, nr);
        printf("Ai dat %d.\n", zar);

      }
      if(zar != 6)
      {
        
        //aflam starea jocului
        if(write(sd, &nr, sizeof(int))<=0)
        {
          perror("Eroare write pioniAcitivi client");
        }
        if(read(sd, &on, sizeof(int))<0)
        {
          perror("Eroare read pioniActivi client");
        }
      if(on > 0)
      {
        do
        {
          do
          {
            printf("Ai 4 pioni. Cu care vrei sa continui? \n");
            fgets(stare2, sizeof(stare2), stdin);
            nrp = atoi(stare2);
            
            if(nrp <1 || nrp>4)
            {
              printf("Ai de ales un numar intre 1 si 4! \n");
            }
            else ok = 1;
          }while(ok == 0);
          ok = 0;

          if(write(sd, &nrp, sizeof(int)) <= 0)
          {
            perror("Eroare write pion.");
          }
          if(read(sd, &nrp, sizeof(int)) <= 0)
          {
            perror("Eroare read pion");
          }
          if(nrp == 5) // asta e un caz de eroare, pentru while-ul din functia din server
          {
            printf("Alege un pion care este mutat in joc\n");
          }
        }while(nrp == 5);

        Primesteij(sd, tura, nrp);
      }
      else
      {
        printf("Asteptam un 6\n");
      }
      
     }       

    }
    
   tura++;
   if(tura >4) tura=1;
  }

  
  /* inchidem conexiunea, am terminat */
  close (sd);
}

void initiazaTabla()
{	
	/* tabla ar trebui sa arate asa :*/
	/*

	*****000:::::
	*V*V*0:0:R:R:
	*****0:0:::::
	*V*V*0:0:R:R:
	*****0:0:::::
	00000%%%00000
	0****%%%....0
	00000%%%00000
	^^^^^0^0.....
	^G^G^0^0.A.A.
	^^^^^0^0.....
	^G^G^0^0.A.A.
	^^^^^000.....
	
	*/ 


	/* hard codez tabla de joc*/
	int i,j;
	for(i = 1; i<=5; i++)
		for(j=1;j<=13; j++)
			if(j<=5) ludo[i][j] = '*'; /* casa verde */
			else if(j>=9) ludo[i][j] = ':';/* casa rosu */

	for(i=9; i<=13; i++)
		for(j=1;j<=13; j++)
			if(j<=5) ludo[i][j] = '^';/* casa galben */
			else if(j>=9) ludo[i][j] = '.';/* casa albastru */	
	for(i=6;i<=8;i++)
		for(j=6;j<=8;j++)
			ludo[i][j] = '%'; /* mijlocul tablei unde nu am nimic */
	
	/* punem finish-ul */
	for(j=2;j<=5; j++)
		ludo[7][j]='*'; /* verde */
	for(j=9;j<=12; j++)
		ludo[7][j] = '.';/*albastru*/
	for(i=2; i<=5; i++)
		ludo[i][7] = ':';/*rosu*/
	for(i=9; i<=12; i++)
		ludo[i][7] = '^';/*galben*/

	/*pionii*/

	/*VERDE*/
	ludo[2][2] = ludo[2][4] = ludo[4][2] = ludo[4][4] = 'V';

	/*ROSU*/
	ludo[2][10] = ludo[2][12] = ludo[4][10] = ludo[4][12] = 'R';

	/*GALBEN*/
	ludo[10][2] = ludo[10][4] = ludo[12][2] = ludo[12][4] = 'G';

	/*ALBASTRU*/
	ludo[10][10] = ludo[10][12] = ludo[12][10] = ludo[12][12] = 'A';

}

void PrintTabla()
{
  int i, j;
  for(i = 1; i<=13; i++)
  {
    for(j=1; j<=13; j++)
    {
      if(ludo[i][j] == 0)
      {
        printf("0");
      }
      else
      {
        printf("%c", (char)ludo[i][j]);
      }     
    }
    printf("\n");
  }
}

void StergePion(int i, int j)
{
  
  ludo[i][j] = 0;

}

void actualizeazaTabla(int tura, int i, int j)
{
  int culoare;

  switch(tura)
  {
    case 1:
      culoare = 'G';
      break;
    case 2:
      culoare = 'V';
      break;
    case 3:
      culoare = 'R';
      break;
    case 4:
      culoare = 'A';
      break;
  }

  ludo[i][j] = culoare;

}

int WrRdZar(int sd, int nr)
{
  int zar;
  if(write(sd, &nr, sizeof(int)) <= 0)
  {
    perror("Eroare write() client");
  }
  if(read(sd, &zar, sizeof(int)) <= 0)
  {
    perror("Eroare read() client zar!");
  }

  return zar;
}

void Primesteij(int sd, int tura, int nrp)
{
  int i,j;
  //urmeaza doua read si doua write-uri
  //pentru a primi i si j la care este mutat pionul!
     
  if(write(sd, &nrp, sizeof(int)) <= 0)
  {
    perror("Eroare write client care pion!");
  }
  if(read(sd, &i, sizeof(int)) <=0)
  {
    perror("Eroare read i de la server!");
  }

  if(write(sd, &nrp, sizeof(int)) <= 0)
  {
    perror("Eroare write client care pion!");
  }
  if(read(sd, &j, sizeof(int)) <= 0)
  {
    perror("Eroare read j de la server");
  }
  
  actualizeazaTabla(tura, i,j);
  PrintTabla();
  StergePion(i,j);

}

void printTabel(sqlite3 *db, char user[20])
{/*afisez pentru user care este activitatea lui de pana atunci*/
  char *errmsg = 0;
  char comanda[500];
  int bdd;

  strcpy(comanda , "SELECT * from JUCATORI WHERE USERNAME='user';");
  sprintf(comanda, "SELECT * from JUCATORI WHERE USERNAME='%s';", user);

  

    bdd = sqlite3_exec(db, comanda, callback, 0, &errmsg);
   
    if( bdd != SQLITE_OK )
    {
      printf("SQL error: %s\n", errmsg);
      sqlite3_free(errmsg);
    } 
    else 
    {
      printf("Te mai asteptam\n");
    }
}
