#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>

#define PORT "9032"

int SendingFile(int listener, char path[]);
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa);

int main(void)
{
    // master file descriptor list
    fd_set master;
    // temp file descriptor list for select()
    fd_set read_fds;  
    // maximum file descriptor number
    int fdmax;        
    // listening socket descriptor
    int listener;
    // newly accept()ed socket descriptor
    int newfd;
    // client address
    struct sockaddr_storage remoteaddr; 
    socklen_t addrlen;

    // buffer for client data
    char buf[256];    
    int nbytes;

	char remoteIP[INET6_ADDRSTRLEN];

    // for setsockopt() SO_REUSEADDR, below
    int yes=1;        
    int i, j, rv;

	struct addrinfo hints, *ai, *p;

    // clear the master and temp sets
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

	/*Since the members of a struct (like addrinfo) are not initialized in any way when declaring a variable of that type,
     hints originally contains "garbage" -- whatever happed to be in memory where the variable is allocated. 
     Thus the code calls memset to zero out all members in an easy/fast way 
     (as opposed to setting the member variables to zero one-by-one).*/
	memset(&hints, 0, sizeof hints);
    //add necessary info to hints
	hints.ai_family = AF_UNSPEC; //use IPv4 or IPv6, whichever
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; //fill in my ip for me

    
    /*The getaddrinfo() function allocates and initializes a linked list of addrinfo structures,
    one for each network address that matches node and service, subject to any restrictions imposed by hints,
     and returns a pointer to the start of the list in res.*/

    //The gai_strerror() function shall return a text string describing an error value for the getaddrinfo()
	if((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0){
		fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
		exit(1);
	}
	
    //loop through all and bind to first we can
	for(p = ai; p != NULL; p = p->ai_next){
        //create socket
    	listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if(listener < 0)
			continue;
		
		//lose the pesky "address already in use" error message
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        //bind
		if(bind(listener, p->ai_addr, p->ai_addrlen) < 0){
			close(listener);
			continue;
		}

		break;
	}

	//if we got here, it means we didn't get bound
	if(p == NULL){
		fprintf(stderr, "selectserver: failed to bind\n");
		exit(2);
	}

    //free linked list
	freeaddrinfo(ai); 

    //listen
    if(listen(listener, 10) == -1){
        perror("listen");
        exit(3);
    }

    //add the listener to the master set
    FD_SET(listener, &master);

    //keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    //main loop
    for(;;){
        read_fds = master; //copy it 
        //first param of select is max value of descriptor+1
        if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1){
            perror("select");
            exit(4);
        }

        //run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++){
            //check if file descriptor is in set
            if(FD_ISSET(i, &read_fds)){
                if(i == listener){
                    //handle new connections
                    addrlen = sizeof remoteaddr;
					newfd = accept(listener,
						(struct sockaddr *)&remoteaddr,
						&addrlen);

					if (newfd == -1)
                        perror("accept");
                    else{
                        //add to master set
                        FD_SET(newfd, &master); 
                        //keep track of the max
                        if(newfd > fdmax)
                            fdmax = newfd;

                        //inet_ntop - convert IPv4 and IPv6 addresses from binary to text form
                        printf("[+]Connection from %s\n", 
                            inet_ntop(remoteaddr.ss_family,get_in_addr((struct sockaddr*)&remoteaddr),
								remoteIP, INET6_ADDRSTRLEN));
                    }
                } 
                else {
                    //handle data from a client
                    if((nbytes = recv(i, buf, sizeof buf, 0)) <= 0){
                        // got error or connection closed by client
                        if (nbytes == 0) 
                            printf("[+]Socket hung up\n"); // connection closed
                        else 
                            perror("recv");
                        
                        close(i); //close
                        FD_CLR(i, &master); //remove from master set
                    } 
                    else{
                        buf[nbytes] = '\0';
                        if(buf[0] == '/'){
                            for(j = 0; j <= fdmax; j++) {
                                //send to everyone
                                if (FD_ISSET(j, &master)) {
                                    //except the listener and ourselves
                                    if (j != listener && j != i)
                                        if(SendingFile(j, buf) == 1)
                                            perror("send");
                                        else
                                            printf("File sent successfuly\n");
                                }
                            }
                        }
                        else{
                            for(j = 0; j <= fdmax; j++){
                                //send to everyone
                                if(FD_ISSET(j, &master)){
                                    //except the listener and ourselves
                                    if(j != listener && j != i) {
                                        if(send(j, buf, nbytes, 0) == -1){
                                            perror("send");
                                        }
                                    }
                                }
                            }
                        }                        
                    }
                }
            } 
        } 
    } 
    return 0;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    //checking if it is IPv4
	if(sa->sa_family == AF_INET) 
		return &(((struct sockaddr_in*)sa)->sin_addr);
    //IPv6
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int SendingFile(int listener, char path[])
{
    long file_size, sent, part_read;
    long all_sent = 0;
    struct stat fileinfo;
    FILE* plik;
    unsigned char bufor[10000];
    
    //check if path is correct
    if(stat(path, &fileinfo) < 0)
        return printf("Server can't find information about file.\n"), 1;
    
    //file size
    file_size = fileinfo.st_size;
    if(file_size == 0)
        return 1;

    //open file
    plik = fopen(path, "rb");
    if (plik == NULL)
        return printf("Couldn't read the file\n"), 1;
    
    //read file and send data until there is no data left to send
    while(all_sent < file_size){
        part_read = fread(bufor, 1, 4096, plik);
        sent = send(listener, bufor, part_read, 0);
        if(part_read != sent)
            break;
        
        all_sent += sent;
    }

    //close file
    fclose(plik);   

    return 0;
}