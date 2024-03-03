#include "header.h"
#include <sys/mman.h>

movie_profile* movies[MAX_MOVIES];
unsigned int num_of_movies = 0;
unsigned int num_of_reqs = 0;
int cntt = 0;
//char* moviename[MAX_REQ][MAX_MOVIES];
//double moviescore[MAX_REQ][MAX_MOVIES];

struct data
{
	char* moviename[MAX_MOVIES];
	double moviescore[MAX_MOVIES];
	int cnt;
	int base;
	int size;
	int left;
}typedef data;

struct send
{
	int order;
	int time;
	int mer;
	int b;
}typedef send; 

send* deli[MAX_REQ];
data d[MAX_REQ];

// Global request queue and pointer to front of queue
// TODO: critical section to protect the global resources
request* reqs[MAX_REQ];
int front = -1;

/* Note that the maximum number of processes per workstation user is 512. * 
 * We recommend that using about <256 threads is enough in this homework. */
pthread_t tid[MAX_CPU][MAX_THREAD]; //tids for multithread

#ifdef PROCESS
pid_t pid[MAX_CPU][MAX_THREAD]; //pids for multiprocess
#endif

//mutex
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; 

void initialize(FILE* fp);
request* read_request();
int pop();

int pop(){
	front+=1;
	return front;
}

void* cal(void* arg)
{
	int n = *(int*) arg;
	int cntt = 0;
	for(int i = 0; i < num_of_movies; i++)
	{
		if(strcmp(reqs[n]->keywords, "*") == 0 || strstr(movies[i]->title, reqs[n]->keywords) != NULL)
		{
			double sum = 0;
			for(int j = 0; j < NUM_OF_GENRE; j++)
			{
				sum += reqs[n]->profile[j]*movies[i]->profile[j];
			}
			//printf("found target\n");
			d[n].moviename[cntt] = malloc(sizeof(char)*(strlen(movies[i]->title)+1));
			strcpy(d[n].moviename[cntt], movies[i]->title);
			d[n].moviescore[cntt] = sum;
			cntt += 1;
		}
	}
	d[n].cnt = cntt;
	
	if(d[n].cnt > 60000)
		d[n].base = 32;
	else if(d[n].cnt > 30000)
		d[n].base = 16;
	else if(d[n].cnt > 10000)
		d[n].base = 8;
	else if(d[n].cnt > 5000)
		d[n].base = 4;
	else if(d[n].cnt > 2000)
		d[n].base = 2;
	else 
		d[n].base = 1;
	
	d[n].size = d[n].cnt / d[n].base;
	d[n].left = d[n].cnt % d[n].base;

	pthread_exit(NULL);
}

void* sorting(void* arg)
{
	send temp = *(send*) arg;
	int n = temp.order;
	int cnt = temp.time;
	int size = d[n].size;

	if(cnt == d[n].base - 1)
		sort(d[n].moviename+size*cnt, d[n].moviescore+size*cnt, (size + d[n].cnt % d[n].base));
	else
		sort(d[n].moviename+size*cnt, d[n].moviescore+size*cnt, size);

	pthread_exit(NULL);
}

void* wr(void* arg)
{
	int n = *(int*) arg;
	char filename[20];
	sprintf(filename, "%dt.out", reqs[n]->id);
	FILE* fd = fopen(filename, "w+");
	for(int i = 0; i < d[n].cnt; i++)
	{
		fprintf(fd, "%s\n", d[n].moviename[i]);
		free(d[n].moviename[i]);
	}
	pthread_exit(NULL);
}

void* truemerge(void* arg)
{
	send temp = *(send*) arg;
	int n = temp.order;
	int s = temp.mer;
	int t = temp.time - 1;
	int b = temp.b;

	int l = 2*s*t;
	int r = l + s;
	char* newname[2*s + d[n].base];
	double newscore[2*s + d[n].base];
	int c = 0;

	int end = r + s;
	if(temp.time == temp.b)
		end = d[n].cnt;
	
	while(l < 2*s*t+s && r < end)
	{
		if(d[n].moviescore[l] > d[n].moviescore[r])
		{
			newname[c] = malloc(sizeof(char)*(strlen(d[n].moviename[l])+1));
			strcpy(newname[c], d[n].moviename[l]);
			newscore[c] = d[n].moviescore[l];
			l += 1;
		}
		else if(d[n].moviescore[l] < d[n].moviescore[r])
		{
			newname[c] = malloc(sizeof(char)*(strlen(d[n].moviename[r])+1));
			strcpy(newname[c], d[n].moviename[r]);
			newscore[c] = d[n].moviescore[r];
			r += 1;
		}
		else if(strcmp(d[n].moviename[l], d[n].moviename[r]) < 0)
		{
			newname[c] = malloc(sizeof(char)*(strlen(d[n].moviename[l])+1));
			strcpy(newname[c], d[n].moviename[l]);
			newscore[c] = d[n].moviescore[l];
			l += 1;
		}
		else
		{
			newname[c] = malloc(sizeof(char)*(strlen(d[n].moviename[r])+1));
			strcpy(newname[c], d[n].moviename[r]);
			newscore[c] = d[n].moviescore[r];
			r += 1;
		}
		c += 1;
	}

	if(l < 2*s*t+s)
	{
		for(int j = l; j < 2*s*t+s; j++)
		{
			free(d[n].moviename[end - (2*s*t+s -j)]);
			d[n].moviename[end - (2*s*t+s -j)] = malloc(sizeof(char)*(strlen(d[n].moviename[j])+1));
			strcpy(d[n].moviename[end - (2*s*t+s -j)], d[n].moviename[j]);
			d[n].moviescore[end - (2*s*t+s -j)] = d[n].moviescore[j];
		}
	}

	for(int i = 0; i < c; i++)
	{
		free(d[n].moviename[2*s*t + i]);
		d[n].moviename[2*s*t + i] = malloc(sizeof(char)*(strlen(newname[i])+1));
		strcpy(d[n].moviename[2*s*t + i], newname[i]);
		d[n].moviescore[2*s*t + i] = newscore[i];
	}
	pthread_exit(NULL);
}

void* merge(void* arg)
{
	int n = *(int*)arg;
	int base = d[n].base;
	int count = 1;
	while(base > 1)
	{
		for(int i = 1; i <= base/2; i++)
		{
			deli[n][i].time = i;
			deli[n][i].mer = count*d[n].size;
			deli[n][i].b = base/2;
			pthread_create(&tid[n][i], NULL, truemerge, &deli[n][i]);
		}
		for(int i = 1; i <= base/2; i++)
		{
			pthread_join(tid[n][i], NULL);
		}
		count *= 2;
		base /= 2;
	}
	pthread_exit(NULL);
}

int main(int argc, char *argv[]){

	if(argc != 1){
#ifdef PROCESS
		fprintf(stderr,"usage: ./pserver\n");
#elif defined THREAD
		fprintf(stderr,"usage: ./tserver\n");
#endif
		exit(-1);
	}
	FILE *fp;

	if ((fp = fopen("./data/movies.txt","r")) == NULL){
		ERR_EXIT("fopen");
	}

	initialize(fp);
	assert(fp != NULL);
	fclose(fp);	

	int num[num_of_reqs];
	//calculate score
	for(int i = 0; i < num_of_reqs; i++)
	{
		num[i] = i;
		pthread_create(&tid[i][0], NULL, cal, (void*)&num[i]);
	}
	for(int i = 0; i < num_of_reqs; i++)
	{
		pthread_join(tid[i][0], NULL);
	}
	//printf("after calculate score\n");
#ifdef PROCESS
	char **name;
	name = mmap(NULL, d[0].cnt*sizeof(char*), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	double* score = mmap(NULL, d[0].cnt*sizeof(double), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	for(int i = 0; i < d[0].cnt; i++)
	{
		//printf("%s\n",d[0].moviename[i]);
		name[i] = mmap(NULL, MAX_LEN, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		strcpy(name[i], d[0].moviename[i]);
		score[i] = d[0].moviescore[i];
		//printf("%s\n", name[i]);
	}
	
	int base = d[0].base;
	while(base >= 1)
	{
		for(int i = 0; i < base; i++)
		{
			pid[0][i] = fork();
			if(pid[0][i] == 0 && d[0].base == base)
			{
				int start = d[0].size*i;
				int end = d[0].size*(i+1);
				int size = d[0].size;
				if(i == d[0].base - 1)
				{
					end += d[0].left;
					size += d[0].left;
				}
				char* sub[size];
				double pts[size];
				for(int j = start; j < end; j++)
				{
					sub[j - start] = malloc(sizeof(char)*(strlen(name[j])+1));
					strcpy(sub[j - start], name[j]);
					pts[j - start] = score[j];
				}
				sort(sub, pts, size);
				for(int j = start; j < end; j++)
				{
					strcpy(name[j], sub[j - start]);
					score[j] = pts[j - start];
					free(sub[j - start]);
				}
				_exit(0);
			}
			else if(pid[0][i] == 0)
			{
				int l = d[0].size*i;
				int r = l + d[0].size/2;
				char* temp[d[0].size + d[0].left];
				double tmppts[d[0].size + d[0].left];
				int c = 0;
				int end = d[0].size*(i+1);
				if(i == base - 1)
					end += d[0].left;
				while(l < d[0].size*i + d[0].size/2 && r < end)
				{
					if(score[l] > score[r])
					{
						temp[c] = malloc(sizeof(char)*(strlen(name[l])+1));
						strcpy(temp[c], name[l]);
						tmppts[c] = score[l];
						l += 1;
					}
					else if(score[r] > score[l])
					{
						temp[c] = malloc(sizeof(char)*(strlen(name[r])+1));
						strcpy(temp[c], name[r]);
						tmppts[c] = score[r];
						r += 1;
					}
					else if(strcmp(name[l], name[r]) < 0)
					{
						temp[c] = malloc(sizeof(char)*(strlen(name[l])+1));
						strcpy(temp[c], name[l]);
						tmppts[c] = score[l];
						l += 1;
					}
					else
					{
						temp[c] = malloc(sizeof(char)*(strlen(name[r])+1));
						strcpy(temp[c], name[r]);
						tmppts[c] = score[r];
						r += 1;
					}
					c += 1;
				}
				if(l < d[0].size*i + d[0].size/2 )
				{
					for(int j = l; j < d[0].size*i + d[0].size/2 ; j++)
					{
						temp[c] = malloc(sizeof(char)*(strlen(name[j])+1));
						strcpy(temp[c], name[j]);
						tmppts[c] = score[l];
						c += 1;
					}
				}
				for(int j = 0; j < c; j++)
				{
					strcpy(name[d[0].size*i+j], temp[j]);
					score[d[0].size*i+j] = tmppts[j];
					free(temp[j]);
				}
				_exit(0);
			}
		}
		for(int i = 0; i < base; i++)
			waitpid(pid[0][i], 0, 0);
		base /= 2;
		d[0].size *= 2;
	}
	
	char filename[20];
	sprintf(filename, "%dp.out", reqs[0]->id);
	FILE* fd = fopen(filename, "w+");
	for(int i = 0; i < d[0].cnt; i++)
	{
		fprintf(fd, "%s\n",name[i]);
		free(d[0].moviename[i]);
		munmap(name[i], MAX_LEN);
	}
	munmap(name, d[0].cnt*sizeof(char*));

#elif defined THREAD
	//send to sort
	for(int i = 0; i < num_of_reqs; i++)
	{
		deli[i] = malloc(d[i].base*sizeof(send));
		for(int j = 0; j < d[i].base; j++)
		{
			deli[i][j].order = num[i];
			deli[i][j].time = j;
			pthread_create(&tid[i][j], NULL, sorting, &deli[i][j]);
		}	
	}
	for(int i = 0; i < num_of_reqs; i++)
	{
		for(int j = 0; j < d[i].base; j++)
			pthread_join(tid[i][j], NULL);
	}
	//printf("after send to lib.c\n");

	//merge sorted
	for(int i = 0; i < num_of_reqs; i++)
	{
		pthread_create(&tid[i][0], NULL, merge, &num[i]);
	}
	for(int i = 0; i < num_of_reqs; i++)
	{
		pthread_join(tid[i][0], NULL);
	}
	//printf("after merge result\n");

	//write to file
	
	for(int i = 0; i < num_of_reqs; i++)
	{
		pthread_create(&tid[i][0], NULL, wr, (void*)&num[i]);
	}
	for(int i = 0; i < num_of_reqs; i++)
	{
		pthread_join(tid[i][0], NULL);
	}
	
	//printf("finish writing to file\n");
#endif
	return 0;
}

/**=======================================
 * You don't need to modify following code *
 * But feel free if needed.                *
 =========================================**/

request* read_request(){
	int id;
	char buf1[MAX_LEN],buf2[MAX_LEN];
	char delim[2] = ",";

	char *keywords;
	char *token, *ref_pts;
	char *ptr;
	double ret,sum;

	scanf("%u %254s %254s",&id,buf1,buf2);
	keywords = malloc(sizeof(char)*strlen(buf1)+1);
	if(keywords == NULL){
		ERR_EXIT("malloc");
	}

	memcpy(keywords, buf1, strlen(buf1));
	keywords[strlen(buf1)] = '\0';
	
	double* profile = malloc(sizeof(double)*NUM_OF_GENRE);
	if(profile == NULL){
		ERR_EXIT("malloc");
	}
	sum = 0;
	ref_pts = strtok(buf2,delim);
	for(int i = 0;i < NUM_OF_GENRE;i++){
		ret = strtod(ref_pts, &ptr);
		profile[i] = ret;
		sum += ret*ret;
		ref_pts = strtok(NULL,delim);
	}

	// normalize
	sum = sqrt(sum);
	for(int i = 0;i < NUM_OF_GENRE; i++){
		if(sum == 0)
				profile[i] = 0;
		else
				profile[i] /= sum;
	}

	request* r = malloc(sizeof(request));
	if(r == NULL){
		ERR_EXIT("malloc");
	}

	r->id = id;
	r->keywords = keywords;
	r->profile = profile;

	return r;
}

/*=================initialize the dataset=================*/
void initialize(FILE* fp){

	char chunk[MAX_LEN] = {0};
	char *token,*ptr;
	double ret,sum;
	int cnt = 0;

	assert(fp != NULL);

	// first row
	if(fgets(chunk,sizeof(chunk),fp) == NULL){
		ERR_EXIT("fgets");
	}

	memset(movies,0,sizeof(movie_profile*)*MAX_MOVIES);	

	while(fgets(chunk,sizeof(chunk),fp) != NULL){
		
		assert(cnt < MAX_MOVIES);
		chunk[MAX_LEN-1] = '\0';

		const char delim1[2] = " "; 
		const char delim2[2] = "{";
		const char delim3[2] = ",";
		unsigned int movieId;
		movieId = atoi(strtok(chunk,delim1));

		// title
		token = strtok(NULL,delim2);
		char* title = malloc(sizeof(char)*strlen(token)+1);
		if(title == NULL){
			ERR_EXIT("malloc");
		}
		
		// title.strip()
		memcpy(title, token, strlen(token)-1);
	 	title[strlen(token)-1] = '\0';

		// genres
		double* profile = malloc(sizeof(double)*NUM_OF_GENRE);
		if(profile == NULL){
			ERR_EXIT("malloc");
		}

		sum = 0;
		token = strtok(NULL,delim3);
		for(int i = 0; i < NUM_OF_GENRE; i++){
			ret = strtod(token, &ptr);
			profile[i] = ret;
			sum += ret*ret;
			token = strtok(NULL,delim3);
		}

		// normalize
		sum = sqrt(sum);
		for(int i = 0; i < NUM_OF_GENRE; i++){
			if(sum == 0)
				profile[i] = 0;
			else
				profile[i] /= sum;
		}

		movie_profile* m = malloc(sizeof(movie_profile));
		if(m == NULL){
			ERR_EXIT("malloc");
		}

		m->movieId = movieId;
		m->title = title;
		m->profile = profile;

		movies[cnt++] = m;
	}
	num_of_movies = cnt;

	// request
	scanf("%d",&num_of_reqs);
	assert(num_of_reqs <= MAX_REQ);
	for(int i = 0; i < num_of_reqs; i++){
		reqs[i] = read_request();
	}
}
/*========================================================*/