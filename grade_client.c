#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <memory.h>
#include <pthread.h>

typedef struct client {    
	int login;             //0-logout, 1-login
	char id[10];
	int type;             //0-none 1-student, 2-TA
} client_t;

#define N 257
#define DO_SYS(syscall) do {		\
    if( (syscall) == -1 ) {		\
        perror( #syscall );		\
        exit(EXIT_FAILURE);		\
    }						\
} while( 0 )

void split(char str_tok[N][N], char str[N])
{
	int len_all = strlen(str)+1;
	int count = 0;
	int len_word = 0;
	int i = 0;
	for(int j=0; j<N; j++)
		str_tok[j][0] = '\0';
	for(i=0; i<len_all; i++)
	{
		if(str[i] == ' ')
		{
			str_tok[count][len_word] = '\0';
			count++;
			len_word = 0;
		}
		else 
		{
			str_tok[count][len_word] = str[i];
			len_word++;
		}
	}
}

struct addrinfo*
alloc_tcp_addr(const char *host, uint16_t port, int flags)
{
    int err;   struct addrinfo hint, *a;   char ps[16];

    snprintf(ps, sizeof(ps), "%hu", port); 
    memset(&hint, 0, sizeof(hint));
    hint.ai_flags    = flags;
    hint.ai_family   = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_protocol = IPPROTO_TCP;

    if( (err = getaddrinfo(host, ps, &hint, &a)) != 0 ) {
        fprintf(stderr,"%s\n", gai_strerror(err));
        exit(EXIT_FAILURE);
    }

    return a; // should later be freed with freeaddrinfo()
}


/*
 * Return client fd connect()ed to host+port
 */
int tcp_connect(const char* host, uint16_t port)
{
    int clifd;
    struct addrinfo *a = alloc_tcp_addr(host, port, 0);

    DO_SYS( clifd = socket( a->ai_family,
				 a->ai_socktype,
				 a->ai_protocol ) 	);
    DO_SYS( connect( clifd,
				 a->ai_addr,
				 a->ai_addrlen  )   );

    freeaddrinfo( a );
    return clifd;
}

int before_send(char *msg, char *ans, client_t *c)    // return 1-dont send to server
{													  // return 0-send to server
	char tok_msg[N][N];
	split(tok_msg, msg);
	if(strcmp(tok_msg[0], "Login") == 0) {            //login handle
		if(c->type == 1 || c->type == 2) {
			strcpy(ans, "Wrong user information\n");
			return 1;
		}
		else {
			return 0;
		}
	}
	else if(strcmp(tok_msg[0], "ReadGrade") == 0) {  //ReadGrade handle
		if(c->login == 0) {
			strcpy(ans, "Not logged in\n");
			return 1;
		}
		else if(c->type == 2) {
			if(tok_msg[1][0] == '\0') {
				strcpy(ans, "Missing argument\n");
				return 1;
			}
			else {
				return 0;
			}
		}
		else if(c->type == 1 && tok_msg[1][0] != '\0') {
			strcpy(ans, "Action not allowed\n");
			return 1;
		}
		else if(c->type == 1) {
			strcat(msg, " ");
			strcat(msg, c->id);
			return 0;
		}
	}
	else if(strcmp(tok_msg[0], "GradeList") == 0) {		//GradeList handle
		if(c->login == 0) {
			strcpy(ans, "Not logged in\n");
			return 1;
		}
		if(c->type != 2) {
			strcpy(ans, "Action not allowed\n");
			return 1;
		}
		else {
			return 0;
		}
	}
	else if(strcmp(tok_msg[0], "UpdateGrade") == 0) {		//UpdateGrade handle
		if(c->login == 0) {
			strcpy(ans, "Not logged in\n");
			return 1;
		}
		if(c->type != 2) {
			strcpy(ans, "Action not allowed\n");
			return 1;
		}
		else {
			return 0;
		}
	}
	else if(strcmp(tok_msg[0], "Logout") == 0) {		//Logout handle
		if(c->login == 0) {
			strcpy(ans, "Not logged in\n");
			return 1;
		}
		else {
			c->login = 0;
			c->type = 0;
			strcpy(ans, "Good bye ");
			strcat(ans, c->id);
			strcat(ans, "\n");
			return 1;
		}
	}
	else if(strcmp(tok_msg[0], "Exit") == 0) {            // Exit handle
		if(c->login == 1) {
			c->login = 0;
			c->type = 0;
			strcpy(ans, "Good bye ");
			strcat(ans, c->id);
			strcat(ans, "\n");
		}
		return 1;
	}
	else {
		strcpy(ans, "Wrong input");
		return 1;
	}
}

void after_send(char *buf, client_t *c, int *fd, char host_name[N], int *port)
{
	char tok_buf[N][N];
	int i=0;
	split(tok_buf, buf);
	if(strcmp(tok_buf[1], "Student") == 0) {
		c->type = 1;
		for(i=0; tok_buf[2][i] != '\n';i++){}
		tok_buf[2][i] = '\0';	
		strcpy(c->id, tok_buf[2]);
		c->login = 1;
	}
	else if(strcmp(tok_buf[1], "TA") == 0) {
		c->type = 2;
		for(i=0; tok_buf[2][i] != '\n';i++){}
		tok_buf[2][i] = '\0';
		strcpy(c->id, tok_buf[2]);
		c->login = 1;
	}
	else if(strcmp(tok_buf[0], "GradeList") == 0) {
		char s[N];
		memset(s, '\0', N);
		memset(buf, '\0', N);
		strcpy(buf, "");
		while(strcmp(s, "no more students") != 0) {
			memset(s, '\0', N);
			DO_SYS(     read (*fd, s, N) 		  				 );
			DO_SYS(     write (*fd, "", 2) 		  				 );
			memset(buf, '\0', N);
			if(strcmp(s, "no more students") != 0) {
				printf("%s", s);
			}
		}
	}
}

int main(int argc, char *argv[])
{
	int pipe_a[2];
	int pipe_b[2];
	pipe(pipe_a);
	pipe(pipe_b);
	if(fork() == 0) {                      //child
		close(pipe_a[1]);
		close(pipe_b[0]);
        client_t c = {0};
        client_t *client;
        client = &c;
		char cmd_copy[N];
		char host_name[N];
        char ans[N];
		int port,fd, done_c=1, check_msg=0, check_connection=0;
		memset(ans, '\0', N);
		memset(cmd_copy, '\0', N);
		strcpy(host_name, argv[1]);
		port = atoi(argv[2]);
		fd = tcp_connect(host_name, port);
		read(pipe_a[0], cmd_copy, N);
		while(1) {
			check_msg = before_send(cmd_copy, ans, client);
			if(check_msg == 0) {
   	    		DO_SYS(     write(fd, cmd_copy, strlen(cmd_copy))  );
   	    		DO_SYS(     read (fd, ans, N) 		  				 );
				after_send(ans, client, &fd, host_name, &port);
			}
			if(strcmp(cmd_copy, "Exit") == 0) {
            	break;
           	} 
           	write(pipe_b[1], ans, strlen(ans)+1);
			memset(ans, '\0', N);
			memset(cmd_copy, '\0', N);
			read(pipe_a[0], cmd_copy, N);
		}
		DO_SYS(     close(fd)      					         );
		write(pipe_b[1], cmd_copy, strlen(cmd_copy)+1);  
		read(pipe_a[0], cmd_copy, N);
		write(pipe_b[1], ans, strlen(ans)+1); 
	}
	else                    //parent - command line
	{
		char cmd[N];
        int len_cmd;
        char tok_cmd_p[N][N];
		char buf_p[N];
		int done_p=1;
		close(pipe_a[0]);
		close(pipe_b[1]);
        while(1) {
			memset(cmd, '\0', N);
			memset(buf_p, '\0', N);
		    printf("\n> ");
		    scanf("%[^\n]%*c", cmd);
            len_cmd = strlen(cmd)+1;
		    write(pipe_a[1], cmd, len_cmd);
		    read(pipe_b[0], buf_p, N);
            if(strcmp(buf_p, "Exit") == 0) {
				memset(buf_p, '\0', N);
				write(pipe_a[1], "", 2);
				memset(buf_p, '\0', N);
				read(pipe_b[0], buf_p, N);
				printf("%s", buf_p);
                break;
            }
			printf("%s", buf_p);
        }
	}
	return 0;
}
