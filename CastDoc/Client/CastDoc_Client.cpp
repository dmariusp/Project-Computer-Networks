/* cliTCP.c - Exemplu de client TCP
   Trimite un nume la server; primeste de la server "Hello nume".

   Autor: Lenuta Alboaie  <adria@infoiasi.ro> (c)2009

*/
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>


/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* portul de conectare la server*/
int port;


int fileWrite(int serverfd, void* buffer, size_t file_length)
{
    if(buffer != nullptr)
    {
        int filelen;
        size_t len1 = sizeof(size_t);
        while(len1 > 0)
        {
            filelen = write(serverfd, &file_length, sizeof(size_t));
            if (filelen<=0)
            {
                perror("write() error!\n");
                return -1;
            }
            len1 = len1 - filelen;
        }
        auto len2 = ssize_t(file_length);
        while(len2 > 0)
        {
            filelen = write(serverfd,(char*)buffer,file_length);
            if (filelen <= 0)
            {
                perror("write() error!\n");
                return -1;
            }
            len2 = len2 - filelen;
        }
    }
    return 0;
}

ssize_t fileRead(int serverfd, void* &buffer)
{
    size_t file_length;
    int filelen;
    size_t len = sizeof(size_t);

    while(len > 0)
    {
        filelen = read(serverfd, &file_length, sizeof(size_t));
        if(filelen <= 0)
        {
            if (filelen == 0)
                return -1;
            else
            {
                perror("read() error!\n");
                return -1;
            }
        }
        len = len - filelen;
    }

    if(buffer == nullptr)
        buffer = malloc(file_length);

    ssize_t final = file_length;
    while(file_length > 0)
    {
        filelen = read(serverfd, (char*)buffer, file_length);
        if(filelen <= 0)
        {
            if (filelen == 0)
                return -1;
            else
            {
                perror("read() error!\n");
                return -1;
            }
        }
        file_length = file_length - filelen;
    }
    return final;
}

int fileReceiver(int serverfd, char* fpath)
{
    char* buffer = (char*)malloc(1025);
    size_t fsize;

    void* pointer = &fsize;
    if (fileRead(serverfd, pointer) == -1)
        return -1;
    int filedescriptor;
    if ((filedescriptor = open(fpath, O_RDWR | O_CREAT | O_TRUNC, 0600)) == -1)
        return -1;
    ssize_t unsent = fsize;
    ssize_t length;

    pointer = buffer;
    while ((unsent > 0) and ((length = fileRead(serverfd,pointer)) >= 0)){
        buffer = (char*) pointer;
        write(filedescriptor, buffer, length);
        unsent = unsent - length;
        pointer = buffer;
    }
    if(length < 0){
        free(buffer);
        return -1;
    }
    free(buffer);
    close(filedescriptor);
    return 0;
}

int fileSender(int serverfd, char* fpath)
{
    int filedescriptor;
    size_t bytes;
    char buffer[1025];
    size_t fsize;
    struct stat fstat;

    if (stat(fpath, &fstat) < 0)
    {
        fprintf(stderr, "Error at fstat, cause is: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    fsize = (size_t)fstat.st_size;
    if(fileWrite(serverfd, &fsize, sizeof(fsize)) == -1)
        return -1;

    if((filedescriptor = open(fpath, O_RDONLY)) == -1)
        return -1;

    ssize_t unsent = fsize;
    while (((bytes = read(filedescriptor,buffer,1025)) > 0) && (unsent > 0))
    {
        if(fileWrite(serverfd,buffer,bytes) == -1)
            return -1;
        unsent = unsent - bytes;
    }
    close(filedescriptor);
    return 0;
}

int main (int argc, char *argv[])
{
    int sd;			// descriptorul de socket
    struct sockaddr_in server;	// structura folosita pentru conectare

    /* exista toate argumentele in linia de comanda? */
    if (argc != 3)
    {
        printf ("[client] Sintaxa: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }

    /* stabilim portul */
    port = atoi (argv[2]);

    /* cream socketul */
    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror ("[client] Eroare la socket().\n");
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
    char msg1[100];
    size_t msglen1;
    char msg2[100];
    size_t msglen2;

    /* citirea mesajului */
    bzero (msg1, 100);
    printf ("[client]Introduceti tipul fisierului de intrare: ");
    fflush (stdout);
    read (0, msg1, 100);
    msg1[strlen(msg1)-1] = 0;
    msglen1 = strlen(msg1);

    bzero(msg2,100);
    printf ("[client]Introduceti tipul fisierului de iesire: ");
    fflush (stdout);
    read (0,msg2,100);
    msg2[strlen(msg2)-1] = 0;
    msglen2 = strlen(msg2);

    char msgrasp[10];
    size_t msgrasp_len;
    bzero(msgrasp,10);
    /* trimiterea mesajului la server */
    if (write (sd, &msglen1, sizeof(msglen1)) <= 0 )
    {
        perror ("[client]Eroare la write() spre server.\n");
        return errno;
    }
    if(write (sd, msg1, msglen1) <= 0)
    {
        perror ("[client]Eroare la write() spre server.\n");
        return errno;
    }
    if (write (sd, &msglen2, sizeof(msglen2)) <= 0 )
    {
        perror ("[client]Eroare la write() spre server.\n");
        return errno;
    }
    if(write (sd, msg2, msglen2) <= 0)
    {
        perror ("[client]Eroare la write() spre server.\n");
        return errno;
    }


    /* citirea raspunsului dat de server
       (apel blocant pana cand serverul raspunde) */
    if (read (sd, &msgrasp_len, sizeof(size_t)) < 0)
    {
        perror ("[client]Eroare la read() de la server.\n");
        return errno;
    }

    if (read (sd, msgrasp, msgrasp_len) < 0)
    {
        perror ("[client]Eroare la read() de la server.\n");
        return errno;
    }
    msgrasp[msgrasp_len] = 0;
    while(strcmp(msgrasp,"NU") == 0)
    {
        printf("Nu se poate realiza conversia %s -> %s!\n",msg1,msg2);

        bzero (msg1, 100);
        printf ("[client]Introduceti tipul fisierului de intrare: ");
        fflush (stdout);
        read (0, msg1, 100);
        msg1[strlen(msg1)-1] = 0;
        msglen1 = strlen(msg1);

        bzero(msg2,100);
        printf ("[client]Introduceti tipul fisierului de iesire: ");
        fflush (stdout);
        read (0,msg2,100);
        msg2[strlen(msg2)-1] = 0;
        msglen2 = strlen(msg2);
        /* trimiterea mesajului la server */

        bzero(msgrasp,10);
        /* trimiterea mesajului la server */
        if (write (sd, &msglen1, sizeof(msglen1)) <= 0 )
        {
            perror ("[client]Eroare la write() spre server.\n");
            return errno;
        }
        if(write (sd, msg1, msglen1) <= 0)
        {
            perror ("[client]Eroare la write() spre server.\n");
            return errno;
        }
        if (write (sd, &msglen2, sizeof(msglen2)) <= 0 )
        {
            perror ("[client]Eroare la write() spre server.\n");
            return errno;
        }
        if(write (sd, msg2, msglen2) <= 0)
        {
            perror ("[client]Eroare la write() spre server.\n");
            return errno;
        }

        /* citirea raspunsului dat de server
           (apel blocant pana cand serverul raspunde) */
        if (read (sd, &msgrasp_len, sizeof(size_t)) < 0)
        {
            perror ("[client]Eroare la read() de la server.\n");
            return errno;
        }

        if (read (sd, msgrasp, msgrasp_len) < 0)
        {
            perror ("[client]Eroare la read() de la server.\n");
            return errno;
        }
        msgrasp[msgrasp_len] = 0;
    }
    printf("Se poate realiza conversia %s -> %s!\n",msg1,msg2);

    /*trimiterea fisierului*/

    char fpath[200];
    char fname[50];
    bzero(fpath, sizeof(fpath));
    printf("[client]Introduceti fisierul de convertit: ");
    fflush(stdout);
    read(0, fpath, sizeof(fpath));
    fpath[strlen(fpath)-1] = 0;
    strcpy(fname,basename(fpath));
    printf("%s,    %s\n",fpath,fname);
    size_t fname_len = strlen(fname);
    write(sd,&fname_len,sizeof(fname_len));
    write(sd,fname,fname_len);
    fileSender(sd,fname);

    char fname_out[200];
    size_t fname_out_len;
    read(sd,&fname_out_len,sizeof(size_t));
    read(sd,fname_out,fname_out_len);
    printf("%s\n",fname_out);
    fname_out[fname_out_len]=0;
    fileReceiver(sd,fname_out);


    /* inchidem conexiunea, am terminat */
    close (sd);
}
