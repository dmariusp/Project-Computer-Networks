/* servTCPCSel.c - Exemplu de server TCP concurent

   Asteapta un "nume" de la clienti multipli si intoarce clientilor sirul
   "Hello nume" corespunzator; multiplexarea intrarilor se realizeaza cu select().

   Cod sursa preluat din [Retele de Calculatoare,S.Buraga & G.Ciobanu, 2003] si modificat de
   Lenuta Alboaie  <adria@infoiasi.ro> (c)2009

*/
#include "nlohmann/json.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <fstream>
#include <openssl/sha.h>
#include <unordered_map>
#include <utility>
#include <typeinfo>


/* portul folosit */

#define PORT 2728

using json = nlohmann::json;
using namespace std;

unordered_map<string,string> cache;

json config;
extern int errno;		/* eroarea returnata de unele apeluri */

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

void Conversion(char* command, size_t command_len, char* fname_in, char* fname_out) {

    char command_user[100];
    strcpy(command_user,command);

    //command_user[command_len - 1] = 0;
    printf("%s\n", command_user);

    char *cmd_argv[15];
    int cmd_argc = 0;
    for (int i = 0; i < 15; i++)
        cmd_argv[i] = static_cast<char *>(malloc(50 * sizeof(char))); //we dynamically assign space for each parameter of the command
    char *cmd_arg = strtok(command_user, " ");
    while (cmd_arg != NULL) {
        strcpy(cmd_argv[cmd_argc], cmd_arg);
        if(strcmp(cmd_argv[cmd_argc],"$in")==0)
        {
            strcpy(cmd_argv[cmd_argc],fname_in);
        }
        if(strcmp(cmd_argv[cmd_argc],"$out")==0)
        {
            strcpy(cmd_argv[cmd_argc],fname_out);
        }
        cmd_argc++;
        cmd_arg = strtok(NULL, " ");
    }
    cmd_argv[cmd_argc] = cmd_arg;

    execvp(cmd_argv[0], cmd_argv);
    exit(0);
}

int fileWrite(int clientfd, void* buffer, size_t file_length)
{
    if(buffer != nullptr)
    {
        int filelen;
        size_t len1 = sizeof(size_t);
        while(len1 > 0)
        {
            filelen = write(clientfd, &file_length, sizeof(size_t));
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
            filelen = write(clientfd,(char*)buffer,file_length);
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

ssize_t fileRead(int clientfd, void* &buffer)
{
    size_t file_length;
    int filelen;
    size_t len = sizeof(size_t);

    while(len > 0)
    {
        filelen = read(clientfd, &file_length, sizeof(size_t));
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
        filelen = read(clientfd, (char*)buffer, file_length);
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

int fileReceiver(int clientfd, char* fpath)
{
    char* buffer = (char*)malloc(1025);
    size_t fsize;

    void* pointer = &fsize;
    if (fileRead(clientfd, pointer) == -1)
        return -1;
    int filedescriptor;
    if ((filedescriptor = open(fpath, O_RDWR | O_CREAT | O_TRUNC, 0600)) == -1)
        return -1;

    ssize_t unsent = fsize;
    ssize_t length;

    pointer = buffer;
    while ((unsent > 0) and ((length = fileRead(clientfd,pointer)) >= 0)){
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

int fileSender(int clientfd, char* fpath)
{
    int filedescriptor;
    size_t bytes;
    char buffer[1025];
    size_t fsize;
    struct stat fstat;

    if (stat(fpath, &fstat) < 0)
    {
        fprintf(stderr, "Error fstat --> %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    fsize = (size_t)fstat.st_size;
    if(fileWrite(clientfd, &fsize, sizeof(fsize)) == -1)
        return -1;

    if((filedescriptor = open(fpath, O_RDONLY)) == -1)
        return -1;

    ssize_t unsent = fsize;
    // loop preluat si adaptat specific codului meu de la https://stackoverflow.com/a/11965442
    while (((bytes = read(filedescriptor,buffer,1025)) > 0) && (unsent > 0))
    {
        if(fileWrite(clientfd,buffer,bytes) == -1)
            return -1;
        unsent = unsent - bytes;
    }
    close(filedescriptor);
    return 0;
}


//algoritm SHA1 adaptat dupa acest model: https://stackoverflow.com/a/3467645
int getFileHashCode(char* fname, unsigned char* out_pointer)
{
    unsigned char buffer[8192];
    FILE *fd;
    SHA_CTX sha_c;
    int error;
    fd = fopen(fname, "rb");
    if (fd == NULL) {
        return -1;
    }
    SHA1_Init(&sha_c);
    for (;;) {
        size_t length;

        length = fread(buffer, 1, sizeof buffer, fd);
        if (length == 0)
            break;
        SHA1_Update(&sha_c, buffer, length);
    }
    error = ferror(fd);
    fclose(fd);
    if (error) {
        /* some I/O error was encountered; report the error */
        return -1;
    }
    SHA1_Final(out_pointer, &sha_c);
    return 0;

//    if (!fname) {
//        return NULL;
//    }
//
//    FILE *fp = fopen(fname, "rb");
//    if (fp == NULL) {
//        return NULL;
//    }
//
//    unsigned char* sha1_digest = (unsigned char*)(malloc(sizeof(char)*SHA_DIGEST_LENGTH));
//    SHA_CTX context;
//
//    if(!SHA1_Init(&context))
//        return NULL;
//
//    unsigned char buf[1024*16];
//    while (!feof(fp))
//    {
//        size_t total_read = fread(buf, 1, sizeof(buf), fp);
//        if(!SHA1_Update(&context, buf, total_read))
//        {
//            return NULL;
//        }
//    }
//    fclose(fp);
//
//    if(!SHA1_Final(sha1_digest, &context))
//        return NULL;
//
//    return sha1_digest;
}

void conversionPrep(char* fname, char* fname_out, int index_conversie, int fd)
{
    string conversion_command_str;
    size_t conversionlen;
    string nume_conversie = config[index_conversie]["nume_conversie"];
    printf("[server]Se realizeaza conversia: %s\n", nume_conversie.c_str());

    conversion_command_str = config[index_conversie]["comanda"];
    char conversion_command[200];
    bzero(conversion_command,200);
    strcpy(conversion_command,conversion_command_str.c_str());
    conversionlen = strlen(conversion_command);
    size_t fname_out_len;
    fname_out_len=strlen(fname_out);

    if(fork() == 0)
    {
        Conversion(conversion_command,conversionlen,fname,fname_out);
    }
    else
        wait(NULL);

    write(fd,&fname_out_len,sizeof(fname_out_len));
    write(fd,fname_out,fname_out_len);

    fileSender(fd,fname_out);
}

/* realizeaza primirea si retrimiterea unui mesaj unui client */
int conversionsResponse(int fd) {
    // char buffer_conversion[100];
    // FILE *cd; //conversion descriptor


    bool conversion = false;
    //char buffer[100];        /* mesajul */
    int bytes1;
    int bytes2;
    char msg1[100];        //mesajul primit de la client
    char msg2[100];
    size_t msglen1;
    size_t msglen2;
    char msgrasp[100] = "";        //mesaj de raspuns pentru client
    int ok=1;
    int index_conversie;


    while(ok)
    {
        bzero(msg1,100);
        bzero(msg2,100);
        //cd = fopen("conversions.txt", "r");

        read(fd, &msglen1, sizeof(size_t));
        printf("%d\n", msglen1);
        bytes1 = read(fd, msg1, msglen1);
        msg1[msglen1] = 0;
        printf("%s\n", msg1);

        read(fd,&msglen2,sizeof(size_t));
        printf("%d\n", msglen2);
        bytes2 = read(fd,msg2, msglen2);
        msg2[msglen2] = 0;
        printf("%s\n", msg2);

        if (bytes1 < 0) {
            perror("Eroare la read() de la client.\n");
            return 0;
        }
        if(bytes2 < 0)
        {
            perror("Eroare la read() de la client.\n");
            return 0;
        }

        printf("[server]Formatul de intrare receptionat este: %s\n", msg1);

        printf("[server]Formatul de iesire receptionat este: %s\n", msg2);

        string format_in = msg1;
        string format_out = msg2;

        printf("%s,%s\n",format_in.c_str(),format_out.c_str());
        //printf("mesaj de proba\n");
        for(int i=0; i<config.size();i++)
        {
            // printf("vibe check\n");
            if(config[i]["tip_intrare"] == format_in && config[i]["tip_iesire"] == format_out)
            {
                conversion = true;
                index_conversie = i;
                break;
            }
        }

//        while (fgets(buffer_conversion, sizeof(buffer_conversion), cd)) {
//            buffer_conversion[strlen(buffer_conversion) - 1] = 0;
//            if (strcmp(buffer_conversion, msg1) == 0) {
//                conversion = true;
//            }
//        }
//        fclose(cd);
//        /*pregatim mesajul de raspuns */
        bzero(msgrasp, 100);
        if (conversion) {
            strcpy(msgrasp, "DA");
            ok = 0;
        } else {
            strcpy(msgrasp, "NU");
        }

        size_t msgrasp_len;
        msgrasp_len = strlen(msgrasp);

        if (write (fd, &msgrasp_len, sizeof(msgrasp_len)) < 0)
        {
            perror ("[server] Eroare la write() catre client.\n");
            return 0;
        }
        if (write (fd, msgrasp, msgrasp_len) < 0)
        {
            perror ("[server] Eroare la write() catre client.\n");
            return 0;
        }
    }

    char fname[200];
    size_t fsize;
    bzero(fname, sizeof(fname));
    read(fd, &fsize, sizeof(size_t));
    read(fd, fname, fsize);
    printf("%s\n",fname);

    fileReceiver(fd,fname);

/*    if(cache.size()==0)
    {
        printf("peste\n");
        char fname_out[200];

        strcpy(fname_out,fname);
        char* punct_pointer =strchr(fname_out,'.');
        string extensie_out = config[index_conversie]["tip_iesire"];
        strcpy(punct_pointer+1,extensie_out.c_str());
        printf("%s\n",fname_out);

        unsigned char* pointer = (unsigned char*)(malloc(200*sizeof(unsigned char)));
        pointer = getFileHashCode(fname);
        getFileHashCode(fname, pointer);
        string hash_code(reinterpret_cast<char*>(pointer));
        string string_fname_out = fname_out;

        size_t i = 0;
        char bufferTest[50];
        for(i = 0; i < SHA_DIGEST_LENGTH; ++i)
        {
            sprintf(bufferTest, "%02X", pointer[i]);
        }
        string hash_code = bufferTest;
        cache[hash_code] = string_fname_out;
        conversionPrep(fname,fname_out,index_conversie,fd);
    } else
    {
        unsigned char* pointer = (unsigned char*)(malloc(200*sizeof(unsigned char)));
        pointer = getFileHashCode(fname);
        getFileHashCode(fname, pointer);
        string hash_code(reinterpret_cast<char*>(pointer));

        size_t i = 0;
        char bufferTest[50];
        for(i = 0; i < SHA_DIGEST_LENGTH; ++i)
        {
            sprintf(bufferTest, "%02X", pointer[i]);
        }
        string hash_code = bufferTest;

        for(auto i : cache)
        {
            if(i.first == hash_code)
            {
                printf("crima necurata\n");
                char fname_out[200];
                strcpy(fname_out,i.second.c_str());
                printf("%s\n",fname_out);

                size_t fname_out_len;
                fname_out_len=strlen(fname_out);

                write(fd,&fname_out_len,sizeof(fname_out_len));
                write(fd,fname_out,fname_out_len);

                fileSender(fd,fname_out);
                return 1;
            }

        }
        printf("prostia omeneasca\n");
        char fname_out[200];

        strcpy(fname_out,fname);
        char* punct_pointer =strchr(fname_out,'.');
        string extensie_out = config[index_conversie]["tip_iesire"];
        strcpy(punct_pointer+1,extensie_out.c_str());
        printf("%s\n",fname_out);

        string string_fname_out = fname_out;
        cache[hash_code] = string_fname_out;
        conversionPrep(fname,fname_out,index_conversie,fd);
    }
*/
    char fname_out[200];

    strcpy(fname_out,fname);
    char* punct_pointer =strchr(fname_out,'.');
    string extensie_out = config[index_conversie]["tip_iesire"];
    strcpy(punct_pointer+1,extensie_out.c_str());
    printf("%s\n",fname_out);
    conversionPrep(fname,fname_out,index_conversie,fd);

    return 1;
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
    int fd;			/* descriptor folosit pentru
				   parcurgerea listelor de descriptori */
    int nfds;			/* numarul maxim de descriptori */
    int len;			/* lungimea structurii sockaddr_in */

    ifstream f("config.json");
    f>>config;
    f.close();

    char tip_in[10];
    char tip_out[10];
    char comanda[100];

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
    if (listen (sd, 5) == -1)
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
            client = accept(sd, (struct sockaddr *) &from, reinterpret_cast<socklen_t *>(&len));
            /* eroare la acceptarea conexiunii de la un client */
            if (client < 0)
            {
                perror ("[server] Eroare la accept().\n");
                continue;
            }

            if (nfds < client) /* ajusteaza valoarea maximului */
                nfds = client;

            /* includem in lista de descriptori activi si acest socket */
            FD_SET (client, &actfds);

            printf("[server] S-a conectat clientul cu descriptorul %d, de la adresa %s.\n",client, conv_addr (from));
            fflush (stdout);
        }

        /* vedem daca e pregatit vreun socket client pentru a trimite raspunsul */
        for (fd = 0; fd <= nfds; fd++)	/* parcurgem multimea de descriptori */
        {
            /* este un socket de citire pregatit? */
            if (fd != sd && FD_ISSET (fd, &readfds))
            {
                if (conversionsResponse(fd))
                {
                    printf ("[server] S-a deconectat clientul cu descriptorul %d.\n",fd);
                    fflush (stdout);
                    close (fd);		/* inchidem conexiunea cu clientul */
                    FD_CLR (fd, &actfds);/* scoatem si din multime */

                }
            }
        }			/* for */
    }				/* while */
}				/* main */
