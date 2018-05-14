#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <regex>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <map>
#include <arpa/inet.h>
#include <netdb.h>

#define MAXLINE 10001
#define BUFSIZE 4096
#define QLEN 5

void init()
{
	char* home = getenv("HOME");
	chdir(home);
	chdir("rwg");
	char* n = get_current_dir_name();
	printf("server: %s\n",n);
	setenv("PATH", "bin:.", 1);
	return;
}


void welcome_msg(int sockfd)
{
	int n;
	char wm[201] = {"****************************************\n** Welcome to the information server. **\n****************************************\n"};
	n = send(sockfd, wm, strlen(wm), 0);
	if(n < 0) printf("server: send error\n");
	return;
}

void line_promt(int sockfd)
{
	int n;
	char line[5] = {"% "};
	n = send(sockfd, line, strlen(line), 0);
	if(n < 0) printf("server: send error\n");
	return;
}


class cli
{
public:
	cli* cli_id;
	char name[21] = "(no name)";
	int live_flg;
	int sockfd;
	char *addr;
	int port;
	int id;

	int name_time;

	char yell[10001];

	int n, i, k, line_i, read_i, j;
	char cmd_readbuffer[MAXLINE];
	char tmpbuffer[MAXLINE];
	char line[MAXLINE];
	//std::string err_msg1("The command which includes '/' is illegal.\n");
	//char err_msg1[] = "The command which includes '/' is illegal.\n";
	// char test_msg1[1001] = {"Input recieved.\n"};
	char program[257];
	char tmp[257][257];
	char* arg[257];
	int redirection; // 0 if don't need , 1 if need
	char re_dir[257];
	int pipefd_1[2], pipefd_2[2];
	int pipe_mode; // '|' -> pipe_mode = 0 , '|n' (n = 1-1000) pipe_mode = 1 , close -> -1
	int pipe_number; // put output data to which buffer
	int pipe_index; // index of which buffer is use now
	std::string pipebuffer[1001];
	std::string pipe0buffer;
	int pipe_flg[1001]; // 0 if pipe is empty  1 for something inside in mode0, 2 for something inside in mode1 , 3 for both mode0 and mode1
	int cmdc;
	int error_in_line;

	std::string pipefrom[30];
	int pipeobj;

	int pipefrom_u;

	std::string preline;

	std::map<std::string, std::string> env_map;
	cli()
	{
		sockfd = -1;
		live_flg = 0;

		memset(cmd_readbuffer, 0, sizeof(cmd_readbuffer));
		memset(line, 0, sizeof(line));

		for(i = 0;i < 1001; i++) pipe_flg[i] = 0;

		pipe_mode = -1;
		pipe_number = 0;
		pipe_index = 0;

		for(i = 0;i < 30;i++) pipefrom[i].clear();
		pipeobj = -1;
		pipefrom_u = -1;

		env_map.clear();
		env_map[std::string("PATH")] = std::string("bin:.");

		n = 0;
		name_time = 0;
	}
	void name_reset()
	{
		strcpy(name,"(no name)");
	}

	void set(int fd)
	{
		sockfd = fd;
	}

	void reset()
	{
		name_reset();
		name_time = 0;

		memset(cmd_readbuffer, 0, sizeof(cmd_readbuffer));
		memset(line, 0, sizeof(line));

		for(int i = 0;i < 1001; i++) pipe_flg[i] = 0;

		pipe_mode = -1;
		pipe_number = 0;
		pipe_index = 0;
		
		pipeobj = -1;
		pipefrom_u = -1;
		for(int i = 0;i < 30;i++)
		{
			pipefrom[i].clear();
			cli_id[i].pipefrom[id].clear();
		}

		env_map.clear();
		env_map[std::string("PATH")] = std::string("bin:.");

		n = 0;
	}

	void env_set()
	{
		clearenv();
		for(auto& p:env_map)
		{
			std::string str1(p.first.c_str());
			std::string str2(env_map[str1].c_str());
			printf("env_set:%s %s\n",str1.c_str(),str2.c_str());
			setenv(str1.c_str(), str2.c_str(), 1);
		}
	}

	void env_reset()
	{
		printf("env_reset\n");
		clearenv();
		setenv("PATH","bin:.",1);
		return;
	}

	int shell()
	{
			env_set();

		//for(;;)
		//{	
			cmdc = 0;
			n = 0;
			k = strlen(cmd_readbuffer);
			j = strlen(line);
			//printf("j : %d\n",j);
			if(k) for(i = 0; cmd_readbuffer[i-1]!='\n' && cmd_readbuffer[i]!='\0';i++) line[i+j] = cmd_readbuffer[i];
			k = recv(sockfd, cmd_readbuffer, sizeof(cmd_readbuffer), 0);
			if(k <= 0)
			{
				if(errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
				{
					printf("server: receive error\n");
					return 0;
				}
				else return -1;
			}
			j = strlen(line);
			for(i = 0; cmd_readbuffer[i-1]!='\n' && cmd_readbuffer[i]!='\0';i++) line[i+j] = cmd_readbuffer[i];
			line[i+j] = '\0';
			n = strlen(line);
			//printf("n : %d\n",n);
			if (cmd_readbuffer[i]!='\0')
			{
				j = i;
				for(i = 0;cmd_readbuffer[i+j]!='\0';i++)
				{
					cmd_readbuffer[i] = cmd_readbuffer[i+j];
				}
			}
			else memset(cmd_readbuffer, 0, sizeof(cmd_readbuffer));
			printf("%s\n",line);
			if(line[n-1] != '\n') return 2;
			line[n - 1] = '\0';
			preline = std::string(line);
			int f = preline.find('\r');
			if(f != -1) preline.erase(f);
			f = preline.find_first_not_of(" \t");
			if(f != -1) preline = preline.substr(f);
			//printf("strlen : %d\n",strlen(line));
			n--;
			if(n < 0) printf("server: receive error\n");
			else if(n == 0) return -1;
			else
			{
				//printf("n : %d\n",n);
				//printf("%s\n",line);
				error_in_line = 0;
				for(i = 0;i <= n;i++) // test if there is any '/' in input
				{
					if(line[i] == '/')
					{
						std::string err_msg1("The command which includes '/' is illegal.\n");
						k = send(sockfd, err_msg1.c_str(), strlen(err_msg1.c_str()), 0);
						if(k < 0) printf("server: send error\n");
						break;
					}
				}
				if(i == n+1)
				{
					//k = send(sockfd, test_msg1, strlen(test_msg1), 0);
					//if(k < 0) printf("server: send error\n");

					// split readline by space
					std::regex re("\\s+");
					std::string s(line);
					std::vector<std::string> splited_command
					{
						std::sregex_token_iterator(s.begin(), s.end(), re, -1), {}
					};

					k = splited_command.size();
					//printf("k : %d\n",k);
					int cnt, l;
					cnt = 0;
					pipe_number = 0;
					pipe_mode = -1;
					for(i = 0;i < k;i++)
					{
						//printf("%s\n",splited_command[i].c_str());
						if(strcmp(program,"yell") && strcmp(program,"tell") && (splited_command[i][0] == '|' || splited_command[i][0] == '>' || splited_command[i][0] == '<'))
						{
							cmdc++;
							//for(l = 0;l <= 1000;l++) if(pipe_flg[l] != 0) printf("%d:%d\n",l,pipe_flg[l]);
							pipe(pipefd_1);
							pipe(pipefd_2);
							if(splited_command[i][0] == '|')
							{
								if(splited_command[i].size() == 1)
								{
									pipe_mode = 0;
									pipe_number = 1;
									if(pipe_flg[(pipe_index+1)%1001] == 2 || pipe_flg[(pipe_index+1)%1001] == 3) pipe_flg[(pipe_index+1)%1001] = 3;
									else pipe_flg[(pipe_index+1)%1001] = 1;
								}
								else
								{
									pipe_number = 0;
									for(l = 1;l < splited_command[i].size();l++) pipe_number = pipe_number * 10 + ( splited_command[i][l] - '0' );
									//printf("pipe_number: %d\n",pipe_number);
									pipe_mode = 1;
									if(pipe_flg[(pipe_index+pipe_number)%1001] == 1 || pipe_flg[(pipe_index+pipe_number)%1001] == 1) pipe_flg[(pipe_index+pipe_number)%1001] = 3;
									else pipe_flg[(pipe_index+pipe_number)%1001] = 2;
								}
							}
							else if(splited_command[i][0] == '>')
							{
								if(splited_command[i].size() == 1)
								{
									redirection = 1;
									strcpy(re_dir, splited_command[i+1].c_str()); 
									i++;
								}
								else
								{
									std::string str = splited_command[i].substr(1);
									pipeobj = std::stoi(str) - 1;
									printf("%d pipe to %d\n",id, pipeobj);
									if(!cli_id[pipeobj].live_flg)
									{
									
										std::string msg = std::string("*** Error: user #") + std::to_string(pipeobj+1) + std::string(" does not exist yet. ***\n");
										int n = send(sockfd,msg.c_str(),strlen(msg.c_str()),0);
										if(n < 0) printf("server: send error\n");
										error_in_line = 1;

										pipeobj = -1;
									}
									else if(cli_id[pipeobj].pipefrom[id].size() > 0)
									{
										std::string msg = std::string("*** Error: the pipe #") + std::to_string(id+1) + std::string("->#") + std::to_string(pipeobj+1) + std::string(" already exists. ***\n");
										int n = send(sockfd,msg.c_str(),strlen(msg.c_str()),0);
										if(n < 0) printf("server: send error\n");
										pipeobj = -1;
										error_in_line = 1;
									}
								}
							}
							else
							{
								std::string str = splited_command[i].substr(1);
								pipefrom_u = std::stoi(str) - 1;
								if(!pipefrom[pipefrom_u].size())
								{
									std::string msg = std::string("*** Error: the pipe #") + std::to_string(pipefrom_u+1) + std::string("->#") + std::to_string(id+1) + std::string(" does not exist yet. ***\n");
									int n = send(sockfd,msg.c_str(),strlen(msg.c_str()),0);
									if(n < 0) printf("server: send error\n");
									error_in_line = 1;
									pipefrom_u = -1;
								}
							}
							if(((pipefrom_u != -1 || pipeobj != -1) && pipe_mode == -1 && redirection == 0 && pipe_flg[pipe_index] == 0) || error_in_line)
							{
								close(pipefd_1[0]);
								close(pipefd_1[1]);
								close(pipefd_2[0]);
								close(pipefd_2[1]);
							}
							else if(strlen(program) != 0)
							{
								arg[cnt] = NULL;
								pid_t pid;
								if(!strcmp(program,"printenv"))
								{
									close(pipefd_1[0]);
									close(pipefd_1[1]);
									close(pipefd_2[0]);
									close(pipefd_2[1]);
									//char* pPath;
									std::string s1(arg[1]);
									//std::string s2(pPath);
									std::string s3 = s1 + "=" + env_map[std::string(arg[1])] + "\n";
									l = send(sockfd, s3.c_str(), strlen(s3.c_str()), 0);
								}
								else if(!strcmp(program,"setenv"))
								{
									close(pipefd_1[0]);
									close(pipefd_1[1]);
									close(pipefd_2[0]);
									close(pipefd_2[1]);
									//printf("se\n");
									env_map[std::string(arg[1])] = std::string(arg[2]);
								}
								else if(!strcmp(program,"exit")) 
								{
									close(pipefd_1[0]);
									close(pipefd_1[1]);
									close(pipefd_2[0]);
									close(pipefd_2[1]);
									return 0;
								}
								else if(!strcmp(program,"who"))
								{
									close(pipefd_1[0]);
									close(pipefd_1[1]);
									close(pipefd_2[0]);
									close(pipefd_2[1]);
									memset(line,0,sizeof(line));
									return 3;
								}
								else if((pid = fork()) < 0)
								{
								close(pipefd_1[0]);
								close(pipefd_1[1]);
								close(pipefd_2[0]);
								close(pipefd_2[1]);
									printf("server: fork error\n");
									exit(-1);
								}
								else if(pid == 0)
								{
									dup2(sockfd, STDERR_FILENO);
									//printf("Child: %s\n",program);
									//printf("pipe_mode: %d\n",pipe_mode);
									//printf("redirection: %d\n",redirection);
									printf("pipe_index: %d\n",pipe_index);
									if(pipe_flg[pipe_index] || pipefrom_u != -1)
									{
										// child read from pipefd_1
										//printf("Child: read pipe %d\n",pipefd_1[0]);
										close(pipefd_1[1]);
										dup2(pipefd_1[0],0);
										close(pipefd_1[0]);
									}
									else
									{
										close(pipefd_1[0]);
										close(pipefd_1[1]);
									}
									if(pipe_mode == -1 && redirection == 0 && pipeobj == -1)
									{
										//printf("Child: dup2 sockfd\n");
										close(pipefd_2[0]);
										close(pipefd_2[1]);
										dup2(sockfd, STDOUT_FILENO);
									}
									else if(redirection)
									{
										//printf("Child: redirection\n");
										close(pipefd_2[0]);
										close(pipefd_2[1]);
										int fd;
										fd = creat(re_dir, 0644);
										dup2(fd, STDOUT_FILENO);
										close(fd);
									}
									else
									{
										// child write to pipefd_2
										//printf("Child: write pipe %d\n",pipefd_2[1]);
										close(pipefd_2[0]);
										dup2(pipefd_2[1],1);
										close(pipefd_2[1]);
									}
									if(execvp(program, arg) == -1)
									{
										if(redirection == 0 && pipe_mode == -1) close(STDOUT_FILENO);
										if(errno == ENOENT)
										{
											char msg1[] = {"Unknown command: ["};
											char msg2[] = {"].\n"};
											std::string msg = std::string(msg1) + std::string(program) + std::string(msg2);
											l = send(sockfd, msg.c_str(), strlen(msg.c_str()), 0);
											if(l < 0) printf("sever: send error\n");
										}
										else
										{
											char* msg = strerror(errno);
											l = send(sockfd, msg, strlen(msg), 0);
											if(l < 0) printf("sever: send error\n");
										}	
										exit(1);
									}
								}
								else
								{
									//printf("server: parent\n");
									printf("pipe_flg: %d\n",pipe_flg[pipe_index]);
									if(pipe_flg[pipe_index] || pipefrom_u != -1)
									{
										// parent write to pipefd_1
										//printf("server: write pipe %d\n",pipefd_1[1]);
										close(pipefd_1[0]);
										if(pipe_flg[pipe_index] == 2) 
										{
										//	printf("%s",pipebuffer[pipe_index].c_str());
											write(pipefd_1[1], pipebuffer[pipe_index].c_str(), strlen(pipebuffer[pipe_index].c_str()));
										}
										else if(pipe_flg[pipe_index] == 1)
										{
										//	printf("%s",pipe0buffer.c_str());
											write(pipefd_1[1], pipe0buffer.c_str(), strlen(pipe0buffer.c_str()));
											pipe0buffer.clear();
										}
										else if(pipe_flg[pipe_index])
										{
											std::string buffer = pipebuffer[pipe_index] + pipe0buffer;
										//	printf("%s",buffer.c_str());
											write(pipefd_1[1], buffer.c_str(), strlen(buffer.c_str()));
											pipe0buffer.clear();
										}
										else
										{
											write(pipefd_1[1], pipefrom[pipefrom_u].c_str(), strlen(pipefrom[pipefrom_u].c_str()));
											pipefrom[pipefrom_u].clear();
											std::string msg = std::string("*** ") + std::string(cli_id[id].name) + std::string(" (#") + std::to_string(id+1);
											std::string msg2 = std::string(") just received from ") + std::string(cli_id[cli_id[id].pipefrom_u].name) + std::string(" (#");
											std::string msg3 = std::to_string(cli_id[id].pipefrom_u+1) + std::string(") by '") + cli_id[id].preline + std::string("' ***\n");
											std::string msg4 = msg + msg2 + msg3;
											//const char* m = msg4.c_str();
											//broadcast(cli_id,msg4);
											printf("%s",msg4.c_str());
											for(int i = 0;i < 30;i++)
											{
												if(cli_id[i].live_flg)
												{
													int n = send(cli_id[i].sockfd,msg4.c_str(),strlen(msg4.c_str()), 0);
													if(n < 0) printf("server: send error\n");
												}
											}
											printf("server: broadcast\n");
											pipefrom_u = -1;
										}
										close(pipefd_1[1]);
									}
									else
									{
										close(pipefd_1[1]);
										close(pipefd_1[0]);
									}
									printf("pipe_mode: %d\n",pipe_mode);
									if(pipe_mode == -1 && pipeobj == -1)
									{
										close(pipefd_2[0]);
										close(pipefd_2[1]);
									}
									else
									{
										char readbuffer[65537];
										// parent read from pipifd_2
										//printf("server: read pipe %d\n",pipefd_2[0]);
										close(pipefd_2[1]);
										l = read(pipefd_2[0], readbuffer, sizeof(readbuffer));
										readbuffer[l] = '\0';
										//printf("%s",readbuffer);
										if(pipe_mode == 0) pipe0buffer = std::string(readbuffer);
										else if(pipe_mode == 1) pipebuffer[(pipe_index+pipe_number)%1001] = pipebuffer[(pipe_index+pipe_number)%1001] + std::string(readbuffer);
										else cli_id[pipeobj].pipefrom[id] = std::string(readbuffer);
										close(pipefd_2[0]);
									}

									int status;								
									wait(&status);
								printf("server: receive waiting status\n");
									pipe_mode = -1;
									if(!status) // check if child has any error
									{
										//printf("server: child exit without error\n");
										pipe_flg[pipe_index] = 0;
											pipebuffer[pipe_index].clear();
										pipe_index++;
										if(pipe_index > 1000) pipe_index = 0;
										redirection = 0;
									}
									else
									{
										printf("server: child exit with error\ni = %d\n",i);
										if(cmdc == 0)
										{
											pipe_flg[pipe_index] = 0;
											pipebuffer[pipe_index].clear();
											pipe_index++;
											if(pipe_index > 1000) pipe_index = 0;
										}
										redirection = 0;
										error_in_line = 1;
										break;
									}
								}
								cnt = 0;
								program[0] = '\0';
							}
						}
						else
						{
							for(l = 0;l < splited_command[i].size();l++)
							{
								//printf("%d ", l);
								if(cnt == 0) program[l] = splited_command[i][l];
								tmp[cnt][l] = splited_command[i][l];
							}
							//printf("\n");
							if(cnt == 0)program[l] = '\0';
							tmp[cnt][l] = '\0';
							arg[cnt] = tmp[cnt];
							cnt++;
						}
					} //end of for(k)
					if(error_in_line);
					else if(strlen(program) != 0)
					{
						
							//for(l = 0;l <= 1000;l++) if(pipe_flg[l] != 0) printf("%d:%d\n",l,pipe_flg[l]);
							arg[cnt] = NULL;
							pipe(pipefd_1);
							pipe(pipefd_2);
							pid_t pid;
							if(!strcmp(program,"printenv"))
							{
								close(pipefd_1[0]);
								close(pipefd_1[1]);
								close(pipefd_2[0]);
								close(pipefd_2[1]);
								std::string s1(arg[1]);
								//std::string s2(pPath);
								std::string s3 = s1 + "=" + env_map[std::string(arg[1])] + "\n";
								l = send(sockfd, s3.c_str(), strlen(s3.c_str()), 0);
							}
							else if(!strcmp(program,"setenv"))
							{
								//printf("se\n");
								char* pPath;
								close(pipefd_1[0]);
								close(pipefd_1[1]);
								close(pipefd_2[0]);
								close(pipefd_2[1]);
								env_map[std::string(arg[1])] = std::string(arg[2]);
							}
							else if(!strcmp(program,"exit"))
							{
								close(pipefd_1[0]);
								close(pipefd_1[1]);
								close(pipefd_2[0]);
								close(pipefd_2[1]);
								return 0;
							}
							else if(!strcmp(program,"who"))
							{
								close(pipefd_1[0]);
								close(pipefd_1[1]);
								close(pipefd_2[0]);
								close(pipefd_2[1]);
								memset(line,0,sizeof(line));
								return 3;
							}
							else if(!strcmp(program,"name"))
							{
								close(pipefd_1[0]);
								close(pipefd_1[1]);
								close(pipefd_2[0]);
								close(pipefd_2[1]);
								int flg;
								printf("%s\n",arg[1]);
								flg = 1;
								if(arg[2]!=NULL || strlen(arg[1]) > 21) flg = -1;
								else if(strcmp(arg[1],cli_id[id].name) && name_time > 2) flg = -2;
								if(flg!=-1) for(int i = 0;i < 30;i++)
								{
									if(cli_id[i].live_flg && cli_id[i].sockfd != sockfd)
									{
										printf("%d:%s\n",i,cli_id[i].name);	
										if(!strcmp(arg[1],cli_id[i].name))
										{
											flg = 0;
											break;
										}
									}
								}
								if(flg == -1)
								{
									std::string msg = std::string("*** The name should not include space or over 21 characters. ***\n");
									n = send(sockfd,msg.c_str(),strlen(msg.c_str()),0);
									if(n < 0) printf("server: send error\n");
								}
								else if(flg == -2)
								{
									std::string msg("*** Change name for too many times. ***\n");
									n = send(sockfd,msg.c_str(),strlen(msg.c_str()),0);
									if(n < 0) printf("server: send error\n");
								}
								else if(flg)
								{
									strcpy(name,arg[1]);
									memset(line,0,sizeof(line));
									name_time++;
									return 4;
								}
								else
								{
									std::string msg = std::string("*** User '") + std::string(arg[1]) + std::string("' already exists. ***\n");
									n = send(sockfd,msg.c_str(),strlen(msg.c_str()),0);
									if(n < 0) printf("server: send error\n");
								}
							}
							else if(!strcmp(program,"tell"))
							{
								close(pipefd_1[0]);
								close(pipefd_1[1]);
								close(pipefd_2[0]);
								close(pipefd_2[1]);
								int tell;
								
								std::size_t f = preline.find_first_of(" \t");
								std::string::size_type sz;
								std::string l = preline.substr(f); 
								f = preline.find_first_not_of(" \t");
								l = l.substr(f);
								tell = std::stoi(l,&sz);
								tell--;
								l = l.substr(sz);
								f = l.find_first_not_of(" \t");
								l = l.substr(f);
								l = std::string("*** ") + std::string(name) + std::string(" told you ***: ") + l + ("\n");
								if(!cli_id[tell].live_flg)
								{
									std::string msg = std::string("*** Error: user #") + std::to_string(tell+1) + std::string(" does not exist yet. ***\n");
									int n = send(sockfd,msg.c_str(),strlen(msg.c_str()),0);
									if(n < 0) printf("server: send error\n");
								}
								else
								{
									int n = send(cli_id[tell].sockfd,l.c_str(),strlen(l.c_str()),0);
									if(n < 0) printf("server: send error\n");
								}
							}
							else if(!strcmp(program,"yell"))
							{
								close(pipefd_1[0]);
								close(pipefd_1[1]);
								close(pipefd_2[0]);
								close(pipefd_2[1]);
								int f = preline.find_first_of(" \t");
								preline = preline.substr(f);
								f = preline.find_first_not_of(" \t");
								preline = preline.substr(f);
								strcpy(yell,preline.c_str());
								memset(line,0,sizeof(line));
								return 5;
							}
							else if((pid = fork()) < 0)
							{
								close(pipefd_1[0]);
								close(pipefd_1[1]);
								close(pipefd_2[0]);
								close(pipefd_2[1]);
								char* pPath;
								printf("server: fork error\n");
								exit(-1);
							}
							else if(pid == 0)
							{
								dup2(sockfd, STDERR_FILENO);
								//printf("Child: %s\n",program);
								//printf("pipe_mode: %d\n",pipe_mode);
								//printf("redirection: %d\n",redirection);
								//	printf("pipe_index: %d\n",pipe_index);
								if(pipe_flg[pipe_index] || pipefrom_u != -1)
								{ 
									// child read from pipefd_1
									//printf("Child: read pipe %d\n",pipefd_1[0]);
									close(pipefd_1[1]);
									dup2(pipefd_1[0],0);
									close(pipefd_1[0]);
								}
								else
								{
									close(pipefd_1[0]);
									close(pipefd_1[1]);
								}
								if(pipe_mode == -1 && redirection == 0 && pipeobj == -1)
								{
								//	printf("Child: dup2 sockfd\n");
									close(pipefd_2[0]);
									close(pipefd_2[1]);
									dup2(sockfd, STDOUT_FILENO);
								}
								else if(redirection)
								{
								//	printf("Child: redirection\n");
									close(pipefd_2[0]);
									close(pipefd_2[1]);
									int fd;
									fd = creat(re_dir, 0644);
									dup2(fd, STDOUT_FILENO);
									close(fd);
								}
								else
								{
									// child write to pipefd_2
									//printf("Child: write to pipe %d\n",pipefd_2[1]);
									close(pipefd_2[0]);
									dup2(pipefd_2[1],1);
									close(pipefd_2[1]);
								}
								if(execvp(program, arg) == -1)
								{
									if(redirection == 0 && pipe_mode == -1 && pipeobj == 1) close(STDOUT_FILENO);
									if(errno == ENOENT)
									{
										char msg1[] = {"Unknown command: ["};
										char msg2[] = {"].\n"};
										std::string msg = std::string(msg1) + std::string(program) + std::string(msg2);
										l = send(sockfd, msg.c_str(), strlen(msg.c_str()), 0);
										if(l < 0) printf("sever: send error\n");
									}
									else
									{
										char* msg = strerror(errno);
										l = send(sockfd, msg, strlen(msg), 0);
										if(l < 0) printf("sever: send error\n");
									}	
									exit(1);
								}
							}
							else
							{
								//printf("server: parent\n");
								//printf("pipe_flg: %d\n",pipe_flg[pipe_index]);
								if(pipe_flg[pipe_index] || pipefrom_u != -1)
								{
									// parent write to pipefd_1
									//printf("server: write pipe %d\n",pipefd_1[1]);
									close(pipefd_1[0]);
									if(pipe_flg[pipe_index] == 2)
									{
									//		printf("%s",pipebuffer[pipe_index].c_str());
										write(pipefd_1[1], pipebuffer[pipe_index].c_str(), strlen(pipebuffer[pipe_index].c_str()));
									}
									else if(pipe_flg[pipe_index] == 1)
									{
									//		printf("%s",pipe0buffer.c_str());
										write(pipefd_1[1], pipe0buffer.c_str(), strlen(pipe0buffer.c_str()));
										pipe0buffer.clear();
									}
									else if(pipe_flg[pipe_index])
									{
										std::string buffer = pipebuffer[pipe_index] + pipe0buffer;
									//		printf("%s",buffer.c_str());
										write(pipefd_1[1], buffer.c_str(), strlen(buffer.c_str()));
											pipe0buffer.clear();
									}
									else
									{
										write(pipefd_1[1], pipefrom[pipefrom_u].c_str(),strlen(pipefrom[pipefrom_u].c_str()));
										pipefrom[pipefrom_u].clear();
										std::string msg = std::string("*** ") + std::string(cli_id[id].name) + std::string(" (#") + std::to_string(id+1);
										std::string msg2 = std::string(") just received from ") + std::string(cli_id[cli_id[id].pipefrom_u].name) + std::string(" (#");
										std::string msg3 = std::to_string(cli_id[id].pipefrom_u+1) + std::string(") by '") + cli_id[id].preline + std::string("' ***\n");
										std::string msg4 = msg + msg2 + msg3;
										//const char* m = msg4.c_str();
										//broadcast(cli_id,msg4);
										printf("%s",msg4.c_str());
										for(int i = 0;i < 30;i++)
										{
											if(cli_id[i].live_flg)
											{
												int n = send(cli_id[i].sockfd,msg4.c_str(),strlen(msg4.c_str()), 0);
												if(n < 0) printf("server: send error\n");
											}
										}
										printf("server: broadcast\n");
										pipefrom_u = -1;
									}
									close(pipefd_1[1]);
								}
								else
								{
									close(pipefd_1[1]);
									close(pipefd_1[0]);
								}
								if(pipe_mode == -1 && pipeobj == -1)
								{
									close(pipefd_2[0]);
									close(pipefd_2[1]);
								}
								else
								{
									char readbuffer[65537];
									// parent read from pipefd_2
									//printf("server: read pipe %d\n",pipefd_2[0]);
									close(pipefd_2[1]);
									l = read(pipefd_2[0], readbuffer, sizeof(readbuffer));
									readbuffer[l] = '\0';
									//		printf("%s",readbuffer);
									if(pipe_mode == 0) pipe0buffer = std::string(readbuffer);
									else if(pipe_mode == 1) pipebuffer[(pipe_index+pipe_number)%1001] = pipebuffer[(pipe_index+pipe_number)%1001] + std::string(readbuffer);
									else cli_id[pipeobj].pipefrom[id] = std::string(readbuffer);
									close(pipefd_2[0]);
								}

								int status;								
								wait(&status);
								printf("server: receive waiting status\n");
								pipe_mode = -1;
								if(!status) // check if child has any error
								{
								//	printf("server: child exit without error\n");
									pipe_flg[pipe_index] = 0;
											pipebuffer[pipe_index].clear();
									pipe_index++;
									if(pipe_index > 1000) pipe_index = 0;
										redirection = 0;
								}
								else
								{
									printf("server: child exit with error\ni = %d\n",i);
									if(cmdc == 0)
									{
										pipe_flg[pipe_index] = 0;
											pipebuffer[pipe_index].clear();
										pipe_index++;
										if(pipe_index > 1000) pipe_index = 0;
									}
										redirection = 0;
										error_in_line = 1;
								}
							}// end of program execute
					}// end of if error_in_line and program check
				}// end of if / check
			}// end of if recv check
			memset(line, 0, sizeof(line));

			redirection = 0;

			pipe_mode = -1;
			pipe_number = 0;
			env_reset();
			if(pipeobj != -1)
			{
				printf(">\n");
				if(!error_in_line)
				{
					error_in_line = 0;
					return 11;
				}
			}
			error_in_line = 0;
			line_promt(sockfd);
			printf("server: promt\n");
			return 1;
		//}
	}

};

int broadcast(cli *cli_id,std::string &msg)
{
	printf("%s",msg.c_str());
	for(int i = 0;i < 30;i++)
	{
		if(cli_id[i].live_flg)
		{
			int n = send(cli_id[i].sockfd,msg.c_str(),strlen(msg.c_str()), 0);
			if(n < 0) printf("server: send error\n");
		}
	}
	printf("server: broadcast\n");
}

void who(cli *cli_id,int id,int fd)
{
	int n;
	std::string msg("<ID>\t<nickname>\t<IP/port>\t<indicate me>\n");
	n = send(fd, msg.c_str(), strlen(msg.c_str()), 0);
	if(n < 0) printf("server: send error\n");
	for(int i = 0;i < 30; i++)
	{
		if(cli_id[i].live_flg)
		{
			std::string cli_str;
			cli_str = std::to_string(i+1) + "\t" + cli_id[i].name + "\t" + cli_id[i].addr + "/" + std::to_string(cli_id[i].port);
			if(id == i)
			{
				cli_str = cli_str + "\t<-me";
			}
			cli_str = cli_str + "\n";
			n = send(fd, cli_str.c_str(), strlen(cli_str.c_str()), 0);
			if(n < 0) printf("server: send error\n");
		}
	}
	return;
}

int passivesock(char *service, char *protocol,int qlen)
{
	struct servent *pse;
	struct protoent *ppe;
	struct sockaddr_in sin;
	int s, type;

	bzero((char*)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(7017);

	/*if(pse = getservbyname(service, protocol))
		sin.sin_port = htons(ntohs((u_short)pse->s_port) + portbase);
	else if((sin.sin_port = htons((u_short)atoi(service))) == 0)
		errexit("can't get \"%s\" service entry\n", service);*/

	if((ppe = getprotobyname(protocol)) == 0)
	{
		printf("can't get \"%s\" protocol entry\n", protocol);
		exit(0);
	}

	if(strcmp(protocol, "udp") == 0)
		type = SOCK_DGRAM;
	else
		type = SOCK_STREAM;

	s = socket(PF_INET, type, ppe->p_proto);
	if(s < 0)
	{
		printf("can't create socket: %s\n",strerror(errno));
		exit(0);
	}
	if(bind(s,(struct sockaddr*)&sin,sizeof(sin)) < 0)
	{
		printf("can't bind to %s port: %s\n", service,strerror(errno));
		exit(0);
	}
	if(type == SOCK_STREAM && listen(s,qlen) < 0)
	{
		printf("can't listen to %s port: %s\n", service,strerror(errno));
		exit(0);
	}
	
	return s;
}

int passiveTCP(char *service, int qlen)
{
	return passivesock(service, "tcp", qlen);
}



int main(int argc, char *argv[])
{
	printf("server: start\n");
	char *service = "echo";
	struct sockaddr_in fsin;
	int msock;
	fd_set rfds;
	fd_set afds;
	unsigned int alen;
	int fd, nfds;
	cli cli_id[30];
	std::map<int, int> fd_map;

	switch(argc)
	{
		case 1:
			break;
		case 2:
			service = argv[1];
			break;
		default:
			printf("usage:TCPmechod [port]\n");
			exit(0);
	}

	msock = passiveTCP(service, QLEN);
	printf("server: passive\n");

	nfds = getdtablesize();
	FD_ZERO(&afds);
	FD_SET(msock, &afds);


	init();
	printf("server: set dir\n");


	while(1)
	{
		memcpy(&rfds, &afds, sizeof(rfds));

		if(select(nfds, &rfds,(fd_set *)0,(fd_set *)0,(struct timeval *)0) < 0)
		{
			printf("select: %s\n",strerror(errno));
			exit;
		}

		if(FD_ISSET(msock, &rfds))
		{
			int ssock;
			alen = sizeof(fsin);
			ssock = accept(msock,(struct sockaddr *)&fsin, &alen);
			if(ssock < 0)
			{
				("accept: %s\n",strerror(errno));
				continue;
			}
			if(!fd_map[ssock])
			{
				int fs = fcntl(ssock, F_GETFL, 0);
				fcntl(ssock, F_SETFL, fs | O_NONBLOCK);
				for(int i = 0;i < 30; i++)
				{
					if(!cli_id[i].live_flg)
					{
						cli_id[i].cli_id = cli_id;
						cli_id[i].id = i;
						cli_id[i].live_flg = 1;
						cli_id[i].set(ssock);
						fd_map[ssock] = i;
						cli_id[i].addr = inet_ntoa(fsin.sin_addr);
						cli_id[i].port = (int) fsin.sin_port;
						/********************
						give addr and port a const value in spec
						*********************/
						//strcpy(cli_id[i].addr,"CGILAB");
						//cli_id[i].port = 511;
						/********************
						give addr and port a const value in spec
						*********************/
						welcome_msg(ssock);
						std::string msg = std::string("*** User '(no name)' entered from ") + cli_id[i].addr + std::string("/") + std::to_string(cli_id[i].port) + std::string(". ***\n");
						//const char *m = msg.c_str();
						broadcast(cli_id,msg);
						line_promt(ssock);
						printf("%d: %s/%d\n",i+1,cli_id[i].addr,cli_id[i].port);
						break; 
					}
				}
			}
			FD_SET(ssock, &afds);
		}
		for(fd = 0;fd < nfds; ++fd)
			if(fd != msock && FD_ISSET(fd, &rfds))
			{
				printf("server: fd#%d\n",fd);
				int r = cli_id[fd_map[fd]].shell();
				int id = fd_map[fd];
				if(r == 0)
				{
					std::string msg = std::string("*** User '") + std::string(cli_id[id].name) + std::string("' left. ***\n");
					//const char *m = msg.c_str();
					broadcast(cli_id,msg);
					cli_id[id].live_flg = 0;
					cli_id[id].reset();
					(void) close(fd);
					fd_map.erase(fd);
					FD_CLR(fd, &afds);
				}
				else if(r == 3)
				{
					who(cli_id,id,fd);
					line_promt(fd);
					printf("server: who\n");
				}
				else if(r == 4)
				{
					std::string msg = std::string("*** User from ") + std::string(cli_id[id].addr) + std::string("/") + std::to_string(cli_id[id].port) + std::string(" is named '") + std::string(cli_id[id].name) + std::string("'. ***\n");
					//const char* m = msg.c_str();
					broadcast(cli_id,msg);
					line_promt(fd);
				}
				else if(r == 5)
				{
					std::string msg = std::string("*** ") + std::string(cli_id[id].name) + std::string(" yelled ***: ") + std::string(cli_id[id].yell) + std::string("\n");
					//const char* m = msg.c_str();
					broadcast(cli_id,msg);
					line_promt(fd);
				}
				else if(r == 11)
				{
					std::string msg = std::string("*** ") + std::string(cli_id[id].name) + std::string(" (#") + std::to_string(id+1) + std::string(") just piped '") + cli_id[id].preline; 
					////printf("%s\n",msg.c_str());
					std:: string msg4 = msg + std::string("' to ") + std::string(cli_id[cli_id[id].pipeobj].name) + std::string(" (#") + std::to_string(cli_id[id].pipeobj+1) + std::string(") ***\n");
					//const char* m = msg3.c_str();
					//printf("%s",(msg+msg4).c_str());
					broadcast(cli_id,msg4);
					cli_id[id].pipeobj = -1;
					line_promt(fd);
				}

			}		
	}

	return 0;
}
