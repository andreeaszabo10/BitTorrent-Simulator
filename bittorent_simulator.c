#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <mpi.h>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

#define NUM_FILES 1
#define FILENAME 2
#define NUM_CHUNKS 3
#define CHUNKS 4
#define ACK 5
#define TRACKER_REQ 6
#define ALL_DONE 7

// structura de swarm care are numele fisierului si ce clienti detin ceva din el
typedef struct swarm {
	char filename[MAX_FILENAME + 1];
	// pun 1 la pozitia clientilor care au ceva din fisier
	int clients[10];
} swarm;

// structura de fisier cu nume, nr chunk-uri si hash-uri
typedef struct file {
	char filename[MAX_FILENAME + 1];
	int numchunks;
	char hashes[MAX_CHUNKS][HASH_SIZE + 1];
} file;

// trackerul care are lista cu toate fisierele si swarm-urile lor
typedef struct tracker_struct {
	swarm swarms[MAX_FILES];
	file files[MAX_FILES];
} tracker_struct;

// un client are fisierele pe care le detine, le vrea si nr lor
typedef struct client_struct {
	file own_files[MAX_FILES];
	file needed_files[MAX_FILES];
	int numneededfiles, numownedfiles;
} client_struct;

pthread_mutex_t mutex;
tracker_struct track;
int numfiles = 0;
client_struct client;

void *download_thread_func(void *arg)
{
	int rank = *(int*) arg;
	// cate fisiere am downloadat
	int done_files = 0;
	char filename[MAX_FILENAME + 1];

	// cat timp mai am de cerut fisiere
	while (done_files < client.numneededfiles) {
		// trimit un request catre tracker ca sa-mi dea swarm-ul fisierului
		char request[15] = "REQUEST";
        MPI_Send(request, 15, MPI_CHAR, TRACKER_RANK, TRACKER_REQ, MPI_COMM_WORLD);
		strcpy(filename, client.needed_files[done_files].filename);
        MPI_Send(filename, MAX_FILENAME + 1, MPI_CHAR, TRACKER_RANK, 9, MPI_COMM_WORLD);
		
		// primesc de la tracker swarm-ul si informatiile pentru validare(cate chunk-uri are fisierul si hash-urile)
		// clientul are treaba doar cu swarm-ul, hash-urile le folosesc doar la afisare, nu i le dau direct
		int clients_to_ask[10], numchunks;
        MPI_Recv(clients_to_ask, 10, MPI_INT, TRACKER_RANK, 9, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		MPI_Recv(&numchunks, 1, MPI_INT, TRACKER_RANK, 9, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		pthread_mutex_lock(&mutex);

		// actualizez own_files cu noile informatii
		client.numownedfiles++;
		int num = client.numownedfiles - 1;
		strcpy(client.own_files[num].filename, filename);
		client.own_files[num].numchunks = numchunks;

		// Copiază hash-urile primite în structura own_files
		for (int j = 0; j < numchunks; j++) {
			strcpy(client.own_files[num].hashes[j], "0");
		}

		pthread_mutex_unlock(&mutex);

		// initializez hash-urile cu 0, nu am primit nimic pana acum
		char hashes[MAX_CHUNKS][HASH_SIZE + 1];
		for (int a = 0; a < numchunks; a++) {
			MPI_Recv(hashes[a], HASH_SIZE + 1, MPI_CHAR, TRACKER_RANK, 9, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		}

		// vad cati clienti sunt in swarm si le pun rank-ul intr-un vector
		int swarm[10], numinswarm = 0;
		for (int i = 0; i < 10; i++) {
			if (clients_to_ask[i] == 1) {
				swarm[numinswarm] = i;
				numinswarm++;
			}
		}
		
		// variabile ca sa retin nr total de intrebari despre chunk-uri, cate chunk-uri am primit
		// si care a fost ultimul client intrebat din swarm
		int numchunckstried = 0, numchunksgot = 0, lastclient = -1;

		// cat timp mai am de primit chunk-uri din fisier
		while (numchunksgot < numchunks) {
			// iau pe rand chunk-urile
			for (int a = 0; a < numchunks; a++) {
				// daca deja l-am primit sar peste
				if (strcpy(client.needed_files[done_files].hashes[a], "1") == 0) {
					continue;
				}
				if (numchunckstried % 10 == 0 && numchunckstried != 0) {
					// fac un request catre tracker ca mai sus ca sa primesc noul swarm
					strcpy(request, "REQUEST");
					MPI_Send(request, 15, MPI_CHAR, TRACKER_RANK, TRACKER_REQ, MPI_COMM_WORLD);
					MPI_Send(filename, MAX_FILENAME + 1, MPI_CHAR, TRACKER_RANK, 9, MPI_COMM_WORLD);
					
					MPI_Recv(clients_to_ask, 10, MPI_INT, TRACKER_RANK, 9, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
					MPI_Recv(&numchunks, 1, MPI_INT, TRACKER_RANK, 9, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
					for (int a = 0; a < numchunks; a++) {
						MPI_Recv(hashes[a], HASH_SIZE + 1, MPI_CHAR, TRACKER_RANK, 9, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
					}
					
					// actualizez numarul de clienti din swarm si clientii
					numinswarm = 0;
					for (int i = 0; i < 10; i++) {
						if (clients_to_ask[i] == 1) {
							swarm[numinswarm] = i;
							numinswarm++;
						}
					}

				}

				// aleg noul client circular, pentru a implementa round robin
				// daca inainte am cerut ultimului, cer primului, am grija sa nu cer clientului care intreaba
				do {
					lastclient = (lastclient + 1) % numinswarm;
				} while (swarm[lastclient] == rank);

				// am mai intrebat de un chunk
				numchunckstried++;

				// transmit cererea de chunk clientului care vine la rand
				char chunk[15] = "CHUNK", message[3];
				MPI_Send(chunk, 15, MPI_CHAR, swarm[lastclient], 10, MPI_COMM_WORLD);
				MPI_Send(filename, MAX_FILENAME + 1, MPI_CHAR, swarm[lastclient], 11, MPI_COMM_WORLD);
				MPI_Send(&a, 1, MPI_INT, swarm[lastclient], 11, MPI_COMM_WORLD);
				// acesta imi raspunde cu ok daca il are sau no daca nu
				MPI_Recv(message, 3, MPI_CHAR, swarm[lastclient], 8, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				// daca il are, pot sa il primesc
				if (strcmp(message, "OK") == 0) {
					numchunksgot++;
					pthread_mutex_lock(&mutex);
					// marchez ca nu mai am nevoie de acest chunk
					strcpy(client.needed_files[done_files].hashes[a], "1");
					// caut fisierul in lista de fisiere detinute si marchez ca detin chunk-ul primit
					for(int t = 0; t < client.numownedfiles; t++) {
						if (strcmp(client.own_files[t].filename, filename) == 0) {
							strcpy(client.own_files[t].hashes[a], "1");
						}
					}
					pthread_mutex_unlock(&mutex);
				}
			}
		}

		// creez fisierul de output in functie de rank si fisierul cerut
		char out[15] = "client";
		char rank_str[2];
		snprintf(rank_str, sizeof(int), "%d", rank);
		strcat(out, rank_str);
		strcat(out, "_");
		strcat(out, filename);
		strcat(out, "\0");
		// deschid fisierul si scriu hash-urile doar daca le detin ca sa ma asigur
		// ca transferul s-a realizat bine
		FILE *output = fopen(out, "w");
		for (int i = 0; i < numchunks; i++) {
			if (strcmp(client.needed_files[done_files].hashes[i], "1") == 0) {
				fwrite(hashes[i], sizeof(char), HASH_SIZE, output);
        		fwrite("\n", sizeof(char), 1, output);
			}
		}
        fclose(output);
		// cresc numarul de fisiere terminate
		done_files++;
	}

	// trimit trackerului mesaj ca nu mai am nimic de downloadat si inchei executia
	char request[15];
	strcpy(request, "DONE");
	MPI_Send(request, 15, MPI_CHAR, TRACKER_RANK, TRACKER_REQ, MPI_COMM_WORLD);

	return NULL;
}

void *upload_thread_func(void *arg)
{
	while (1) {    
		// primeste un request
		char request[15];
		MPI_Status s;
		MPI_Recv(request, 15, MPI_CHAR, MPI_ANY_SOURCE, 10, MPI_COMM_WORLD, &s);
		int client_rank = s.MPI_SOURCE;

		// daca un client cere un chunk
		if (strcmp(request, "CHUNK") == 0) {
			// vad din ce fisier face parte chunk-ul dorit
			char filename[MAX_FILENAME + 1];
			MPI_Recv(filename, MAX_FILENAME + 1, MPI_CHAR, client_rank, 11, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			// vad care este indexul chunk-ului
			int chunk_index;
			MPI_Recv(&chunk_index, 1, MPI_INT, client_rank, 11, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

			pthread_mutex_lock(&mutex);
			// caut in toate fisierele detinute de client ca sa gasesc fisierul dorit
			for (int i = 0; i < client.numownedfiles; i++) {
				// am gasit fisierul cu numele bun
				if (strcmp(client.own_files[i].filename, filename) == 0) {
					char message[3];
					// daca detin chunk-ul necesar trimit mesajul de confirmare, altfel anunt ca nu-l am
					// orice in afara de "0" inseamna ca am chunk-ul, hash-ul full daca il aveam
					// de la inceput, 1 daca l-am primit pe parcurs
					if (strcmp(client.own_files[i].hashes[chunk_index], "0") != 0) {
						strcpy(message, "OK");
					} else {
						strcpy(message, "NO");
					}
					MPI_Send(message, 3, MPI_CHAR, client_rank, 8, MPI_COMM_WORLD);
				}
			}
			pthread_mutex_unlock(&mutex);
		}

		// am primit de la tracker semnal de stop, opresc executia
		if (strcmp(request, "STOP") == 0) {
			return NULL;
		}
	}
}

void peer(int numtasks, int rank) {

	// construiesc numele fisierului de intrare pentru clientul curent
	char infilename[MAX_FILENAME + 1] = "in";
	char rank_str[2];
	snprintf(rank_str, sizeof(int), "%d", rank);
	strcat(infilename, rank_str);
	strcat(infilename, ".txt");
	strcat(infilename, "\0");

	// deschid fisierul
	FILE *input = fopen(infilename, "r");

	// citesc cate fisiere detine initial si trimit numarul la tracker
	int numfilesowned;
	fscanf(input, "%d", &numfilesowned);
	MPI_Send(&numfilesowned, 1, MPI_INT, TRACKER_RANK, NUM_FILES, MPI_COMM_WORLD);
	client.numownedfiles = numfilesowned;

	// citesc pentru fiecare fisier numele si cate chunk-uri are, apoi le trimit
	for(int i = 0; i < numfilesowned; i++) {
		char filename[MAX_FILENAME + 1];
		fscanf(input, "%s", filename);
		strcpy(client.own_files[i].filename, filename);
		int numchunks;
		fscanf(input, "%d", &numchunks);
		client.own_files[i].numchunks = numchunks;
		MPI_Send(filename, MAX_FILENAME + 1, MPI_CHAR, TRACKER_RANK, FILENAME, MPI_COMM_WORLD);
		MPI_Send(&numchunks, 1, MPI_INT, TRACKER_RANK, NUM_CHUNKS, MPI_COMM_WORLD);

		// pentru fiecare chunk citesc hash-ul si-l trimit
		for(int j = 0; j < numchunks; j++) {
			char hash[HASH_SIZE + 1];
			fscanf(input, "%s", hash);
			hash[HASH_SIZE + 1] = '\0';
			MPI_Send(hash, HASH_SIZE + 1, MPI_CHAR, TRACKER_RANK, CHUNKS, MPI_COMM_WORLD);
			strcpy(client.own_files[i].hashes[j], hash);
		}
	}

	// citesc cate fisiere vrea sa primeasca, apoi numele fisierelor
	// adaug informatiile despre fisierele dorite in structura
	int numfilesneeded;
	fscanf(input, "%d", &numfilesneeded);
	client.numneededfiles = numfilesneeded;
	for(int i = 0; i < numfilesneeded; i++) {
		char filename[MAX_FILENAME + 1];
		fscanf(input, "%s", filename);
		strcpy(client.needed_files[i].filename, filename);
		client.needed_files[i].numchunks = 0;
		// initializez cu "0", nu am niciun hash din fisierele dorite
		for(int j = 0; j < MAX_CHUNKS; j++) {
			strcpy(client.needed_files[i].hashes[j], "0");
		}
	}

	fclose(input);

	// primesc mesajul de la tracker cum ca ar fi terminat de primit fisierele
	char response[4];
	MPI_Recv(response, 4, MPI_CHAR, TRACKER_RANK, ACK, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

	if (strcmp(response, "ACK") != 0) {
		exit(-1);
	}

	pthread_mutex_init(&mutex, NULL);

	pthread_t download_thread;
	pthread_t upload_thread;
	void *status;
	int r;

	r = pthread_create(&download_thread, NULL, download_thread_func, (void *) &rank);
	if (r) {
		printf("Eroare la crearea thread-ului de download\n");
		exit(-1);
	}

	r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) &rank);
	if (r) {
		printf("Eroare la crearea thread-ului de upload\n");
		exit(-1);
	}

	r = pthread_join(download_thread, &status);
	if (r) {
		printf("Eroare la asteptarea thread-ului de download\n");
		exit(-1);
	}

	r = pthread_join(upload_thread, &status);
	if (r) {
		printf("Eroare la asteptarea thread-ului de upload\n");
		exit(-1);
	}

	pthread_mutex_destroy(&mutex);
}

void put_tracker_files(int numtasks, int rank) {

	// pentru fiecare client astept informatiile despre fisiere
	for(int i = 1; i < numtasks; i++) {
		// primesc cate fisiere are un client, care e mai rapid trimite primul
		int numfilesowned;
		MPI_Status s;
		MPI_Recv(&numfilesowned, 1, MPI_INT, MPI_ANY_SOURCE, NUM_FILES, MPI_COMM_WORLD, &s);
		// verific de la care client am primit informatia si sunt atenta sa
		// primesc in continuare de la acelasi
		int client_rank = s.MPI_SOURCE;

		for(int j = 0; j < numfilesowned; j++) {
			// primeste numele fisierului
			char filename[MAX_FILENAME + 1];
			MPI_Recv(filename, MAX_FILENAME + 1, MPI_CHAR, client_rank, FILENAME, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

			// daca mai exista deja fisierul in lista, doar adaug clientul la swarm
			int ok = 1;
			for (int a = 0; a < numfiles; a++) {
				if (strcmp(track.swarms[a].filename, filename) == 0){
					track.swarms[a].clients[client_rank] = 1;
					ok = 0;
				}
			}
			// altfel adaug fisierul in swarm si apoi clientul care-l detine
			if (ok == 1) {
				strcpy(track.swarms[numfiles].filename, filename);
				track.swarms[numfiles].clients[client_rank] = 1;
			}
			
			// primeste cate chunk-uri are din fisier
			int numchunks;
			MPI_Recv(&numchunks, 1, MPI_INT, client_rank, NUM_CHUNKS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

			// completez trackerul cu numele fisierului si cate chunk-uri are
			strcpy(track.files[numfiles].filename, filename);
			track.files[numfiles].numchunks = numchunks;

			// iau fiecare hash si il pun in lista tracker-ului
			for(int k = 0; k < numchunks; k++) {
				char hash[HASH_SIZE + 1];
				MPI_Recv(hash, HASH_SIZE + 1, MPI_CHAR, client_rank, CHUNKS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				strcpy(track.files[numfiles].hashes[k], hash);
			}
			numfiles++;
		}
	}

	// pntru fiecare client trimit un mesaj ca am primit fisierele
	char response[4] = "ACK";
	for(int i = 0; i < numtasks - 1; i++) {
		MPI_Send(response, 4, MPI_CHAR, i + 1, ACK, MPI_COMM_WORLD);
	}

}

void tracker(int numtasks, int rank) {

	// scriu lista de fisiere a trackerului si fac swarm-urile initiale
	put_tracker_files(numtasks, rank);

	int clients_done = 0;

	while(1) {
		char request[15];
		MPI_Status s;
		MPI_Recv(request, 15, MPI_CHAR, MPI_ANY_SOURCE, TRACKER_REQ, MPI_COMM_WORLD, &s);
		int client_rank = s.MPI_SOURCE;
		// daca am un request de la un client
		if (strcmp(request, "REQUEST") == 0) {
			// primeste numele fisierului pe care il vrea clientul
			char filename[MAX_FILENAME + 1];
			MPI_Recv(filename, MAX_FILENAME + 1, MPI_CHAR, client_rank, 9, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

			for(int i = 0; i < numfiles; i++) {
				if (strcmp(track.swarms[i].filename, filename) == 0) {
					// trimite swarm-ul catre clientul care l-a cerut, apoi numarul de chunk-uri
					// si hash-urile pentru verificare
					MPI_Send(track.swarms[i].clients, 10, MPI_INT, client_rank, 9, MPI_COMM_WORLD);
					MPI_Send(&track.files[i].numchunks, 1, MPI_INT, client_rank, 9, MPI_COMM_WORLD);
					for (int a = 0; a < track.files[i].numchunks; a++) {
						MPI_Send(track.files[i].hashes[a], HASH_SIZE + 1, MPI_CHAR, client_rank, 9, MPI_COMM_WORLD);
					}
					// actualizeaza swarm-ul cu noul client
					track.swarms[i].clients[client_rank] = 1;
					break;
				}
			}
		}

		// daca am primit mesaj ca un client a terminat tot ce avea de descarcat
		if (strcmp(request, "DONE") == 0) {
			// cresc nr de clienti care au terminat
			clients_done++;
			// daca au terminat toti trimite catre tracker un mesaj ca sa opreasca tot
			if (clients_done == numtasks - 1) {
				char message[15];
				strcpy(message, "ALL_DONE");
				MPI_Send(message, 15, MPI_CHAR, TRACKER_RANK, TRACKER_REQ, MPI_COMM_WORLD);
			}
		}
		// au terminat toti clientii, trimit semnal catre thread-urile upload sa se inchida
		if (strcmp(request, "ALL_DONE") == 0) {
			for(int i = 1; i < numtasks; i++) {
				char message[15] = "STOP";
				MPI_Send(message, 15, MPI_CHAR, i, 10, MPI_COMM_WORLD);
			}
			break;
		}
	}
}
 
int main (int argc, char *argv[]) {
	int numtasks, rank;
 
	int provided;
	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
	if (provided < MPI_THREAD_MULTIPLE) {
		fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
		exit(-1);
	}
	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	if (rank == TRACKER_RANK) {
		tracker(numtasks, rank);
	} else {
		peer(numtasks, rank);
	}

	MPI_Finalize();
}