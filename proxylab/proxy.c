#include <stdio.h>
#include<pthread.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n\0";
char *object_cache[256]={0},*object_names[256];
int cache_size=0,used_count[256],object_size[256];
pthread_rwlock_t rwlock;

void do_proxy(int connfd);
void send_error(int connfd);
int in_cache(char *object_name);
int if_cache_full();
int LRU();

int main(int argc, char **argv) 
{
	int listenfd, connfd;
	char hostname[MAXLINE], port[MAXLINE];
	socklen_t client_len;
	pthread_t th;
	struct sockaddr_storage client_addr;

	/* Check command line args */
	if (argc != 2)
	{
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}

    	listenfd = Open_listenfd(argv[1]);
	client_len = sizeof(client_addr);
    	while(1)
	{
		connfd = Accept(listenfd, (SA *)&client_addr, &client_len);
		Getnameinfo((SA *)&client_addr,client_len,hostname,MAXLINE,port,MAXLINE,0);
		printf("Accepted connection from (%s, %s)\n",hostname,port);
		Pthread_create(&th,NULL,do_proxy,connfd);
    	}
}

void do_proxy(int connfd)
{
	rio_t rp,rp_c;
	int client_fd,count,object_name_len,index,sign=0;
	int status,if_http=0,if_host=0,if_userAgent=0,if_conn=0,if_p_conn=0,if_length=0;
	char *object_name,*pos,*port_pos,hostname[128],port[10]="80";
	char buf[MAXLINE],*temp;
	char request[MAXLINE];
	rio_readinitb(&rp,connfd);
	request[0]='\0';
	while(status=rio_readlineb(&rp,buf,MAXLINE))
	{
		if(status<0)
		{
			printf("error1\n");
			send_error(connfd);
			return;
		}
		if(!if_http && (pos=strstr(buf,"HTTP/1.1")))//change to HTTP/1.0
		{
			*(pos+7)='0';
			for(count=0;*(buf+count)!=' ' && (buf+count)<pos;count++);
			object_name_len=strlen(buf)-10-count;
			object_name=(char *)malloc(object_name_len);
			strncpy(object_name,buf+count+1,object_name_len-1);
			*(object_name+object_name_len-1)='\0';
			for(;*pos!='/' && pos!=buf;pos--);
			if(pos!=buf)
			strcpy(buf+count+1,pos);
			if_http=1;
		}
		else if(!if_host && (pos=strstr(buf,"Host")))
		{
			if(port_pos=strchr(pos+6,':'))
			{
				strcpy(port,port_pos+1);
				port[strlen(port)-2]='\0';
				*port_pos='\0';
				strcpy(hostname,pos+6);
				*port_pos=':';
			}
			else
			{
				strcpy(hostname,pos+6);
				hostname[strlen(hostname)-2]='\0';
			}
			if_host=1;
		}
		else if(!if_userAgent && (pos=strstr(buf,"User-Agent")))
		{
			strcpy(pos,user_agent_hdr);
			if_userAgent=1;
		}
		else if(!if_p_conn && (pos=strstr(buf,"Proxy-Connection")))
		{
			strcpy(pos+18,"close\r\n");
			if_p_conn=1;
		}
		else if(!if_conn && (pos=strstr(buf,"Connection")))
		{
			strcpy(pos+12,"close\r\n");
			if_conn=1;
		}
		if(buf[0]=='\r' && buf[1]=='\n')
		break;
		strcat(request,buf);
	}
	if(!if_conn)
	strcat(request,"Connection: close\r\n");
	if(!if_p_conn)
	strcat(request,"Proxy-Connection: close\r\n");
	strcat(request,"\r\n");

	pthread_rwlock_rdlock(&rwlock);
	index=in_cache(object_name);
	if(index!=256)//check requested object if in cache
	{
		rio_writen(connfd,object_cache[index],object_size[index]);
		pthread_rwlock_unlock(&rwlock);
		pthread_rwlock_wrlock(&rwlock);
		used_count[index]++;
		pthread_rwlock_unlock(&rwlock);
		free(object_name);
	}
	else//not in cache
	{
		pthread_rwlock_unlock(&rwlock);
		client_fd=open_clientfd(hostname,port);
		if(client_fd<0)
		{
			free(object_name);
			printf("error2\n");
			puts(hostname);
			puts(port);
			send_error(connfd);
			return;
		}
	
		rio_writen(client_fd,request,strlen(request));
		rio_readinitb(&rp_c,client_fd);
		request[0]='\0';
		while(status=rio_readlineb(&rp_c,buf,MAXLINE))//get header
		{
			if(status<0)
			{
				printf("error3\n");
				free(object_name);
				send_error(connfd);
				return;
			}
			if(!if_length && (pos=strstr(buf,"Content-length:")))
			{
				count=atoi(pos+16);//content length
				if_length=1;
			}
			rio_writen(connfd,buf,strlen(buf));
			if(buf[0]=='\r' && buf[1]=='\n')
			break;
		}
		if(count+object_name_len<=MAX_OBJECT_SIZE)
		{
			temp=(char*)malloc(count);
			if(temp)
			sign=1;
			else
			printf("malloc error\n");
		}
		while(status=rio_readnb(&rp_c,buf,MAXLINE))//get content
		{
			if(status<0)
			{
				free(object_name);
				send_error(connfd);
				return;
			}
			rio_writen(connfd,buf,status);
			if(sign)
			{
				memcpy(temp,buf,status);
				temp+=status;
			}
		}
		if(sign)//add into cache
		{
			temp-=count;
			pthread_rwlock_wrlock(&rwlock);
			while(cache_size+count+object_name_len>MAX_CACHE_SIZE || (index=if_cache_full())==256)
			{
				index=LRU();
				free(object_names[index]);
				object_names[index]=NULL;
				cache_size-=sizeof(object_cache[index]);
				free(object_cache[index]);
				object_cache[index]=NULL;
				used_count[index]=0;
			}
			object_cache[index]=temp;
			object_names[index]=object_name;
			object_size[index]=count;
			used_count[index]=1;
			cache_size+=count+object_name_len;
			pthread_rwlock_unlock(&rwlock);
		}
		else
		free(object_name);
	}
}
	
void send_error(int connfd)
{
	rio_writen(connfd,"error\r\n",7);
}

int in_cache(char *object_name)
{
	int i;
	for(i=0;i<256;i++)
	{
		if(object_names[i] && !strcmp(object_name,object_names[i]))
		break;
	}
	return i;
}

int if_cache_full()
{
	int i;
	for(i=0;i<256;i++)
	{
		if(!object_names[i])
		break;
	}
	return i;
}

int LRU()
{
	int i,min=1<<30,min_index;
	for(i=0;i<256;i++)
	{
		if(used_count[i] && used_count[i]<min)
		{
			min_index=i;
			min=used_count[i];
		}
	}
	return min_index;
}
