#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>
#include<getopt.h>
#include "cachelab.h"
#define line_max 20
typedef struct cache_line{
	int tag;
	int group;
	char *inblockAddr; 
	int ifDirty;
	int time; 
}line;

void usage(char *argv);
int getBit(int n);
char** split_line(char *file_line);
int str2hex(char *addr);
int m_read(line **cache,char *addr,int lines_per_group,int group_num,int tag_bit,int inaddr_bit);
int m_write(line **cache,char *addr,int lines_per_group,int group_num,int tag_bit,int inaddr_bit);
void replace(line **cache,int group_index,int tag,int lines_per_group);
void load_to_cache(line **cache,int group_index,int line_index,int tag);

int memory_write_time,evictions=0;
int main(int argc,char * const argv[])
{
	int block_bit;
	int lines_per_group;
	char ch,*trace_path;
	int misses,hits;
	int l,m,x,y,group_bit,group_num,tag_bit;
	line **cache;
	FILE *trace_file;
	char file_line[line_max],**op_addr_index;
	
	while ((ch = getopt(argc, argv, "hvs:E:b:t:")) != -1)
	{
		switch (ch)
		{
			case 'h':
				usage(argv[0]);
				break;
			case 'v':
				printf("useless\n");
				break;
			case 's':
				group_bit=atoi(optarg);
				group_num=pow(2,group_bit);
				break;
			case 'E':
				lines_per_group=atoi(optarg);
				break;
			case 'b':
				block_bit=atoi(optarg);
				break;
			case 't':
				trace_path=optarg;
				break;
		}
	}
	//printf("%d %d %d\n",group_num,lines_per_group,block_bit);
	trace_file = fopen(trace_path,"r");
	memory_write_time=0;
	misses=0;
	hits=0;				
	cache = (line **)malloc(sizeof(line*)*group_num);
	for(l = 0;l < group_num;l++)
	{
		cache[l] = (line *)malloc(sizeof(line)*lines_per_group);
		for(m = 0;m < lines_per_group;m++)
		{
			cache[l][m].inblockAddr = (char *)malloc(block_bit+1);
			cache[l][m].group = -1;
			cache[l][m].tag = -1;
			cache[l][m].ifDirty = 0;
			cache[l][m].time = 0;
		}
	}
	tag_bit = 32-block_bit-group_bit;
	while(fgets(file_line,line_max,trace_file))
	{
		op_addr_index = split_line(file_line);
		if(!strcmp(op_addr_index[0],"L"))
		{
			if(!m_read(cache,op_addr_index[1],lines_per_group,group_num,tag_bit,block_bit))
			misses++;
			else
			hits++;
		}
		else if(!strcmp(op_addr_index[0],"S"))
		{
			if(!m_write(cache,op_addr_index[1],lines_per_group,group_num,tag_bit,block_bit))
			misses++;
			else
			hits++;
		}
		else if(!strcmp(op_addr_index[0],"M"))
		{
			if(!m_read(cache,op_addr_index[1],lines_per_group,group_num,tag_bit,block_bit))
			misses++;
			else
			hits++;
			for(x=0;x<group_num;x++)
			{
				for(y=0;y<lines_per_group;y++)
				if(cache[x][y].group != -1)
				cache[x][y].time++;
			}
			m_write(cache,op_addr_index[1],lines_per_group,group_num,tag_bit,block_bit);
			hits++;
		}
		for(x=0;x<group_num;x++)
		{
			for(y=0;y<lines_per_group;y++)
			if(cache[x][y].group != -1)
			cache[x][y].time++;
		}
		for(x=0;x<3;x++)
		free(op_addr_index[x]);
		free(op_addr_index);
	}
	for(l=0;l<group_num;l++)
	{
		for(m=0;m<lines_per_group;m++)
		free(cache[l][m].inblockAddr);
		free(cache[l]);
	}
	free(cache);
	fclose(trace_file);
	printSummary(hits,misses,evictions);
	return 0;
}

void usage(char *argv)
{
	printf("Usage: %s [-hvsEbt]\n", argv);
	puts("Options:");
	puts("  -h    Print this help message.");
	puts("  -v    displays trace info.");
	puts("  -s    Number of set index bits");
	puts("  -E    number of lines per set");
	puts("  -b    Number of block bits");
	puts("  -t    Name of the valgrind trace to replay");
}

int getBit(int n)
{
	int i;
	for(i = 0;n/2 != 0;n /= 2)
	i++;
	return i;
}

char** split_line(char *file_line)
{
	char **n;
	int i,x,y;
	n = (char**)malloc(sizeof(char *)*3);
	for(i = 0;i < 3;i++)
	n[i] = (char *)malloc(15);
	x=0;
	if(file_line[0]==' ')
	x++;
	for(i=0;i < 3;i++)
	{
		for(y=0;file_line[x]!=' ' && file_line[x]!=',' && file_line[x]!='\n';x++,y++)
		n[i][y] = file_line[x];
		n[i][y] = '\0';
		x++;
	}
	return n;
}

int str2hex(char *addr)
{
	int i,num = 0,x;
	for(i = 0;i < strlen(addr);i++)
	{
		if(addr[i] <= '9' && addr[i] >= '0')
		x = addr[i]-'0';
		else
		x = addr[i]-'a'+10;
		num = num*16+x ;
	}
	return num;
}

int m_read(line **cache,char *addr,int lines_per_group,int group_num,int tag_bit,int inaddr_bit)
{
	int group_bit;
	int addr_d,group_index,temp,i,empty=-1;
	int tag;
	addr_d = str2hex(addr);
	group_bit = getBit(group_num);
	for(i = 1,temp = 1;i <= group_bit;i++)
	temp *= 2;
	temp--;
	group_index = (addr_d >> inaddr_bit)&temp;
	for(i = 1,temp = 1;i <= tag_bit;i++)
	temp *= 2;
	temp--;
	tag = (addr_d >> (inaddr_bit+group_bit))&temp;
	//printf("addr:%s group_index:%d tag:%d\n",addr,group_index,tag);
	for(i = 0;i < lines_per_group;i++)
	{
		if(cache[group_index][i].group != -1 && tag == cache[group_index][i].tag)
		{
			cache[group_index][i].time = 0;
			return 1;
		}
		else if(cache[group_index][i].group == -1)
		empty = i;
	}
	if(empty != -1)
	load_to_cache(cache,group_index,empty,tag);
	else
	replace(cache,group_index,tag,lines_per_group);
	return 0;
}

int m_write(line **cache,char *addr,int lines_per_group,int group_num,int tag_bit,int inaddr_bit)
{
	int group_bit;
	int addr_d,group_index,temp,i,empty=-1;
	int tag;
	addr_d = str2hex(addr);
	group_bit = getBit(group_num);
	for(i = 1,temp = 1;i <= group_bit;i++)
	temp *= 2;
	temp--;
	group_index = (addr_d >> inaddr_bit)&temp;
	for(i = 1,temp = 1;i <= tag_bit;i++)
	temp *= 2;
	temp--;
	tag = (addr_d >> (inaddr_bit+group_bit))&temp;
	//printf("addr:%s group_index:%d tag:%d\n",addr,group_index,tag);
	for(i = 0;i < lines_per_group;i++)
	{
		if(cache[group_index][i].group != -1 && tag == cache[group_index][i].tag)
		{
			if(cache[group_index][i].ifDirty == 0)
			memory_write_time++; 
			cache[group_index][i].time = 0;
			cache[group_index][i].ifDirty = 1;
			return 1;
		}
		else if(cache[group_index][i].group == -1)
		empty = i;
	}
	if(empty != -1)
	{
		memory_write_time++;
		load_to_cache(cache,group_index,empty,tag);
	} 
	else
	replace(cache,group_index,tag,lines_per_group);
	return 0;
}

void replace(line **cache,int group_index,int tag,int lines_per_group)
{
	int i,line_index;
	line_index = 0;
	for(i = 1;i < lines_per_group;i++)
	{
		if(cache[group_index][i].time > cache[group_index][line_index].time)
		line_index = i;
	}
	load_to_cache(cache,group_index,line_index,tag);
	evictions++;
}

void load_to_cache(line **cache,int group_index,int line_index,int tag)
{
	if(cache[group_index][line_index].ifDirty == 1)
	{
		memory_write_time++;
	}
	cache[group_index][line_index].tag = tag;
	cache[group_index][line_index].group = group_index;
	cache[group_index][line_index].ifDirty = 0;
	cache[group_index][line_index].time = 0;
}
