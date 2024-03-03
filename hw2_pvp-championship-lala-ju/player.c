#include "status.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

int main (int argc, char*argv[]) 
{
	if(argc != 3)
	{
		printf("input error");
		perror("input error");
		exit(1);
	}

	int player = atoi(argv[1]);
	pid_t parent = atoi(argv[2]);
	char par;
	switch(player)
	{
		case 0:
			par = 'G';
			break;
		case 1:
			par = 'G';
			break;
		case 2:
			par = 'H';
			break;
		case 3:
			par = 'H';
			break;
		case 4:
			par = 'I';
			break;
		case 5:
			par = 'I';
			break;
		case 6:
			par = 'J';
			break;
		case 7:
			par = 'J';
			break;
		case 8:
			par = 'M';
			break;
		case 9:
			par = 'M';
			break;
		case 10:
			par = 'K';
			break;
		case 11:
			par = 'N';
			break;
		case 12:
			par = 'N';
			break;
		case 13:
			par = 'L';
			break;
		case 14:
			par = 'C';
			break;
	}
	
	char buf[512] = {};
	Status P;
	int logp; //fd for log file
	//record its pssm
	if(player >= 0 && player <= 7) // real player
	{
		//log_player[player_id].txt
		char* filename = malloc(sizeof(char)*16);
		strcpy(filename, "log_player");
		strcat(filename, argv[1]);
		strcat(filename, ".txt");
		logp = open(filename, O_CREAT | O_RDWR | O_APPEND, 0666);
		if(logp < 0)
		{
			perror("can't open log_player.txt");
			exit(1);
		}
		free(filename);

		//read status
		//int list = 0;
		FILE* list = fopen("player_status.txt", "r");
		if(list == NULL)
		{
			perror("open play_status.txt error");
			exit (1);
		}
			

		//get PSSM
		P.real_player_id = player;
		int hp;
		int atk;
		char att[10];
		char cur;
		int fl;
		for(int i = 0; i <= player; i++)
		{
			fscanf(list ,"%d %d %s %c %d\n", &hp, &atk, att, &cur, &fl);
		}

		//printf("%d %d %s %c %d\n", hp, atk, att, cur, fl);
		P.HP = hp;
		P.ATK = atk;
		if(strcmp(att, "FIRE") == 0)
			P.attr = FIRE;
		else if(strcmp(att, "GRASS") == 0)
			P.attr = GRASS;
		else if(strcmp(att, "WATER") == 0)
			P.attr = WATER;
		P.current_battle_id = cur;
		P.battle_ended_flag = fl;
		//printf("%d %d %d %c %d\n", P.HP, P.ATK, P.attr, P.current_battle_id, P.battle_ended_flag);
	}
	else
	{
		//create fifo
		char* fifoname;
		if(player == 8 || player == 9)
			fifoname = malloc(sizeof(char)*13);
		else
			fifoname = malloc(sizeof(char)*14);
			
		strcpy(fifoname, "player");
		strcat(fifoname, argv[1]);
		strcat(fifoname, ".fifo");
		int mk = mkfifo(fifoname, 0666);
		if(mk < 0)
		{
			if(errno != EEXIST)
			{
				perror("mkfifo() error\n");
				exit(1);
			}
		}
		int fif = open(fifoname, O_RDONLY);
		if(fif < 0)
		{
			perror("can't open fifo\n");
			exit(1);
		}
		free(fifoname);
		
		//block at reading
		read(fif, &P, sizeof(Status));
		char* fn = malloc(sizeof(char)*2);
		sprintf(fn, "%d", P.real_player_id);
		//log_player[player_id].txt
		char* filename = malloc(sizeof(char)*16);
		strcpy(filename, "log_player");
		strcat(filename, fn);
		strcat(filename, ".txt");
		logp = open(filename, O_WRONLY | O_APPEND);
		if(logp < 0)
		{
			perror("can't open log_player.txt\n");
			exit(1);
		}
		free(filename);
		free(fn);

		//write log file after receive from sibling
		sprintf(buf, "%d,%d fifo from %d %d,%d\n", player, getpid(), P.real_player_id, P.real_player_id, P.HP);
		write(logp, &buf, strlen(buf));
		close(fif);
	}

	//original hp
	int hp = P.HP;
	
	while(true)
	{
		while(P.battle_ended_flag != 1) //not yet finish
		{
			//write to log file before sending pssm
			sprintf(buf, "%d,%d pipe to %c,%d %d,%d,%c,%d\n", player, getpid(), par, parent, P.real_player_id, P.HP, P.current_battle_id, P.battle_ended_flag);
			write(logp, &buf, strlen(buf));
			//send back to parent
			write(STDOUT_FILENO, &P, sizeof(Status));

			//receive from parent
			read(STDIN_FILENO, &P, sizeof(Status)); 
			//write to log file after receive from parent
			sprintf(buf, "%d,%d pipe from %c,%d %d,%d,%c,%d\n", player, getpid(), par, parent, P.real_player_id, P.HP, P.current_battle_id, P.battle_ended_flag);
			write(logp, &buf, strlen(buf));
		}

		//after battle end
		P.battle_ended_flag = 0;
		if(P.HP <= 0) //lose in the battle
		{
			//zone A
			char* agent;
			if(P.current_battle_id == 'B' || P.current_battle_id == 'D' || P.current_battle_id == 'E' || P.current_battle_id == 'G' ||P.current_battle_id == 'H' || P.current_battle_id == 'I' || P.current_battle_id == 'J')
			{
				P.HP = hp;
				switch(P.current_battle_id)
				{   
					//send pssm to corespond agent player
					case 'B' :
						agent = "14";
						break;
					case 'D' :
						agent = "10";
						break;
					case 'E' :
						agent = "13";
						break;
					case 'G' :
						agent = "8";
						break;
					case 'H' :
						agent = "11";
						break;
					case 'I' :
						agent = "9";
						break;
					case 'J' :
						agent = "12";
						break;
				}
				//write log file before send to sibling
				sprintf(buf, "%d,%d fifo to %s %d,%d\n", P.real_player_id, getpid(), agent, P.real_player_id,P.HP);
				write(logp, &buf, strlen(buf));

				//write to fifo
				char* fifoname;
				if(strcmp(agent, "8") == 0 || strcmp(agent, "9") == 0)
					fifoname = malloc(sizeof(char)*13);
				else	
					fifoname = malloc(sizeof(char)*14);
				strcpy(fifoname, "player");
				strcat(fifoname, agent);
				strcat(fifoname, ".fifo");
				
				int mk = mkfifo(fifoname, 0666);
				if(mk < 0)
				{
					if(errno != EEXIST)
					{
						perror("mkfifo() error\n");
						exit(1);
					}
				}
				int agentfi = open(fifoname, O_WRONLY);
				if(agentfi < 0)
				{
					perror("can't open agent fifo\n");
					exit(1);
				}
				write(agentfi, &P, sizeof(Status));
				free(fifoname);
				close(agentfi);

				exit(0);
			}
			else //zone b 
			{
				exit(0);
			}
				
		}
		else //winner
		{
			char a = 'A';
			if(P.current_battle_id == a) //last battle
			{
				exit(0);
			}
			//hp recover //battle end flag recover
			P.HP += (hp - P.HP)/2;
		}
	}
	exit(0);
}