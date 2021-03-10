/* servTCPCSel.c - Exemplu de server TCP concurent 
   
   Asteapta un "nume" de la clienti multipli si intoarce clientilor sirul
   "Hello nume" corespunzator; multiplexarea intrarilor se realizeaza cu select().
   
   Cod sursa preluat din [Retele de Calculatoare,S.Buraga & G.Ciobanu, 2003] si modificat de 
   Lenuta Alboaie  <adria@infoiasi.ro> (c)2009
   
*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>


/* portul folosit */

#define PORT 2728

extern int errno;		/* eroarea returnata de unele apeluri */

int ludo[14][14];
int pioni[16][2];/*toti pionii din joc*/
int pion1[4] = {0, 4, 8, 12};/* pozitiile primului pion a fiecarui jucator*/
int on[5];
int pioniTerminati[5]; /*pentru fiecare jucator, cati pioni a terminat de mutat*/
int pioniFinish[17];/*care pioni sunt prelucrati pentru a fi terminati*/

/*declarare functii folosite*/
void Fork( int Jucatori[5], char Username[5][20], int nrjucatori, sqlite3 *db);
void initiazaTabla();
void PrintTabla();
void initiazaPion(int tura, int nrp, int *i, int *j);
void mutaPion(int tura, int nr, int nrp, int *i, int *j, int Jucatori[5], int nrjucatori);
void trimiteInapoi(int tura, int lin, int col);
int verificLiber(int tura, int nr, int lin, int col);
void Finish(int tura, int nr, int nrp, int *i, int *j, int Jucatori[5], int nrjucatori);
void GameOver( int tura, int Jucatori[5], int nrjucatori);
int RdWrZar(int cl, int tura);
int RdWrCarePion(int cl, int tura, int caz);
void RdWrij(int cld, int i, int j);
int RollDice();
int pioniActivi(int tura, int cld);
void clasament(int nrjucatori);
void InsertUsername(char Username[20], sqlite3 *db);
void updateTable(char Username[5][20],int castigator, int nrJucatori, sqlite3 *db);
void printTable(sqlite3 *db);


/* functie de convertire a adresei IP a clientului in sir de caractere */
char * conv_addr (struct sockaddr_in address)
{
  static char str[25];
  char port[7];

  /* adresa IP a clientului */
  strcpy (str, inet_ntoa (address.sin_addr));	
  /* portul utilizat de client */
  bzero (port, 7);
  sprintf (port, ":%d", ntohs (address.sin_port));	
  strcat (str, port);
  return (str);
}
/*functia callback pentru comenzi sql*/
static int callback(void *beUsed, int argc, char **argv, char **ColmName) 
{
   int i;
   for(i = 0; i<argc; i++) {
      printf("%s = %s\n", ColmName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");
   return 0;
}

/* programul */
int main ()
{
  struct sockaddr_in server;	/* structurile pentru server si clienti */
  struct sockaddr_in from;
  fd_set readfds;		/* multimea descriptorilor de citire */
  fd_set actfds;		/* multimea descriptorilor activi */
  struct timeval tv;		/* structura de timp pentru select() */
  int sd, client;		/* descriptori de socket */
  int optval=1; 			/* optiune folosita pentru setsockopt()*/ 
  
  int nfds;			/* numarul maxim de descriptori */
  int len;			/* lungimea structurii sockaddr_in */

  int Jucatori[5], nrclienti = 0, nrjucatori = 0;
  int nrc;
	char USERNAME[5][20];
	char user[20]="";

  time_t oldtime;
  time_t currtime;

    sqlite3 *db;
    char *errmsg = 0;
    char *tabel;
    char comanda[500];

    /*DESCHID BAZA DE DATE*/
    int bdd = sqlite3_open("username.db", &db);

    if(bdd)
    {
        fprintf(stderr, "Eroare la open baza de date: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    else
    {
        fprintf(stdout, "Succes la deschiderea bazei de date\n");
    }

  /* AICI CREAM TABELUL JOCULUI */
    tabel = "CREATE TABLE IF NOT EXISTS JUCATORI("  \
       		"USERNAME       CHAR(20)     PRIMARY KEY," \
      		"SCOR           INT," \
			"CASTIG      INT," \
			"PIERDERI       INT);";
	bdd = sqlite3_exec(db, tabel, callback, 0, &errmsg);
   
    if( bdd != SQLITE_OK )
    {
      fprintf(stderr, "SQL error: %s\n", errmsg);
      sqlite3_free(errmsg);
    } 
    else 
    {
      fprintf(stdout, "Table created successfully\n");
    }

  /* creare socket */
	if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("[server] Eroare la socket().\n");
      return errno;
    }

	/*setam pentru socket optiunea SO_REUSEADDR */ 
	setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,&optval,sizeof(optval));

	/* pregatim structurile de date */
	bzero (&server, sizeof (server));

	/* umplem structura folosita de server */	
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl (INADDR_ANY);
	server.sin_port = htons (PORT);

	/* atasam socketul */
	if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
      perror ("[server] Eroare la bind().\n");
      return errno;
    }

	/* punem serverul sa asculte daca vin clienti sa se conecteze */
	if (listen (sd, 4) == -1)
    {
      perror ("[server] Eroare la listen().\n");
      return errno;
    }
  
	/* completam multimea de descriptori de citire */
	FD_ZERO (&actfds);		/* initial, multimea este vida */
	FD_SET (sd, &actfds);		/* includem in multime socketul creat */

	tv.tv_sec = 1;		/* se va astepta un timp de 1 sec. */
	tv.tv_usec = 0;
  
	/* valoarea maxima a descriptorilor folositi */
	nfds = sd;

	printf ("[server] Asteptam la portul %d...\n", PORT);
	fflush (stdout);
	
	oldtime = time(NULL); 


	/* servim in mod concurent clientii... */
	while (1)
    {
    	/* ajustam multimea descriptorilor activi (efectiv utilizati) */
    	bcopy ((char *) &actfds, (char *) &readfds, sizeof (readfds));

    	/* apelul select() */
    	if (select (nfds+1, &readfds, NULL, NULL, &tv) < 0)
	    {
	    	perror ("[server] Eroare la select().\n");
	    	return errno;
	    }
      
    	/* vedem daca e pregatit socketul pentru a-i accepta pe clienti */
    	if (FD_ISSET (sd, &readfds))
	    {
	      /* pregatirea structurii client */
	    	len = sizeof (from);
	    	bzero (&from, sizeof (from));

	    	/* a venit un client, acceptam conexiunea */
	    	client = accept (sd, (struct sockaddr *) &from, &len);

	    	/* eroare la acceptarea conexiunii de la un client */
	    	if (client < 0)
	    	{
	        	perror ("[server] Eroare la accept().\n");
	        	continue;
	      	}
        	else
        	{	  
            	nrclienti++;
            	nrjucatori++;
            	Jucatori[nrjucatori] = client;

				if(read(client, &user, sizeof(user)) <= 0)
				{
					perror("Eroare read initial server\n");
				}
				

				strcpy(USERNAME[nrjucatori], user);
				InsertUsername(user, db);
				bzero(&user, sizeof(user));

				if(write(client, &nrjucatori, sizeof(int)) <= 0)
				{
					perror("Eroare write initial server\n");
				}				

        	}

        	if (nfds < client) /* ajusteaza valoarea maximului */
        		nfds = client;
            
	    	/* includem in lista de descriptori activi si acest socket */
	    	FD_SET (client, &actfds);

	    	printf("[server] S-a conectat clientul cu descriptorul %d, de la adresa %s.\n",client, conv_addr (from));
	    	fflush (stdout);

			currtime = time(NULL);

			if(currtime - oldtime >= 15)
			{
				printf("Au trecut %d secunde\n", currtime-oldtime);
				if(nrjucatori == 1)
				{
					close(client);
				}			
				else if(nrjucatori >=2 && nrjucatori <= 4) 
				{	
					for(int i=1; i<=nrjucatori; i++)
					{
						printf("JUCATORUL %d, username=%s, descriptor=%d\n", i, USERNAME[i], Jucatori[i]);
					}
					Fork(Jucatori,USERNAME, nrjucatori, db);			
				}

				/*aici reinitiez vectorul de descriptorul si cel de username pentru urmatorii clienti*/
				for(int i = 1; i <= nrjucatori; i++)
                {
					FD_CLR (Jucatori[i], &actfds);
                    Jucatori[i]=0;
					bzero(&USERNAME[i], sizeof(USERNAME[i]));
                }
				nrjucatori = 0;   

				oldtime = time(NULL);
			}
			else if(nrjucatori == 4)
            {/*chiar daca nu a trecut timpul, jocul poate incepe deja cu 4 jucatori si vor fi prelucrati in continuare clientii*/
				for(int i=1; i<=nrjucatori; i++)
				{
					printf("JUCATORUL %d, username=%s, descriptor=%d\n", i, USERNAME[i], Jucatori[i]);
				}

                Fork(Jucatori, USERNAME,  nrjucatori, db);
                
                for(int i =1; i<=4; i++)
                {
					FD_CLR (Jucatori[i], &actfds);
                    Jucatori[i]=0;
					bzero(&USERNAME[i], sizeof(USERNAME[i]));
                }
				nrjucatori = 0;
				
            }
			
	    }
     	
    }				/* while */
}				/* main */

void Fork( int Jucatori[5], char Username[5][20], int nrjucatori, sqlite3 *db)
{/*aceasta este functia principala, start-ul jocului*/
  pid_t pp; /*pentru fork*/
  int i, nrc;

  for(i=1; i<=nrjucatori; i++)
  {
		if(read(Jucatori[i], &nrc, sizeof(int)) <= 0)
		{
			perror("Eroare read initial server\n");
		}
		if(write(Jucatori[i], &nrjucatori, sizeof(int)) <= 0)
		{
			perror("Eroare write initial server\n");
		}  
  }
  
  pp = fork();
  switch(pp)
  {
  case -1:
    perror("Eroare la fork()");
    break;
  case 0:
     //sunt in copil - aici desfasor jocul
     initiazaTabla();
     PrintTabla();
     int tura;
	  int nr; /*numarul de pe zar*/
	  int nrp;
	  tura=1;
	  int gata = 0;

	  while(gata == 0)
	  {	
		  
		  nr=RdWrZar(Jucatori[tura], tura);/*aici primesc numarul de pe zar*/
		
		  while(nr == 6)
		  {
			nrp = RdWrCarePion(Jucatori[tura], tura, nr);/*aici primesc ce pion vrea jucatorul sa mute*/
	
			if(pioni[pion1[tura-1] + nrp - 1][0] == 0) /*daca pionul ales nu e initiat--il initiem in joc*/
			{
				on[tura-1]=1;
				initiazaPion(tura, nrp, &pioni[pion1[tura-1] + nrp - 1][0], &pioni[pion1[tura-1] + nrp - 1][1]);
				RdWrij(Jucatori[tura], pioni[pion1[tura-1] + nrp - 1][0], pioni[pion1[tura-1] + nrp - 1][1] );
				PrintTabla();
			}
			else
			{
				ludo[ pioni[pion1[tura-1] + nrp - 1][0] ][ pioni[pion1[tura-1] + nrp - 1][1] ] = 0;/*mut pionul asa ca trebuie sa las pozitia goala*/
				mutaPion(tura, nr, nrp, &pioni[pion1[tura-1] + nrp - 1][0], &pioni[pion1[tura-1] + nrp - 1][1], Jucatori, nrjucatori);
				RdWrij(Jucatori[tura], pioni[pion1[tura-1] + nrp - 1][0], pioni[pion1[tura-1] + nrp - 1][1] );
				PrintTabla();
	
				if(pioniFinish[pion1[tura-1] + nrp -1] == 1) /*daca pionul a ajuns la finish, devine inactiv, adica nu va mai putea fi mutat*/
				{
					pioni[pion1[tura-1] + nrp - 1][0] = -1;
					pioni[pion1[tura-1] + nrp - 1][1] = -1;	
				}
				if(pioniTerminati[tura] == 4) /*daca jucatorul are 4 pioni terminati--ajunsi la finish -- s-a terminat jocul*/
				{
					gata = 1;
					updateTable(Username, tura, nrjucatori, db);
					GameOver(tura, Jucatori, nrjucatori);					
					printTable(db);
					exit(0);
				}
			}
			  
			nr=RdWrZar(Jucatori[tura], tura);/*daca a fost 6, dam iar cu zarul: daca e 6, ne intoarcem la while, altfel coboram la if*/

		    }
		if(nr != 6)
		{
			if( pioniActivi(tura, Jucatori[tura]) >= 1)
			{
				nrp = RdWrCarePion(Jucatori[tura], tura, nr);
				
				ludo[ pioni[pion1[tura-1] + nrp - 1][0] ][ pioni[pion1[tura-1] + nrp - 1][1] ] = 0;/*mut pionul asa ca trebuie sa las pozitia goala*/
				mutaPion(tura, nr, nrp, &pioni[pion1[tura-1] + nrp - 1][0], &pioni[pion1[tura-1] + nrp - 1][1], Jucatori, nrjucatori);
				RdWrij(Jucatori[tura], pioni[pion1[tura-1] + nrp - 1][0], pioni[pion1[tura-1] + nrp - 1][1] );
				PrintTabla();

				if(pioniFinish[pion1[tura-1] + nrp -1] == 1)
				{
					pioni[pion1[tura-1] + nrp - 1][0] = -1;
					pioni[pion1[tura-1] + nrp - 1][1] = -1;	
				}
				if(pioniTerminati[tura] == 4)
				{
					gata = 1;
					
					updateTable(Username, tura, nrjucatori, db);	
					GameOver(tura, Jucatori, nrjucatori);		
					printTable(db);
					printf("GATA JOCUL\n");
					exit(0);
				}
				
			}
			
		}
		tura++;
		if(tura >nrjucatori) tura=1;
    }
	exit(0);/*pentru a inchide procesul*/
    break;
	
  //default:
    //sunt in parinte
    //wait(NULL);
  }

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
{ /*afisez tabla*/
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
void initiazaPion(int tura, int nrp, int *i, int *j)
{
	int lin = *i;
	int col = *j;
	
	switch(tura)
	{
	case 1:
		switch (nrp)
		{
		case 1:
			ludo[13][6] = 'G';
			ludo[12][4] = 0;
			lin=13;col=6;
			*i=lin; *j=col;
			break;
		case 2:
			ludo[13][6] = 'G';
			ludo[10][4] = 0;
			lin=13;col=6;
			*i=lin; *j=col;
			break;
		case 3:
			ludo[13][6] = 'G';
			ludo[10][2] = 0;
			lin=13;col=6;
			*i=lin; *j=col;
			break;
		case 4:
			ludo[13][6] = 'G';
			ludo[12][2] = 0;
			lin=13;col=6;
			*i=lin; *j=col;
			break;
		}
		break;

	case 2:
		switch (nrp)
		{
		case 1:
			ludo[6][1] = 'V';
			ludo[4][2] = 0;
			lin=6;col=1;
			*i=lin; *j=col;
			break;
		case 2:
			ludo[6][1] = 'V';
			ludo[4][4] = 0;
			lin=6;col=1;
			*i=lin; *j=col;
			break;
		case 3:
			ludo[6][1] = 'V';
			ludo[2][2] = 0;
			lin=6;col=1;
			*i=lin; *j=col;
			break;
		case 4:
			ludo[6][1] = 'V';
			ludo[2][4] = 0;
			lin=6;col=1;
			*i=lin; *j=col;
			break;
		}
		break;
	case 3:
		switch (nrp)
		{
		case 1:
			ludo[1][8] = 'R';
			ludo[2][10] = 0;
			lin=1;col=8;
			*i=lin; *j=col;
			break;
		case 2:
			ludo[1][8] = 'R';
			ludo[4][10] = 0;
			lin=1;col=8;
			*i=lin; *j=col;
			break;
		case 3:
			ludo[1][8] = 'R';
			ludo[2][12] = 0;
			lin=1;col=8;
			*i=lin; *j=col;
			break;
		case 4:
			ludo[1][8] = 'R';
			ludo[4][12] = 0;
			lin=1;col=8;
			*i=lin; *j=col;
			break;
		}
		break;
	case 4:
		switch (nrp)
		{
		case 1:
			ludo[8][13] = 'A';
			ludo[10][12] = 0;
			lin=8;col=13;
			*i=lin; *j=col;
			break;
		case 2:
			ludo[8][13] = 'A';
			ludo[10][10] = 0;
			lin=8;col=13;
			*i=lin; *j=col;
			break;
		case 3:
			ludo[8][13] = 'A';
			ludo[12][10] = 0;
			lin=8;col=13;
			*i=lin; *j=col;
			break;
		case 4:
			ludo[8][13] = 'A';
			ludo[12][12] = 0;
			*i=8; *j=13;
			
			break;
		}
		break;
	
	}

}
void mutaPion(int tura, int nr, int nrp, int *i, int *j, int Jucatori[5], int nrjucatori)
{ /*functia recursiva de mutare de pion: aceasta va intoarce prin i si j la ce pozitie am pus pionul dupa nr mutari*/
	int culoare;
	int lin = *i;
	int col = *j;
	int aux; /* aceasta variabila am luat-o pentru a schimba liniile si coloanele 
	            deoarece, daca fac simplu, ex. lin-- sau col++, mi-ar mai fi intrat recursiv iar pe alte if-uri, astfel, imi garantez unicitatea recursiei*/

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

	if(nr == 0)
	{   if(ludo[lin][col] != 0) /*daca deja in acea pozitie este un pion, il trimite acasa*/
    	{	
		 if(ludo[lin][col] == 'G' && culoare!='G')
		 {
			printf("Inainte era %c, in locul lui vine %c\n", 'G', culoare);
		 	trimiteInapoi(1,lin,col);
		 }
		 else if(ludo[lin][col] == 'V' && culoare!= 'V')
		 {
			printf("Inainte era %c, in locul lui vine %c\n", 'V', culoare);
			trimiteInapoi(2,lin,col);
		 } 	
		 else if(ludo[lin][col] == 'R' && culoare!='R')
		 {
			printf("Inainte era %c, in locul lui vine %c\n", 'R', culoare);
			trimiteInapoi(3,lin,col);
		 }	
		 else if(ludo[lin][col] == 'A' && culoare != 'A')
		 {
			printf("Inainte era %c, in locul lui vine %c\n", 'A', culoare);
			trimiteInapoi(4,lin,col);
		 }
		 	
		}
		
		ludo[lin][col] = culoare; /*am terminat de mutat pionul, il lasa acolo*/	
	}
	else
	{ /*in implementare, am realizat ca liniile si coloanele pe care se muta pionii sunt 6-7-8 si acolo punem conditiile in functie de culoare*/
	
		if(lin == 6)
		{	
			if(col >=1 && col < 5)
			{
				*i = lin;
				aux = col; aux++;
				*j = aux;
				mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
			}
			else if(col == 5) /*pozitia [6][5] trebuie sa faca cotitura la [5][6]*/
			{
				aux = lin; aux--;
				*i = aux;
				aux = col; aux++;
				*j = aux;
				mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
			}
			else if( col >=9 && col<=13)
			{
				if(culoare == 'A')
				{
					if(verificLiber(tura, nr, lin, col) == 1) /*cu o linie inainte de finish pentru fiecare culoare in parte este foarte important sa stim daca putem muta pionul sau nu*/
					{
						if(col == 13)
						{
							aux = lin; aux++;
							*i = aux;
							*j = col;
							mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
						}
						else
						{
							*i = lin;
							aux = col; aux++;
							*j = aux;
							mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
						}		
					}


				}
				else
				{
					if(col == 13)
					{
						aux = lin; aux++;
						*i = aux;
						*j = col;
						mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
					}
					else
					{
						*i = lin;
						aux = col; aux++;
						*j = aux;
						mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
					}	
				}
			}
			
	 	}
		if(col == 6)
		{
			if(lin > 9 && lin <= 13)
			{
				aux = lin; aux--;
				*i = aux;
				*j = col;
      			mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
			}
			else if(lin == 9)/*pozitia [9][6] trebuie sa faca cotitura la [8][5]*/
			{
				aux = lin; aux--;
				*i = aux;
				aux = col; aux--; 
				*j = aux;
				mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
			}
			else if(lin>=1 && lin <=5)
			{
				if(culoare == 'R')
				{
					if(verificLiber(tura, nr, lin, col) == 1)
					{
						if(lin == 1)
						{
							*i = lin;
							aux = col; aux++;
							*j = aux;
							mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
						}
						else
						{
							aux = lin; aux--;
							*i = aux;
							*j = col;
      						mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
						}				
					}
				}
				else
				{
					if(lin == 1)
					{
						*i = lin;
						aux = col; aux++;
						*j = aux;
						mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
					}
					else
					{
						aux = lin; aux--;
						*i = aux;
						*j = col;
      					mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
					}	
				}

			}
			
	 	}
		if(lin == 7) /*reprezinta finish-ul pentru culorile verde si albastru*/
	 	{
			if(col >= 1 && col <= 4)
			{
				if(culoare == 'V')
				{
					
					Finish(tura, nr, nrp, i, j, Jucatori, nrjucatori);/*aici se calculeaza daca pionul poate fi mutat si daca a fost terminat*/
				}
				else
				{
					aux = lin; aux--;
					*i = aux;
					*j = col;
					mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
				}
			}
			if(col <= 13 && col >=10)
			{
				if(culoare == 'A')
				{
					Finish(tura, nr, nrp, i, j, Jucatori, nrjucatori);
				}
				else
				{
					aux = lin; aux++;
					*i = aux;
					*j = col;
					mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
				}
			
			}
		}
		if(col == 7)/*reprezinta finish-ul pentru culorile rosu si galben*/
	 	{
			if(lin >=1 && lin<=4)
			{
				if(culoare == 'R')
				{
					Finish(tura, nr, nrp, i, j, Jucatori, nrjucatori);
				}
				else
				{
					*i = lin;
					aux = col; aux++;
					*j = aux;
					mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
				}
			 
			}
			if(lin<= 13 && lin >=10)
			{
				if(culoare == 'G')
				{
					Finish(tura, nr, nrp, i, j, Jucatori, nrjucatori);
				}
				else
				{
					*i = lin;
					aux = col; aux--;
					*j = aux;
					mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
				}	 
			}
		}
		if(lin == 8)
		{
			if(col>9 && col<=13)
			{
				*i = lin;
				aux = col; aux--;
				*j = aux;
				mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
			}
			else if(col == 9)/*pozitia [8][9] trebuie sa faca cotitura la [9][8]*/
			{
				aux = lin; aux++;
				*i = aux;
				aux = col; aux--;
				*j = aux;
				mutaPion(tura,nr-1,nrp, i, j,Jucatori, nrjucatori);			
			}
			else if(col>=1 && col<=5)
			{
				if(culoare == 'V')
				{
					if(verificLiber(tura, nr, lin, col) == 1)
					{
						if(col == 1)
						{
							aux = lin; aux--;
							*i = aux;
							*j = col;
							mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
						}
						else
						{
							*i = lin;
							aux = col; aux--;
							*j = aux;
							mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
						}
					}
					
				}
				else
				{
					if(col == 1)
					{
						aux = lin; aux--;
						*i = aux;
						*j = col;
						mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
					}
					else
					{
						*i = lin;
						aux = col; aux--;
						*j = aux;
						mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
					}
				}		

			}
			 	
		} 
		if(col == 8)
		{
			if(lin>=1 && lin<5)
			{
				aux = lin; aux++;
				*i = aux;
				*j = col;
				mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);	
			}
			else if (lin == 5)/*pozitia [5][8] trebuie sa faca cotitura la [6][9]*/
			{
				aux = lin; aux++;
				*i = aux;
				aux = col; aux++;
				*j = aux;
				mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
			}
			else if(lin >=9 && lin <= 13)
			{
				if(culoare == 'G')
				{
					if(verificLiber(tura, nr, lin, col) == 1)
					{
						if(lin == 13)
						{
							*i = lin;
							aux = col; aux--;
							*j = aux;
							mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
						}
						else
						{
							aux = lin; aux++;
							*i = aux;
							*j = col;
							mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);			
						}
					}
				}
				else
				{
					if(lin == 13)
					{
						*i = lin;
						aux = col; aux--;
						*j = aux;
						mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);
					}
					else
					{
						aux = lin; aux++;
						*i = aux;
						*j = col;
						mutaPion(tura,nr-1,nrp, i, j, Jucatori, nrjucatori);			
					}
				}

			}
			
			
		}
	}
}
void trimiteInapoi(int tura, int lin, int col)
{ /*daca am ajuns aici inseamna ca un alt pion a ajuns intr-o casuta unde era deja un altul, iar acela trebuie trimis inapoi in baza*/

	int k = 1;

	switch (tura)
	{
	case 1:
		while(pioni[pion1[tura-1] + k- 1][0] != lin && pioni[pion1[tura-1] + k - 1][1] != col )/*ma uit in vectorul cu pioni sa vad ultimele pozitii unde a fost mutat ca sa pot sa il initializez*/
		{
			k++;
		}
		if(k == 1)
			ludo[12][4] = 'G';/*inseamna ca de la pozitia asta a fost scos- tot acolo il trimit*/
		else if(k == 2)
			ludo[10][4] = 'G';
		else if(k == 3)
			ludo[10][2] = 'G';
		else ludo[12][2] = 'G';
		break;
	case 2:
		while(pioni[pion1[tura-1] + k- 1][0] != lin && pioni[pion1[tura-1] + k - 1][1] != col )
		{
			k++;
		}
		if(k == 1)
			ludo[4][2] = 'V';
		else if(k == 2)
			ludo[4][4] = 'V';
		else if(k == 3)
			ludo[2][2] = 'V';
		else ludo[2][4] = 'V';
		break;
	case 3:
		while(pioni[pion1[tura-1] + k- 1][0] != lin && pioni[pion1[tura-1] + k - 1][1] != col )
		{
			k++;
		}
		if(k == 1)
			ludo[2][10] = 'R';
		else if(k == 2)
			ludo[4][10] = 'R';
		else if(k == 3)
			ludo[2][12] = 'R';
		else ludo[4][12] = 'R';
		break;
	case 4:
		while(pioni[pion1[tura-1] + k- 1][0] != lin && pioni[pion1[tura-1] + k - 1][1] != col )
		{
			k++;
		}
		if(k == 1)
			ludo[10][12] = 'A';
		else if(k == 2)
			ludo[10][10] = 'A';
		else if(k == 3)
			ludo[12][10] = 'A';
		else ludo[12][12] = 'A';
		break;
	}
	//indiferent care jucator este, pionul trebuie reinitiat
	pioni[pion1[tura-1] + k- 1][0] = 0;
	pioni[pion1[tura-1] + k- 1][1] = 0;
}


int verificLiber(int tura, int nr, int lin, int col)
{/*aici calculez daca pot continua cu pionul de mutat -- fiind inaintea finish-ului cu cateva pozitii*/
	int liber = 0;

	switch(tura)
	{
	case 1:
		liber = (13 - lin) + (13 - (9 + pioniTerminati[tura])) + 1;
		if(nr <= liber) return 1;
		break;
	case 2:
		liber = (col - 1) + ((5 - pioniTerminati[tura]) -1) + 1;
		if(nr <= liber) return 1;
		break;
	case 3:
		liber = (lin - 1) + ((5 - pioniTerminati[tura]) - 1) + 1;
		if(nr <= liber) return 1;
		break;
	case 4:
		liber = (13 - col) + (13 - (9 + pioniTerminati[tura]) + 1);
		if(nr <= liber) return 1;
		break;
	}

	return 0;
}


void Finish(int tura, int nr, int nrp, int *i, int *j, int Jucatori[5], int nrjucatori)
{
	int liber; /*vom calcula cate pozitii au mai ramas pentru finish*/
	int lin = *i;
	int col = *j;
	switch(tura)
	{
	case 1:
		liber = lin - (9 + pioniTerminati[tura]); /*9 ESTE PRIMA POZITIE UNDE SE PUNE PRIMUL PION TERMINAT*/
		if(nr == liber )/*am atatea pozitii libere cat am pe zar -- deci il mutam si este terminat-- nu-l mai folosim*/
		{
			ludo[lin - nr][col] = 'G';
			pioniTerminati[tura]++;
			pioniFinish[ pion1[tura-1] + nrp - 1 ] = 1;

			lin = lin - nr;
			*i = lin;
			*j = col;

		}
		else if(liber == 0)/*daca s-a intamplat ca pionul sa fie la acea pozitie inainte, dar mai avea nevoie de cateva pozitii pt a fi terminat si un alt pion a ocupat intre timp acea pozitie libera, pionul iar interogat il mutam ca fiind terminat */
		{
			ludo[lin - nr][col] = 'G';
			pioniTerminati[tura]++;
			pioniFinish[ pion1[tura-1] + nrp - 1 ] = 1;
		}
		else if(nr < liber)/*nu mutam*/
		{
			lin = lin - nr;
			ludo[lin][col] = 'G';
			*i = lin;
			*j = col;
		}
		break;
	case 2:
		liber = (5 - pioniTerminati[tura]) - col;
		if(nr == liber)
		{
			ludo[lin][col + nr] = 'V';
			pioniTerminati[tura]++;
			pioniFinish[ pion1[tura-1] + nrp - 1 ] = 1;

			col = col + nr;
			*i = lin;
			*j = col;

		}
		else if(liber == 0)
		{
			ludo[lin][col + nr] = 'V';
			pioniTerminati[tura]++;
			pioniFinish[ pion1[tura-1] + nrp - 1 ] = 1;
		}
		else if(nr < liber)
		{
			col = col + nr;
			ludo[lin][col] = 'V';
			*i = lin;
			*j = col;
		}
		break;
	case 3:
		liber = (5 - pioniTerminati[tura]) - lin;
		if(nr == liber)
		{
			ludo[lin + nr][col] = 'R';
			pioniTerminati[tura]++;
			pioniFinish[ pion1[tura-1] + nrp - 1 ] = 1;

			lin = lin + nr;
			*i = lin;
			*j = col;

		}
		else if(liber == 0)
		{
			ludo[lin + nr][col] = 'R';
			pioniTerminati[tura]++;
			pioniFinish[ pion1[tura-1] + nrp - 1 ] = 1;
		}
		else if( nr < liber)
		{
			lin = lin + nr;
			ludo[lin][col] = 'R';
			*i = lin;
			*j = col;
		}
		break;
	case 4:
		liber = col - (9 + pioniTerminati[tura]);
		if(nr == liber)
		{
			ludo[lin][col - nr] = 'A';
			pioniTerminati[tura]++;
			pioniFinish[ pion1[tura-1] + nrp - 1 ] = 1;

			col = col - nr;
			*i = lin;
			*j = col;

		}
		else if(liber == 0)
		{
			ludo[lin][col - nr] = 'A';
			pioniTerminati[tura]++;
			pioniFinish[ pion1[tura-1] + nrp - 1 ] = 1;
		}
		else if(nr < liber)
		{
			col = col - nr;
			ludo[lin][col] = 'A';
			*i = lin;
			*j = col;
		}
		break;
	}
}

void GameOver( int tura, int Jucatori[5], int nrjucatori)
{/*avem un castigator*/

	int win = 10 + tura;
	switch (tura)
	{
	case 1:
		printf("A castigat GALBEN\n");
		break;
	case 2:
		printf("A castigat VERDE\n");
		break;
	case 3:
		printf("A castigat ROSU\n");
		break;
	case 4:
		printf("A castigat ALBASTRU\n");
		break;
	}

	int i, nr;
	for(i = 1; i <= nrjucatori; i++)/*trimit fiecarui jucatorul castigatorul si il inchid dupa*/
	{
    	if(read(Jucatori[i], &nr, sizeof(int)) <=0)
    	{
     		perror("Eroare read GameOver");
    	}
		if(write(Jucatori[i], &win, sizeof(int)) <= 0)
		{
			perror("Eroare write GameOver");
		}
		close(Jucatori[i]);
	}
	printf("CLASAMENT\n");
	clasament(nrjucatori);/*afisam clasamentul*/
	
	for(i = 1; i<= nrjucatori; i++) /*am golit vectorul de descriptori ca sa nu se intample sa mai trimita vreo informatie cuiva*/
	{
		Jucatori[i] = 0;
	}
}
int RdWrZar(int cl, int tura)
{/*primesc de la client un numar si ii trimit numarul de pe zar*/
	int nr;

	if (read (cl, &nr,sizeof(int)) <= 0)
	{
		perror ("Eroare la read() de la client.\n");
			
	}
	
	printf ("[Jucator %d]Mesajul a fost receptionat...%d\n",tura, nr);
		      
	/*pregatim mesajul de raspuns */
	nr = RollDice();    
	printf("[Jucator %d]Dam cu zarul...%d\n",tura, nr);
		      
		      
	/* returnam mesajul clientului */
	if (write (cl, &nr, sizeof(int)) <= 0)
	{
		
		perror ("[Jucator]Eroare la write() catre client.\n");
	}
	else
	printf ("[Jucator %d]Mesajul a fost trasmis cu succes.\n",tura);	

	return nr;

}
int RdWrCarePion(int cl, int tura, int zar)
{/*primesc de la client ce pion vrea sa mute si ii trimit inapoi numarul ca o confirmare ca se muta--sau eroare cu 5*/
	int nrp;
	int ok=0;
	int var = 5;

	if(zar == 6)
	{
		do
		{
			//daca am un 6, poate alege ce pion continua
			//fie initiat sau nu

			if( read (cl, &nrp, sizeof(nrp)) <= 0)
			{
				perror("Eroare read() client. \n");
			}
			
			if(pioni[pion1[tura-1] + nrp - 1][0] < 0)
			{
				//daca pionul este terminat, nu mai poate fi folosit
				if ( write(cl, &var, sizeof(int)) <= 0)
				{
					perror("Eroare la write() la client.\n");
				}
			}
			else
			{
				if ( write(cl, &nrp, sizeof(int)) <= 0)
				{
					perror("Eroare la write() la client.\n");
				}
				ok = 1;
			}
			
		}while(ok == 0);

		
		
	}
	else
	{
		//daca nu am pe zar 6, atunci trebuie sa continui doar cu un zar deja folosit
		
		do{
			if( read (cl, &nrp, sizeof(nrp)) <= 0)
			{
				perror("Eroare read() client. \n");
			}
			if(pioni[pion1[tura-1] + nrp - 1][0] <= 0)
			{
				if ( write(cl, &var, sizeof(int)) <= 0)
				{
					perror("Eroare la write() la client.\n");
				}
			}
			else
			{
				if ( write(cl, &nrp, sizeof(int)) <= 0)
				{
					perror("Eroare la write() la client.\n");
				}
				ok = 1;
			}

		}while(ok == 0);
	}
	
	return nrp;
}

void RdWrij(int cld, int i, int j)
{/*trimit jucatorului la ce pozitii s-a mutat sa afiseze si el matricea actualizata*/
	int rd;
	
	if(read(cld, &rd, sizeof(int)) <=0)
	{
		perror("Eroare read i din server!");
	}
	if(write(cld, &i, sizeof(int)) <= 0)
	{
		perror("Eroare write i din server!");
	}
	if(read(cld, &rd, sizeof(int)) <=0)
	{
		perror("Eroare read j din server!");
	}
	if(write(cld, &j, sizeof(int)) <= 0)
	{
		perror("Eroare write j din server!");
	}

}


int RollDice()
{
	srand(time(NULL));
	return rand()%6 + 1;
}

int pioniActivi(int tura, int cld)
{/*calculez cati pioni activi are jucatorul ca sa stie daca asteapta pana la un 6 sau poate muta*/
	int nr=0;
	int i;
	int rd;

	for(i=1; i<=4; i++)
	{
		if(pioni[pion1[tura-1] + i - 1][0] > 0)
			nr++;
	}
	if(read(cld, &rd, sizeof(int))<=0)
	{
		perror("Eroare read pioniActivi server");
	}
	if(write(cld, &nr, sizeof(int))<0)
	{
		perror("Eroare write pioniActivi server");
	}
	return nr;
}

void clasament(int nrjucatori)
{
	int top[5], castig[5];
	int aux;

	for(int i=1; i<=nrjucatori;i++)
	{
		top[i] = pioniTerminati[i];
		castig[i] = i;
	}

	for(int i=1; i<nrjucatori;i++)
	{
		for(int j=i; j<=nrjucatori; j++)
		{
			if(top[i]<top[j])
			{
				aux = top[i];
				top[i]=top[j];
				top[j]=aux;
				aux=castig[i];
				castig[i]=castig[j];
				castig[j]=aux;
			}
		}
	}
	for(int i=1; i<=nrjucatori;i++)
	{
		printf("%d. Jucatorul %d cu punctajul: %d\n", i, castig[i], top[i]);
	}
}

void InsertUsername(char Username[20], sqlite3 *db)
{/*inserez username-ul in baza de date*/
	char comanda[1024];
	int bdd;
	char *errmsg = 0;

	strcpy(comanda, "INSERT INTO JUCATORI(USERNAME, SCOR, CASTIG, PIERDERI) VALUES('user', 0, 0, 0);");
	sprintf(comanda, "INSERT INTO JUCATORI(USERNAME, SCOR, CASTIG, PIERDERI) VALUES('%s', 0, 0, 0);", Username);

	bdd = sqlite3_exec(db, comanda, NULL, 0, &errmsg);
   
    if( bdd != SQLITE_OK )
    {
      printf("Jucatorul exista, eroarea de la sql: %s\n", errmsg);
      sqlite3_free(errmsg);
    } 
    else 
    {
      printf("Am inserat cu succes jucatorul nou!\n");
    }
}
void updateTable(char Username[5][20],int castigator, int nrJucatori, sqlite3 *db)
{/*la final de joc, actualizez baza de date*/
	char comanda[1024];
	int bdd;
	char *errmsg = 0;
	int i;
	int s;
	char user[20];
	for(i=1; i<= nrJucatori; i++)
	{
		s=pioniTerminati[i];
		strcpy(user,Username[i]);

		if(i == castigator)
		{
			strcpy(comanda, "UPDATE JUCATORI set SCOR=SCOR+s, CASTIG=CASTIG+1 WHERE USERNAME='user';");
			sprintf(comanda, "UPDATE JUCATORI set SCOR=SCOR+%d, CASTIG=CASTIG+1 WHERE USERNAME='%s';",s, user);

			bdd = sqlite3_exec(db, comanda, NULL, 0, &errmsg);
   
    		if( bdd != SQLITE_OK )
    		{
      			printf("SQL error: %s\n", errmsg);
      			sqlite3_free(errmsg);
    		} 
    		else 
    		{
      			printf("Am actualizat cu succes\n");
    		}
		}
		else
		{
			strcpy(comanda, "UPDATE JUCATORI set SCOR=SCOR+s, PIERDERI=PIERDERI+1 WHERE USERNAME='user';");
			sprintf(comanda, "UPDATE JUCATORI set SCOR=SCOR+%d, PIERDERI=PIERDERI+1 WHERE USERNAME='%s';",s, user);


			bdd = sqlite3_exec(db, comanda, NULL, 0, &errmsg);
   
    		if( bdd != SQLITE_OK )
    		{
      			printf("SQL error: %s\n", errmsg);
      			sqlite3_free(errmsg);
    		} 
    		else 
    		{
      			printf("Am actualizat cu succes\n");
    		}

		}
		bzero(&user, sizeof(user));
	}
	printf("\n");

}

void printTable(sqlite3 *db)
{/*afisez top 3 cei mai buni jucatori din baza de date*/
	char comanda[500];
	int bdd;
	char *errmsg = 0;

	strcpy(comanda , "SELECT * FROM JUCATORI ORDER BY SCOR DESC LIMIT 3;");

    bdd = sqlite3_exec(db, comanda, callback, 0, &errmsg);
   
    if( bdd != SQLITE_OK )
    {
      printf("SQL error: %s\n", errmsg);
      sqlite3_free(errmsg);
    } 
 
}