#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include <sys/select.h>

#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)

#define OBJ_NUM 3

#define FOOD_INDEX 0
#define CONCERT_INDEX 1
#define ELECS_INDEX 2
#define RECORD_PATH "./bookingRecord"

#define HI 0
#define ID 1
#define EXIT 2
#define COMMAND 3

#define UNLOCK 0
#define LOCK 1

static char* obj_names[OBJ_NUM] = {"Food", "Concert", "Electronics"};

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[512];  // data sent by/to client
    size_t buf_len;  // bytes used by buf
    int id;
    int wait_for_write;  // used by handle_read to know if the header is read or not.
} request;

server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list

const char* accept_read_header = "ACCEPT_FROM_READ";
const char* accept_write_header = "ACCEPT_FROM_WRITE";
const unsigned char IAC_IP[3] = "\xff\xf4";

static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

typedef struct {
    int id;          // 902001-902020
    int bookingState[OBJ_NUM]; // array of booking number of each object (0 or positive numbers)
}record;

int checkcommand(request* reqP, int fd)
{
    int len = reqP->buf_len;
    record input;
    read(fd, &input, sizeof(record));
    lseek(fd, (-1)*sizeof(record), SEEK_CUR);
    int ori = input.bookingState[0] + input.bookingState[1] + input.bookingState[2];

    int s = len-1;
    int m = len;
    int time = 1;
    int ch_f = 0;
    int ch_c = 0;
    int ch_e = 0;
    int new = 0;
    for(int i = len-1; i >= 0; i--)
    {
        //fprintf(stderr, "command:  %c\n", reqP->buf[i]);
        if(i == 0)
        {
            if(reqP->buf[i] == '-')
                new *= (-1);
            else if(reqP->buf[i] == ' ')
                return -1;
            else if(reqP->buf[i] < '0' || reqP->buf[i] > '9')
                return -1;
            else
            {
                int val = reqP->buf[i] - '0';
                double d = 1;
                for(int j = 0; j < s-i; j++)
                    d *= 10;
                val = val* (int)d;
                new += val;
            }
            ch_f = new;
            new = 0;
            continue;
        }

        if(reqP->buf[i] == ' ')
        {
            s = i-1;
            if(time == 1)
                ch_e = new;
            else if(time == 2)
                ch_c = new;
            new = 0;
            time += 1;
        }
        else if(reqP->buf[i] == '-')
        {
            if(i-1 > 0 && reqP->buf[i-1] != ' ')
                return -1;
            new *= (-1);
        }
        else if(reqP->buf[i] < '0' || reqP->buf[i] > '9')
            return -1;
        else
        {
            int val = reqP->buf[i] - '0';
            double d = 1;
            for(int j = 0; j < s-i; j++)
                d *= 10;
            val = val* (int)d;
            new += val;
        }
    }

    if(ori + ch_f + ch_c + ch_e > 15) //exceed upperbound
        return 1;
    else if(input.bookingState[0] + ch_f < 0) //lowerbounds
        return 2;
    else if(input.bookingState[1] + ch_c < 0)
        return 2;
    else if(input.bookingState[2] + ch_e < 0)
        return 2;
    
    input.bookingState[0] += ch_f;
    input.bookingState[1] += ch_c;
    input.bookingState[2] += ch_e;

    write(fd, &input, sizeof(record));
    lseek(fd, (-1)*sizeof(record), SEEK_CUR);
    return 0;
}

bool checkid(request* reqP)
{
    int len = reqP->buf_len;
    for(int i = 0; i < len; i++)
    {
        if(reqP->buf[i] < '0' || reqP->buf[i] > '9')
        {
            //fprintf(stderr, "bad appear at non digit: %d", i);
            return false;
        }
            
    }

    int idnum = atoi(reqP->buf);
    if(idnum < 902001 || idnum > 902020)
        return false;
    return true;
}

int handle_read(request* reqP) {
    /*  Return value:
     *      1: read successfully
     *      0: read EOF (client down)
     *     -1: read failed
     */
    int r;
    char buf[512];
    memset(buf, '0', sizeof(buf));

    // Read in request from client
    r = read(reqP->conn_fd, buf, sizeof(buf));
    if (r < 0) return -1;
    if (r == 0) return 0;
    char* p1 = strstr(buf, "\015\012");
    int newline_len = 2;
    if (p1 == NULL) {
       p1 = strstr(buf, "\012");
        if (p1 == NULL) {
            if (!strncmp(buf, IAC_IP, 2)) {
                // Client presses ctrl+C, regard as disconnection
                //fprintf(stderr, "Client presses ctrl+C....\n");
                return 0;
            }
            ERR_EXIT("this really should not happen...");
        }
    }
    size_t len = p1 - buf + 1;
    memmove(reqP->buf, buf, len);
    reqP->buf[len - 1] = '\0';
    reqP->buf_len = len-1;
    return 1;
}



int main(int argc, char** argv) {

    // Parse args.
    if (argc != 2) {
        //fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    struct sockaddr_in cliaddr;  // used by accept()
    int clilen;

    int conn_fd;  // fd for a new connection with client
    int file_fd;  // fd for file that we open for reading
    
    char buf[512];
    memset(buf, 0, sizeof(buf));
    int buf_len;

    // Initialize server
    init_server((unsigned short) atoi(argv[1]));

    // Loop for handling connections
    //fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);

    int book[21];
    for(int i = 1; i <= 20; i++)
    {
        book[i] = open(RECORD_PATH, O_RDWR);
        if(book[i] < 0)
            ERR_EXIT("open error");
        
        int ch = lseek(book[i], (i-1)*sizeof(record), SEEK_SET);
        if(ch < 0)
            ERR_EXIT("lseek error");
    }
    
    fd_set master;
    fd_set read_fds;
    int state[maxfd+1];
    int readlc[30];
    int samewr[30];
    memset(readlc, 0, 30*sizeof(int));
    memset(samewr, 0, 30*sizeof(int));
    memset(state, 0, (maxfd+1)*sizeof(int));

    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    FD_SET(svr.listen_fd, &master);
    maxfd = svr.listen_fd;
        
    struct flock wr;
    memset (&wr, 0, sizeof(struct flock));
    wr.l_type = F_WRLCK;
    wr.l_whence = SEEK_CUR;
    wr.l_start = 0;
    wr.l_len = sizeof(record);

    struct flock rd;
    memset (&rd, 0, sizeof(struct flock));
    rd.l_type = F_RDLCK;
    rd.l_whence = SEEK_CUR;
    rd.l_start = 0;
    rd.l_len = sizeof(record);

    struct flock un;
    memset (&un, 0, sizeof(struct flock));
    un.l_type = F_UNLCK;
    un.l_whence = SEEK_CUR;
    un.l_start = 0;
    un.l_len = sizeof(record);

    while (1) {
        // TODO: Add IO multiplexing
        read_fds = master;
        if(select(maxfd+1, &read_fds, NULL, NULL, NULL) == -1)
                ERR_EXIT("select");
        
        for(int i = 0; i <= maxfd; i++)
        {
            memset(buf, 0, sizeof(buf));
            if(FD_ISSET(i, &read_fds))
            {
                if(i == svr.listen_fd)
                {
                    clilen = sizeof(cliaddr);
                    conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
                    if (conn_fd < 0) 
                    {
                        if (errno == EINTR || errno == EAGAIN) 
                            continue;  // try again
                        if (errno == ENFILE) {
                            //(void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                            continue;
                        }
                        ERR_EXIT("accept");
                    }
                    else
                    {
                        requestP[conn_fd].conn_fd = conn_fd;
                        strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
                        //fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);
                        FD_SET(conn_fd, &master);
                        if(conn_fd > maxfd)
                            maxfd = conn_fd;
                        sprintf(buf, "%s\n", "Please enter your id (to check your booking state):");
                        write(conn_fd, buf, strlen(buf));
                        state[conn_fd] = ID;
                        //fprintf(stderr, "%d STATE: %d\n", i, state[i]);
                    }
                }
                else
                {
                    conn_fd = i;
                    int ret = handle_read(&requestP[conn_fd]); // parse data from client to requestP[conn_fd].buf
                    //fprintf(stderr, "ret = %d\n", ret);
                    //fprintf(stderr, "%d STATE: %d\n", i, state[i]);
                    if (ret < 0) 
                    {
                        close(requestP[conn_fd].conn_fd);
                        free_request(&requestP[conn_fd]);
                        FD_CLR(i, &master);                      
                    }
#ifdef READ_SERVER
                    else if(ret == 0)
                    {
                        if(state[i] <= ID)
                        {}
                        else
                        {
                            readlc[requestP[conn_fd].id % 100] -= 1;
                            if(readlc[requestP[conn_fd].id % 100] == 0)
                                fcntl(book[requestP[conn_fd].id % 100], F_SETLK, &un);
                        }
                        close(requestP[conn_fd].conn_fd);
                        free_request(&requestP[conn_fd]);
                        FD_CLR(i, &master); 
                    }
                    else
                    {
                        int id = 30;
                        if(state[i] == ID)
                        {
                            //fprintf(stderr, "in state id?? lets check id\n");
                            if(! checkid(&requestP[conn_fd]))
                            {
                                sprintf(buf, "%s\n", "[Error] Operation failed. Please try again.");
                                write(conn_fd, buf, strlen(buf));
                                //fprintf(stderr, "not satisfy chaeckid\n");
                                close(requestP[conn_fd].conn_fd);
                                free_request(&requestP[conn_fd]);
                                FD_CLR(i, &master);
                                continue;
                            }
                            else
                            {
                                requestP[conn_fd].id = atoi(requestP[conn_fd].buf);
                                id = requestP[conn_fd].id;
                                id %= 100;

                                //get if there is write lock
                                
                                if(fcntl(book[id], F_SETLK, &rd) == -1)
                                {
                                    sprintf(buf, "%s\n", "Locked.");
                                    write(conn_fd, buf, strlen(buf));
                                    close(requestP[conn_fd].conn_fd);
                                    free_request(&requestP[conn_fd]);
                                    FD_CLR(i, &master);
                                    state[i] = HI;
                                    continue;
                                }

                                //there is no write lock
                                //rd.l_type = F_RDLCK;
                                //fcntl(book[id], F_SETLK, &rd);
                                readlc[id] += 1;
                                
                                //fprintf(stderr, "%d\n", readlc[id]);

                                record input;
                                read(book[id], &input, sizeof(record));
                                lseek(book[id], (-1)*sizeof(record), SEEK_CUR);
                                
                                sprintf(buf, "%s", "Food: ");
                                write(conn_fd, buf, strlen(buf));
                                sprintf(buf, "%d", input.bookingState[0]);
                                write(conn_fd, buf, strlen(buf));
                                sprintf(buf, "%s\n%s", " booked","Concert: ");
                                write(conn_fd, buf, strlen(buf));
                                sprintf(buf, "%d", input.bookingState[1]);
                                write(conn_fd, buf, strlen(buf));
                                sprintf(buf, "%s\n%s", " booked", "Electronics: ");
                                write(conn_fd, buf, strlen(buf));
                                sprintf(buf, "%d", input.bookingState[2]);
                                write(conn_fd, buf, strlen(buf));
                                sprintf(buf, "%s\n\n%s\n", " booked","(Type Exit to leave...)");
                                write(conn_fd, buf, strlen(buf));
                                
                                state[i] = EXIT;
                            }
                        }
                        else if(state[i] == EXIT)
                        {
                            id = requestP[conn_fd].id;
                            id %= 100;
                            if(strcmp(requestP[conn_fd].buf, "Exit") == 0)
                            {
                                close(requestP[conn_fd].conn_fd);
                                free_request(&requestP[conn_fd]);
                                FD_CLR(i, &master);
                                state[i] = HI;
                                readlc[id] -= 1;
                                if(readlc[id] == 0)
                                    fcntl(book[id], F_SETLK, &un);
                            }
                        }
                        
                    }
#elif defined WRITE_SERVER
                    else if(ret == 0)
                    {
                        if(state[i] <= ID)
                        {}
                        else
                        {
                            samewr[requestP[conn_fd].id % 100] = 0;
                            fcntl(book[requestP[conn_fd].id % 100], F_SETLK, &un);
                        }
                        close(requestP[conn_fd].conn_fd);
                        free_request(&requestP[conn_fd]);
                        FD_CLR(i, &master); 
                    }
                    else
                    {
                        int id = 30;
                        if(state[i] == ID)
                        {
                            //fprintf(stderr, "in state id?? lets check id\n");
                            if(! checkid(&requestP[conn_fd]))
                            {
                                sprintf(buf, "%s\n", "[Error] Operation failed. Please try again.");
                                write(conn_fd, buf, strlen(buf));
                                //fprintf(stderr, "not satisfy chaeckid\n");
                                close(requestP[conn_fd].conn_fd);
                                free_request(&requestP[conn_fd]);
                                FD_CLR(i, &master);
                                continue;
                            }
                            else
                            {
                                requestP[conn_fd].id = atoi(requestP[conn_fd].buf);
                                id = requestP[conn_fd].id % 100;

                                //fcntl(book[id], F_SETLK, &wr)
                                //fprintf(stderr, "write lock %d\n", samewr[id]);
                                if(fcntl(book[id], F_SETLK, &wr) == -1 || samewr[id] == 1)
                                {
                                    sprintf(buf, "%s\n", "Locked.");
                                    write(conn_fd, buf, strlen(buf));
                                    close(requestP[conn_fd].conn_fd);
                                    free_request(&requestP[conn_fd]);
                                    FD_CLR(i, &master);
                                    state[i] = HI;
                                    continue;
                                }

                                //wr.l_type = F_WRLCK;
                                //fcntl(book[id], F_SETLK, &wr);
                                samewr[id] = 1;

                                //fprintf(stderr, "satisfy checkid\n");
                                
                                record input;
                                read(book[id], &input, sizeof(record));
                                lseek(book[id], (-1)*sizeof(record), SEEK_CUR);
                                
                                sprintf(buf, "%s", "Food: ");
                                write(conn_fd, buf, strlen(buf));
                                sprintf(buf, "%d", input.bookingState[0]);
                                write(conn_fd, buf, strlen(buf));
                                sprintf(buf, "%s\n%s", " booked", "Concert: ");
                                write(conn_fd, buf, strlen(buf));
                                sprintf(buf, "%d", input.bookingState[1]);
                                write(conn_fd, buf, strlen(buf));
                                sprintf(buf, "%s\n%s", " booked", "Electronics: ");
                                write(conn_fd, buf, strlen(buf));
                                sprintf(buf, "%d", input.bookingState[2]);
                                write(conn_fd, buf, strlen(buf));
                                sprintf(buf, "%s\n\n%s\n", " booked","Please input your booking command. (Food, Concert, Electronics. Positive/negative value increases/decreases the booking amount.):");
                                write(conn_fd, buf, strlen(buf));
                                state[i] = COMMAND;
                            }
                        }
                        else if(state[i] == COMMAND)
                        {
                            id = requestP[conn_fd].id;
                            id %= 100;
                            int check = checkcommand(&requestP[conn_fd], book[id]);
                            if(check == 1)
                            {
                                //wrlock[conn_fd] = UNLOCK;
                                //fprintf(stderr, requestP[conn_fd].buf, requestP[conn_fd].buf_len);
                                //fprintf(stderr, "\nnot satisfy chaeckcommand\n");
                                sprintf(buf, "%s\n", "[Error] Sorry, but you cannot book more than 15 items in total.");
                                write(conn_fd, buf, strlen(buf));
                                close(requestP[conn_fd].conn_fd);
                                free_request(&requestP[conn_fd]);
                                FD_CLR(i, &master);
                                samewr[id] = 0;
                                fcntl(book[id], F_SETLK, &un); 
                            }
                            else if(check == 2)
                            {
                                sprintf(buf, "%s\n", "[Error] Sorry, but you cannot book less than 0 items.");
                                write(conn_fd, buf, strlen(buf));
                                close(requestP[conn_fd].conn_fd);
                                free_request(&requestP[conn_fd]);
                                FD_CLR(i, &master);
                                samewr[id] = 0;
                                fcntl(book[id], F_SETLK, &un); 
                            }
                            else if(check == -1)
                            {
                                //fprintf(stderr,"%s\n" ,"command with alp");
                                sprintf(buf, "%s\n", "[Error] Operation failed. Please try again.");
                                write(conn_fd, buf, strlen(buf));
                                close(requestP[conn_fd].conn_fd);
                                free_request(&requestP[conn_fd]);
                                FD_CLR(i, &master);
                                samewr[id] = 0;
                                fcntl(book[id], F_SETLK, &un); 
                            }
                            else
                            {
                                //fprintf(stderr, "satisfy checkcommmand\n");
                                record input;
                                read(book[id], &input, sizeof(record));
                                lseek(book[id], (-1)*sizeof(record), SEEK_CUR);
                                
                                sprintf(buf, "%s", "Bookings for user 9020");
                                write(conn_fd, buf, strlen(buf));
                                if(id < 10)
                                    sprintf(buf, "%s%d", "0", id);
                                else
                                    sprintf(buf, "%d", id);
                                write(conn_fd, buf, strlen(buf));
                                sprintf(buf, "%s\n%s", " are updated, the new booking state is:","Food: ");
                                write(conn_fd, buf, strlen(buf));
                                sprintf(buf, "%d", input.bookingState[0]);
                                write(conn_fd, buf, strlen(buf));
                                sprintf(buf, "%s\n%s", " booked", "Concert: ");
                                write(conn_fd, buf, strlen(buf));
                                sprintf(buf, "%d", input.bookingState[1]);
                                write(conn_fd, buf, strlen(buf));
                                sprintf(buf, "%s\n%s", " booked", "Electronics: ");
                                write(conn_fd, buf, strlen(buf));
                                sprintf(buf, "%d", input.bookingState[2]);
                                write(conn_fd, buf, strlen(buf));
                                sprintf(buf, "%s", " booked\n");
                                write(conn_fd, buf, strlen(buf));

                                close(requestP[conn_fd].conn_fd);
                                free_request(&requestP[conn_fd]);
                                FD_CLR(i, &master);
                                state[i] = HI;
                                samewr[id] = 0;
                                fcntl(book[id], F_SETLK, &un); 
                                
                            }
                            //samewr[id] = 0;
                            //fcntl(book[id], F_SETLK, &un); 
                        }
                    }
                    
#endif
                }
            }
        }

    }
    free(requestP);

    return 0;
}

// ======================================================================================================
// You don't need to know how the following codes are working
#include <fcntl.h>
//
static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->id = 0;
}

static void free_request(request* reqP) {
    /*if (reqP->filename != NULL) {
        free(reqP->filename);
        reqP->filename = NULL;
    }*/
    init_request(reqP);
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");
    //
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }

    // Get file descripter table size and initialize request table
    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (int i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);

    return;
}
