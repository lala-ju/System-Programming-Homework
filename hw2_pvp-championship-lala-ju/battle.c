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
#include <ctype.h>

int main (int argc, char* argv[]) 
{
	//printf("%d\n", argc);
	char* battle;
	pid_t pidnum;
	if(argc != 3)
	{
		printf("input error");
		perror("input error");
		exit(1);
	}

	battle = argv[1];
	pidnum = atoi(argv[2]);

	//printf("%c    %d\n", *battle, pidnum);
	char buf[512] = {};
	char pid[10] = {};
	sprintf(pid, "%d", getpid());
	char* playl;
	char* playr;
	char parent;

	//printf("%s\n", pid);
	//printf("%ld\n", strlen(pid));

	switch(*battle)
	{
		case 'A':
			parent = '0';
			playr = "C";
			playl = "B";
			break;
		case 'B':
			parent = 'A';
			playr = "E";
			playl = "D";
			break;
		case 'C':
			parent = 'A';
			playr = "14";
			playl = "F";
			break;
		case 'D':
			parent = 'B';
			playr = "H";
			playl = "G";
			break;
		case 'E':
			parent = 'B';
			playr = "J";
			playl = "I";
			break;
		case 'F':
			parent = 'C';
			playr = "L";
			playl = "K";
			break;
		case 'G':
			parent = 'D';
			playr = "1";
			playl = "0";
			break;
		case 'H':
			parent = 'D';
			playr = "3";
			playl = "2";
			break;
		case 'I':
			parent = 'E';
			playr = "5";
			playl = "4";
			break;
		case 'J':
			parent = 'E';
			playr = "7";
			playl = "6";
			break;
		case 'K':
			parent = 'F';
			playr = "10";
			playl = "M";
			break;
		case 'L':
			parent = 'F';
			playr = "13";
			playl = "N";
			break;
		case 'M':
			parent = 'K';
			playr = "9";
			playl = "8";
			break;
		case 'N':
			parent = 'L';
			playr = "12";
			playl = "11";
			break;
	}

	//log_battle[battle_id].txt
	char* filename = malloc(sizeof(char)*16);
	strcpy(filename, "log_battle");
	strcat(filename, battle);
	strcat(filename, ".txt");
	int logb = open(filename, O_CREAT | O_RDWR, 0666);
	if(logb < 0)
	{
		perror("can't open log_battle.txt\n");
		exit(1);
	}
	free(filename);

	//pipe process
	int fdl1[2]; //parent read children write
	int fdl2[2]; //parent write children read 
	int fdr1[2];
	int fdr2[2];

	//fork pid
	int statusr, statusl;
	pid_t pidr, pidl;

	for(int i = 0; i < 2; i++)
	{
		if(i == 0)
		{
			pipe(fdl1);
			pipe(fdl2);
			pidl = fork();
			if(pidl < 0)
			{
				perror("fork error: ");
				exit(1);
			}
			if(pidl == 0)
				break;
		}
		else
		{
			pipe(fdr1);
			pipe(fdr2);
			pidr = fork();
			if(pidr < 0)
			{
				perror("fork error: ");
				exit(1);
			}
			if(pidr == 0)
				break;
		}
	}

	if(pidl == 0) //left child
	{
		//printf("%d in the left child process?>?\n", getpid());
		//pipe fd
		close(fdl1[0]);
		close(fdl2[1]);
		int dch = dup2(fdl1[1], STDOUT_FILENO);
		if(dch < 0)
		{
			perror("fork error: ");
			exit(1);
		}
		dch = dup2(fdl2[0], STDIN_FILENO);
		if(dch < 0)
		{
			perror("fork error: ");
			exit(1);
		}
		close(fdl1[1]);
		close(fdl2[0]);
		
		//printf("%d in the left child process?? %c\n", getpid(),*battle);
		//exec which battle or player
		if( ! isdigit(playl[0]))
		{
			char *args[] = {"./battle", playl, pid, NULL};
			execv("./battle", args);
		}
		else	
		{
			char *args[] = {"./player", playl, pid, NULL};
			execv("./player", args);
		}
	} 
	else if(pidr == 0) //right child
	{
		//printf("%d in the right child process???\n", getpid());
		//pipe fd
		close(fdr1[0]);
		close(fdr2[1]);
		int dch = dup2(fdr1[1], STDOUT_FILENO);
		if(dch < 0)
		{
			perror("fork error: ");
			exit(1);
		}
		dch = dup2(fdr2[0], STDIN_FILENO);
		if(dch < 0)
		{
			perror("fork error: ");
			exit(1);
		}
		close(fdr1[1]);
		close(fdr2[0]);

		//printf("%d in the right child process?? %c\n", getpid(),*battle);
		//exec which battle or player
		if( ! isdigit(playr[0]))
		{
			char *args[] = {"./battle", playr, pid, NULL};
			execv("./battle", args);
		}
		else	
		{
			char *args[] = {"./player", playr, pid, NULL};
			execv("./player", args);
		}
	}
	
	//parent process
	close(fdl1[1]);
	close(fdl2[0]);
	close(fdr1[1]);
	close(fdr2[0]);

	//battle start
	bool bat = true;
	Status right, left;
	//while loop should start at here
	while (bat)
	{
		//fork process left child
		//receive left child pssm
		read(fdl1[0], &left, sizeof(Status));
		//write to log file
		sprintf(buf, "%c,%d pipe from %s,%d %d,%d,%c,%d\n", *battle, getpid(), playl, pidl, left.real_player_id, left.HP, left.current_battle_id, left.battle_ended_flag);
		write(logb, &buf, strlen(buf));

		//fork process right child
		//receive right child pssm
		read(fdr1[0], &right, sizeof(Status));
		//write to log file
		sprintf(buf, "%c,%d pipe from %s,%d %d,%d,%c,%d\n", *battle, getpid(), playr, pidr, right.real_player_id, right.HP, right.current_battle_id, right.battle_ended_flag);
		write(logb, &buf, strlen(buf));
		
		left.current_battle_id = *battle;
		right.current_battle_id = *battle;
		int atkr = right.ATK;
		int atkl = left.ATK;
		
		//boosting
		if(right.attr == 0) //FIRE
		{
			if(*battle == 'A' || *battle == 'G' || *battle == 'E' || *battle == 'M' || *battle == 'F')
			{
				atkr *= 2;
			}
		}
		else if(right.attr == 1) //GRASS
		{
			if(*battle == 'B' || *battle == 'H' || *battle == 'J' || *battle == 'K' || *battle == 'L')
			{
				atkr *= 2;
			}
		}
		else if(right.attr == 2) //WATER
		{
			if(*battle == 'D' || *battle == 'I' || *battle == 'C' || *battle == 'N')
			{
				atkr *= 2;
			}
		}

		if(left.attr == 0) //FIRE
		{
			if(*battle == 'A' || *battle == 'G' || *battle == 'E' || *battle == 'M' || *battle == 'F')
			{
				atkl *= 2;
			}
		}
		else if(left.attr == 1) //GRASS
		{
			if(*battle == 'B' || *battle == 'H' || *battle == 'J' || *battle == 'K' || *battle == 'L')
			{
				atkl *= 2;
			}
		}
		else if(left.attr == 2) //WATER
		{
			if(*battle == 'D' || *battle == 'I' || *battle == 'C' || *battle == 'N')
			{
				atkl *= 2;
			}
		}
		
		//attack
		if(right.HP < left.HP) //right attack
		{
			left.HP -= atkr;
			if(left.HP > 0)
				right.HP -= atkl;
		}
		else if(right.HP == left.HP)
		{
			if(right.real_player_id < left.real_player_id) //right attack
			{
				left.HP -= atkr;
				if(left.HP > 0)
					right.HP -= atkl;
			}
			else //left attack
			{
				right.HP -= atkl;
				if(right.HP > 0)
					left.HP -= atkr;
			}
		}
		else //left attack
		{
			right.HP -= atkl;
			if(right.HP > 0)
				left.HP -= atkr;
		}

		//check if battle end
		if(right.HP <= 0 || left.HP <= 0)
		{
			right.battle_ended_flag = 1;
			left.battle_ended_flag = 1;
		}

		//write to log file
		sprintf(buf, "%c,%d pipe to %s,%d %d,%d,%c,%d\n", *battle, getpid(), playl, pidl, left.real_player_id, left.HP, left.current_battle_id, left.battle_ended_flag);
		write(logb, &buf, strlen(buf));
		//write back to player
		write(fdl2[1], &left, sizeof(Status));
		//write to log file
		sprintf(buf, "%c,%d pipe to %s,%d %d,%d,%c,%d\n", *battle, getpid(), playr, pidr, right.real_player_id, right.HP, right.current_battle_id, right.battle_ended_flag);
		write(logb, &buf, strlen(buf));
		//write back to player
		write(fdr2[1], &right, sizeof(Status));

		if(right.HP <= 0 || left.HP <= 0)
		{
			bat = false;
			break;
		}
	}

	//wait for the lose children's death
	//change to passing mode //pass to parent
	//passing mode
	if(pidnum != 0)
	{
		if(right.HP <= 0) //right is loser 
		{
			waitpid(pidr, &statusr, 0);
			while(left.HP > 0) //check left pssm
			{
				if(left.current_battle_id == 'A' && left.battle_ended_flag == 1)
					break;
				//receive left child pssm
				read(fdl1[0], &left, sizeof(Status));
				//left.current_battle_id = *battle;
				//write to log file
				sprintf(buf, "%c,%d pipe from %s,%d %d,%d,%c,%d\n", *battle, getpid(), playl, pidl, left.real_player_id, left.HP, left.current_battle_id, left.battle_ended_flag);
				write(logb, &buf, strlen(buf));

				//write to log file
				sprintf(buf, "%c,%d pipe to %c,%d %d,%d,%c,%d\n", *battle, getpid(), parent, pidnum, left.real_player_id, left.HP, left.current_battle_id, left.battle_ended_flag);
				write(logb, &buf, strlen(buf));
				//send to parent
				write(1, &left, sizeof(Status));

				//read feedback from parent
				read(0, &left, sizeof(Status));
				//left.current_battle_id = *battle;
				//write to log file
				sprintf(buf, "%c,%d pipe from %c,%d %d,%d,%c,%d\n", *battle, getpid(), parent, pidnum, left.real_player_id, left.HP, left.current_battle_id, left.battle_ended_flag);
				write(logb, &buf, strlen(buf));

				//write to log file
				sprintf(buf, "%c,%d pipe to %s,%d %d,%d,%c,%d\n", *battle, getpid(), playl, pidl, left.real_player_id, left.HP, left.current_battle_id, left.battle_ended_flag);
				write(logb, &buf, strlen(buf));
				//write back to child
				write(fdl2[1], &left, sizeof(Status));
			}
			waitpid(pidl, &statusl, 0);
			close(fdr2[1]);
			close(fdr1[0]);
			close(fdl2[1]);
			close(fdl1[0]);

			exit(0); //the remaining child is dead
		}
		else //left is the loser
		{
			waitpid(pidl, &statusl, 0);
			while(right.HP > 0) //chck right pssm
			{
				if(right.current_battle_id == 'A' && right.battle_ended_flag == 1)
					break;
				//receive right child pssm
				read(fdr1[0], &right, sizeof(Status));
				//right.current_battle_id = *battle;
				//write to log file
				sprintf(buf, "%c,%d pipe from %s,%d %d,%d,%c,%d\n", *battle, getpid(), playr, pidr, right.real_player_id, right.HP, right.current_battle_id, right.battle_ended_flag);
				write(logb, &buf, strlen(buf));

				//write to log file
				sprintf(buf, "%c,%d pipe to %c,%d %d,%d,%c,%d\n", *battle, getpid(), parent, pidnum, right.real_player_id, right.HP, right.current_battle_id, right.battle_ended_flag);
				write(logb, &buf, strlen(buf));
				//send to parent
				write(1, &right, sizeof(Status));

				//read feedback from parent
				read(0, &right, sizeof(Status));
				//right.current_battle_id = *battle;
				//write to log file
				sprintf(buf, "%c,%d pipe from %c,%d %d,%d,%c,%d\n", *battle, getpid(), parent, pidnum, right.real_player_id, right.HP, right.current_battle_id, right.battle_ended_flag);
				write(logb, &buf, strlen(buf));

				//write to log file
				sprintf(buf, "%c,%d pipe to %s,%d %d,%d,%c,%d\n", *battle, getpid(), playr, pidr, right.real_player_id, right.HP, right.current_battle_id, right.battle_ended_flag);
				write(logb, &buf, strlen(buf));
				//write back to child
				write(fdr2[1], &right, sizeof(Status));
			}
			waitpid(pidr, &statusr, 0);
			close(fdl2[1]);
			close(fdl1[0]);
			close(fdr2[1]);
			close(fdr1[0]);
			
			exit(0); //the remaining child is dead
		}
	}
	else
	{
		if(right.HP > 0)
		{
			//print champion message
			sprintf(buf, "Champion is P%d\n", right.real_player_id);
			write(1, &buf, strlen(buf));
			waitpid(pidr, &statusr, 0);
			waitpid(pidl, &statusl, 0);
		}
		else
		{
			//print champion message
			sprintf(buf, "Champion is P%d\n", left.real_player_id);
			write(1, &buf, strlen(buf));
			waitpid(pidl, &statusl, 0);
			waitpid(pidr, &statusr, 0);
		}
		close(fdl2[1]);
		close(fdl1[0]);
		close(fdr2[1]);
		close(fdr1[0]);
		
		exit(0);
	}
		
	exit(0);
}