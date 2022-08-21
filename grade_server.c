#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <memory.h>
#include <unistd.h>
#include "parse_file.h"
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>

#define N 256
#define MAX_STUDENTS 20000
#define THREAD_POOL_SIZE 5

pthread_t thread_pool[THREAD_POOL_SIZE];
pthread_mutex_t que_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
struct Queue *que;
struct student students_data[MAX_STUDENTS];
struct TA TA_data[MAX_STUDENTS];
int count_students;

// A linked list (LL) node to store a queue entry and a path
struct QNode {
    int clifd;
    struct QNode* next;
};

// The queue, front stores the front node of LL and rear stores the
// last node of LL
struct Queue {
    struct QNode *front, *rear;
};

// A utility function to create a new linked list node.
struct QNode* newNode(int clifd){
    struct QNode* temp = (struct QNode*)malloc(sizeof(struct QNode));
    if(!temp) return NULL;
    temp->clifd = clifd;
    temp->next = NULL;
    return temp;
}

// A utility function to create an empty queue
struct Queue* createQueue(){
    struct Queue* q = (struct Queue*)malloc(sizeof(struct Queue));
    if(!q) return NULL;
    q->front = NULL;
    q->rear = NULL;
    return q;
}


void enQueue(struct Queue* q, struct QNode* temp){
    // If queue is empty, then new node is front and rear both
    if (q->rear == NULL) {
        q->front = temp;
        q->rear = temp;
    }
    else {
        // Add the new node at the end of queue and change rear
        q->rear->next = temp;
        q->rear = temp;
    }
}

// Function to remove a path from given queue q
int deQueue(struct Queue* q){

    // If queue is empty, return NULL.
    if (q->front == NULL)
        return -1;

    // Store previous front and move front one node ahead
    struct QNode* temp = q->front;

    q->front = q->front->next;

    // If front becomes NULL, then change rear also as NULL
    if (q->front == NULL)
        q->rear = NULL;
    int clifd = temp->clifd;
    free(temp); //free the node
    return clifd;
}

int is_empty(const struct Queue *q){
    if(q == NULL || (q->front == NULL && q->rear == NULL))
        return 1;
    return 0;
}


/*----------------------------------------handle files-------------------------------------------------*/

void parse_TA_file(TA_t* ta) {
    // get TA data from file
    FILE *fileptr;
    char *token;
    char line [256];
    int i;
    int count_TA = 0;
    fileptr = fopen("assistants.txt", "r");
    char chr;
    while (fgets(line, 256, fileptr) != NULL)
    {
        token = strtok(line, ":");
        strcpy(ta[count_TA].id, token);
        token = strtok(NULL, ":");
        i = strlen(token);
        token[i-1] = '\0'; 
        strcpy(ta[count_TA].password, token);
        count_TA +=1;
    }
    fclose(fileptr);
}

int parse_student_file(student_t* student) {
    //get students data from file
    FILE *fileptr;
    char *token;
    char line [256];
    int i;
    fileptr = fopen("students.txt", "r");
    int count_students = 0;
    while(fgets(line, 256, fileptr) != NULL){
        token = strtok(line, ":");
        strcpy(student[count_students].id, token);
        token = strtok(NULL, ":");
        i = strlen(token);
        token[i-1] = '\0';     
        strcpy(student[count_students].password, token);
        strcpy(student[count_students].grade, "0");
        count_students +=1;
    }
    fclose(fileptr);
    return count_students;
}

// we want to print the students in ascending order by id
int cmp_by_id(const void *a, const void *b) //will be used to sort the students by their id.
{
    student_t *ia = (student_t *)a;
    student_t *ib = (student_t *)b;
    return strcmp(ia->id, ib->id);
}

void sort_students_data(student_t* students_data, int count_students)
{
    qsort(students_data, count_students, sizeof(student_t), cmp_by_id);
}

/*----------------------------------------establish TCP-------------------------------------------------*/

#define DO_SYS(syscall) do {		\
    if( (syscall) == -1 ) {		\
        perror( #syscall );		\
        exit(EXIT_FAILURE);		\
    }						\
} while( 0 )

#define DO_SYS_SOC(syscall) do {    \
    if( (syscall) == -1 ) {			    \
        close(fd);                      \
        return 0;                       \
    }                                   \
}while(0)
    
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

int tcp_establish(int port) {
    int srvfd;
    struct addrinfo *a =
	    alloc_tcp_addr(NULL/*host*/, port, AI_PASSIVE);
    DO_SYS( srvfd = socket( a->ai_family,
				 a->ai_socktype,
				 a->ai_protocol ) 	);
    DO_SYS( bind( srvfd,
				 a->ai_addr,
				 a->ai_addrlen  ) 	);
    DO_SYS( listen( srvfd,
				 5/*backlog*/   ) 	);
    freeaddrinfo( a );
    return srvfd;
}

/*----------------------------------------Functions-------------------------------------------------*/

int is_log_in (char *buf, char *ans, bool *teacher) {
    char* token;
    char log[N];
    char id[N];
    char pas[N];
    memset(log, '\0', N);
    memset(id, '\0', N);
    memset(pas, '\0', N);
    token = strtok(buf," ");
    strcpy(log, token);
    token = strtok(NULL," ");
    if (NULL == token) {
        strcpy(ans, "Wrong user information");
        return strlen(ans);
    }
    strcpy(id, token);
    token = strtok(NULL," ");
    if (NULL == token) {
        strcpy(ans, "Wrong user information");
        return strlen(ans);
    }
    strcpy(pas, token);
    int i = 0;
    for (i; i<count_students; i++)
    {
        if (strcmp(students_data[i].id , id) == 0)
        {
            if (strcmp(students_data[i].password, pas) == 0)
            {
                strcpy(ans, "Welcome Student ");
                strcat(ans, id);
                strcat(ans, "\n");
                return strlen(ans);
            }
        } 
    }   
    i = 0;
    for (i; i<count_students; i++)
    {
        if (strcmp(TA_data[i].id , id) == 0)
        {
            if (strcmp(TA_data[i].password, pas) == 0)
            {
                strcpy(ans, "Welcome TA ");
                *teacher = true;
                strcat(ans, id);
                strcat(ans, "\n");
                return strlen(ans);
            }
        }        
    }

    strcpy(ans, "Wrong user information");
    return strlen(ans);
}


int handle_input(char *buf, char *ans, bool *teacher){
    char *token;
    char first_input[N];
    char sec_input[N]; 
    char third_input[N];
    int len;
    char buf_cpy[N];
    memset(first_input, '\0', N);
    memset(sec_input, '\0', N);
    memset(third_input, '\0', N);
    memset(buf_cpy, '\0', N);
    strcpy(buf_cpy, buf);
    token = strtok(buf_cpy, " ");
    strcpy(first_input, token);
    token = strtok(NULL, " ");
    if(token != NULL) {
        strcpy(sec_input, token);
    }
    token = strtok(NULL, " ");
    if(token != NULL) {
        strcpy(third_input, token);
    }

    if (strcmp(first_input, "Login")== 0) {
        len = is_log_in(buf, ans, teacher);
        return len;
    } 
    else if(strcmp(first_input, "ReadGrade")==0){
    	if (false == *teacher) {
    		// Assumes sec_input is the client's id!
    		for(int i= 0; i< count_students; i++){
	            if(strcmp(students_data[i].id, sec_input) == 0){
	                strcpy(ans, students_data[i].grade);
	                return strlen(ans);
	            }
	        }
    	} else {
    		if (0 == strlen(sec_input)) {
    			strcpy(ans, "Missing argument");
        		return strlen(ans);
    		}
	        for(int i= 0; i< count_students; i++){
	            if(strcmp(students_data[i].id, sec_input) == 0){
	                strcpy(ans, students_data[i].grade);
	                return strlen(ans);
	            }
	        }
	        strcpy(ans, "Invalid id");
        	return strlen(ans);
    	}
            
    }
    else if(strcmp(first_input, "UpdateGrade") == 0){
    	if (false == *teacher) {
    		strcpy(ans, "Action not allowed");
    		return strlen(ans);
    	}
    	if (0 == strlen(sec_input)) {
    		strcpy(ans, "Invalid id");
        	return strlen(ans);
    	}
    	if (0 == strlen(third_input)) {
    		strcpy(ans, "Invalid grade");
        	return strlen(ans);
    	}
        int i = 0;
        int check_student_exists = 1;
        for (i; i<count_students; i++)
        {
            if (strcmp(students_data[i].id , sec_input) == 0)
            {
                strcpy(students_data[i].grade, third_input);
                check_student_exists = 0;
                return 0;
            }
        }
        if (check_student_exists == 1){
            strcpy(students_data[count_students].id, sec_input);
            strcpy(students_data[count_students].password, " ");
            strcpy(students_data[count_students].grade, "0");
            count_students +=1;
            return 0;
        }
	}
    strcpy(ans, buf);
    return strlen(ans);
}

void wait_for_signal(){
    //acquire lock and wait to other thread add item to the que
    pthread_mutex_lock(&que_lock);
    pthread_cond_wait(&not_empty, &que_lock);
    pthread_mutex_unlock(&que_lock);
}

void handle_client(int clifd){
    char buf[N];
    char answer[N];
    bool teacher = false;
    while(1) 
    {
        memset(buf, '\0', N);
        memset(answer, '\0', N);
        int r = recv(clifd, buf, N, 0);

        int s;
        if(r == -1 || r == 0){
            close(clifd);
            return;
        }
        
        handle_input(buf, answer, &teacher);
        if (strcmp(answer, "Exit") == 0 || strcmp(answer, "Kill") == 0)
        {
            close(clifd);
            return;
        }
        if (strcmp(answer, "GradeList") == 0) 
        {
        	if (false == teacher) {
        		memset(answer, '\0', N);
	            strcat(answer, "Action not allowed");
	            int k = strlen(answer)+1;
	            s = send(clifd, answer, k, 0);
	            if (s == -1 || s == 0){
	                close(clifd);
	            }
	            return;
        	}
        	s = send(clifd, answer, N, 0);
            if (s == -1 || s == 0){
                close(clifd);
                return;
            }
            student_t data_copy[MAX_STUDENTS];
            memcpy(data_copy, students_data, sizeof(student_t)*MAX_STUDENTS);
            sort_students_data(data_copy, count_students); //working on a copy of the data so the original will not change.
            for (int i = 0; i < count_students; i++)
            {
                memset(answer, '\0', N);
                strcpy(answer, data_copy[i].id);
                strcat(answer, ": ");
                strcat(answer, data_copy[i].grade);
                strcat(answer, "\n");
                int k = strlen(answer)+1;
                s = send(clifd, answer, k, 0);
                if (s == -1  || s == 0){
                    close(clifd);
                    return;
                }
                r = recv (clifd, buf, N, 0);
                if(r == -1 || r == 0){
                    close(clifd);
                    return;
                }
            }
            memset(answer, '\0', N);
            strcat(answer, "no more students");
            int k = strlen(answer)+1;
            s = send(clifd, answer, k, 0);
            if (s == -1 || s == 0){
                close(clifd);
                return;
            }
            r = recv (clifd, buf, N, 0);
            if(r == -1 || r == 0){
                close(clifd);
                return;
            }
        }
        else
        {
        	s = send(clifd, answer, N, 0);
            if(s == -1 || s == 0){
                close(clifd);
                return;
            }

        }
    }
}

void* start(void* arg){
    while(1){
        //Try to acquire the lock and iterate over the que
        pthread_mutex_lock(&que_lock);
        if (!is_empty(que)) {
            int clifd = deQueue(que);
            pthread_mutex_unlock(&que_lock);
            handle_client(clifd);
        }
        else {
            pthread_mutex_unlock(&que_lock);
            wait_for_signal();
        }
    }
}

void echo_server(int srvfd)
 {
    que = createQueue();
    int clifd;
    while(1){
        DO_SYS( clifd = accept(srvfd, NULL, NULL) );
        struct QNode *node = newNode(clifd);
        pthread_mutex_lock(&que_lock);
        enQueue(que,node);
        pthread_cond_signal(&not_empty); //Notify other threads there is new item
        pthread_mutex_unlock(&que_lock);

    }
    DO_SYS( close(srvfd) ); 
}


int main(int argc, char *argv[]) {
    int port = atoi(argv[1]);
    parse_TA_file(TA_data);
    count_students = parse_student_file(students_data); // we will use this num if "GradeList" request is sent.
    int srvfd = tcp_establish(port);
    for(int i =0; i< THREAD_POOL_SIZE; i++){
        pthread_create(&thread_pool[i], NULL, start ,NULL);
    }
    echo_server(srvfd);
    return 0;
}
