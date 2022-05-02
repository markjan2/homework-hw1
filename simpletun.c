
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_tun.h>

#define PASSWORD_FILE "/home/jeter.jen/server/passwords"
#define TUN_PATH	  "/dev/net/tun"
#define S_OK          true
#define S_FAIL        false
#define DEFAULT_PORT  9999

bool authentication(char *cInputPassword)
{
	int iFd = open(PASSWORD_FILE, O_RDWR);
	struct stat file_status;
	
	char cCorrectPassword[32];
	memset(cCorrectPassword, 0, 32);
	
	if(fstat(iFd, &file_status) < 0)
	{
		printf("password file is empty\n");
		return S_FAIL;
	}

	if(file_status.st_size != sizeof(cInputPassword))
	{
		printf("connect failed, wrong password.\n");
		return S_FAIL;
	}
	
	if((read(iFd, cCorrectPassword, file_status.st_size)) == -1)
	{
		printf("read password file fail\n");
		return S_FAIL;
	}

	cCorrectPassword[file_status.st_size-1] = '\0';
	cInputPassword[sizeof(cInputPassword)-1] = '\0';
	
	if(strcmp(cCorrectPassword, cInputPassword) != 0)
	{
		printf("connect failed, wrong password.\n");
		return S_FAIL;
	}

	return S_OK;
}

int get_device_tun(char *cInterface)
{
	struct ifreq ifr;
    int iFd, iError;
	
	if((iFd = open(TUN_PATH, O_RDWR)) < 0)
	{
		printf("open tun fail\n");
		return S_FAIL;
	}
	
	memset(&ifr, 0, sizeof(ifr));
	
	ifr.ifr_flags = IFF_TUN;
	
	if(*cInterface)
	{
		strncpy(ifr.ifr_name, cInterface, IFNAMSIZ);
	}

	if((iError = ioctl(iFd, TUNSETIFF, (void *)&ifr)) < 0 ){
       close(iFd);
       return iError;
    }
	
	strcpy(cInterface, ifr.ifr_name);

	return iFd;
}

bool connect_server(char *cInterface, char *cServerIP)
{
	int iSockfd = 0;
	char cBuf[32];
	
	struct sockaddr_in sClientInfo;
	
	// socket
	iSockfd = socket(AF_INET , SOCK_STREAM , 0);
	if (iSockfd == -1)
	{
		printf("create socket fail\n");
		return S_FAIL;
    }
	
	sClientInfo.sin_family = AF_INET;
	sClientInfo.sin_port = htons(DEFAULT_PORT);
	sClientInfo.sin_addr.s_addr = inet_addr(cServerIP);
	
	if(connect(iSockfd, (struct sockaddr *)&sClientInfo, sizeof(sClientInfo)) < 0)
	{
		printf("connect to server fail\n");
		return S_FAIL;
	}
	
	while(true)
	{
		printf("Enter some words you want to send: (enter 'q' to close the socket)\n");
		
		fgets(cBuf,32,stdin);
		
		if (!strcmp(cBuf, "q")) // quit the program
		{
			break;
		}
		
		if (send(iSockfd, cBuf, strlen(cBuf), 0) < 0) // send something to the server
		{
			printf("can't send message to server\n");
			close(iSockfd);
			return S_FAIL;
		}
	}
	
	close(iSockfd);
	
	return S_OK;
}

bool establish_connection(char *cInterface, bool bEnableTun)
{
	int iSockfd = 0, iNread = 0;
	fd_set fdset, master;
	int iClientInfoLen = 0, iClientFd = 0;
	int iMaxfd = 0, iSelectRet = 0;
	int iFd = 0;
	int iTunfd = 0;
	
	char buf[32];
	
	struct sockaddr_in sServerInfo, sClientInfo;
	
	if ((iSockfd = socket(PF_INET,SOCK_STREAM,IPPROTO_TCP)) < 0) //socket 類型 
    {
        printf("Failed to create socket\n");
    }
	
	memset(&sServerInfo, 0, sizeof(sServerInfo));
	
	sServerInfo.sin_family = AF_INET;
	sServerInfo.sin_port = htons(DEFAULT_PORT);
	sServerInfo.sin_addr.s_addr = INADDR_ANY;
	
	if(bind(iSockfd, (struct sockaddr *)&sServerInfo, sizeof(sServerInfo)) == -1)
	{
		printf("bind fail\n");
		return S_FAIL;
	}
	
	if(listen(iSockfd, 10) == -1)
	{
		printf("listen fail\n");
		return S_FAIL;
	}

	struct timeval timeout;
	timeout.tv_sec = 30;
	timeout.tv_usec = 0;
	
	char cServerMsg[32] = "connected, tunnel established.";

	FD_ZERO(&fdset);
	FD_ZERO(&master);
	FD_SET(iSockfd, &master);
	
	if(bEnableTun)
	{
		if((iTunfd = get_device_tun(cInterface)) < 0)
		{
			printf("connecting to tun fail\n");
			return S_FAIL;
		}
		FD_SET(iTunfd, &master);
		iMaxfd = iTunfd > iSockfd ? iTunfd : iSockfd;
	}
	else
	{
		iMaxfd = iSockfd;
	}
	
	while(true)
	{
		fdset = master; 
		iSelectRet = select(iMaxfd+1, &fdset, NULL, NULL, &timeout);
		
		if(iSelectRet < 0)
		{
			printf("...\n");
			continue;
		}
		else if(iSelectRet == 0)
		{
			printf("select timeout\n");
			return S_FAIL;
		}
		else
		{
			for(iFd = 0;iFd <= iMaxfd;iFd++)
			{
				if(FD_ISSET(iFd, &fdset))
				{
					if(iFd == iSockfd)
					{
						iClientInfoLen = sizeof(sClientInfo);
						if((iClientFd = accept(iSockfd, (struct sockaddr *)&sClientInfo, &iClientInfoLen)) < 0)
						{
							printf("can't accept client connection\n");
							return S_FAIL;
						}
						
						FD_SET(iClientFd, &master);
						
						if (iClientFd > iMaxfd)
						{
							iMaxfd = iClientFd;
						}
						
						printf("connected, tunnel established.\n");
					}
					else
					{
						printf("...\n");
						close(iFd);
						FD_CLR(iFd, &master);
					}
				}
				
				if(FD_ISSET(iTunfd, &fdset))
				{
					if(iFd == iTunfd)
					{
						printf("connected, tunnel established.\n");
					}
				}
			}
		}
	}

	return S_OK;
}

int main(int argc, char *argv[])
{
	char cInterface[32];
	memset(cInterface, 0, 32);

	char cServerIP[32];
	memset(cServerIP, 0, 32);
	
	char cPassword[32];
	memset(cPassword, 0, 32);
	
	bool bClientFlag = false;
	bool bEnableTun = false;
	
	int opt;
	
	while((opt = getopt(argc, argv, "i:c:w:h")) != -1) 
	{
        switch(opt)
		{
			case 'i':
			    strncpy(cInterface, optarg, sizeof(cInterface));
				bEnableTun = true;
				break;
			case 'c':
				strncpy(cServerIP, optarg, sizeof(cServerIP));
				bClientFlag = true;
				break;
			case 'w':
				strncpy(cPassword, optarg, sizeof(cPassword));
				bClientFlag = true;
				break;
			case 'h':
			default: 
				printf("simpletun -i <ifacename> [-c <serverIP>] [-w <password>]\n");
				break;
		}
	}	
	
	argc -= optind;
	argv += optind;
	
	if(argc > 0) 
    {
        printf("simpletun -i <ifacename> [-c <serverIP>] [-w <password>]\n");
		return S_FAIL;
    }
    
    if(*cInterface == '\0')
	{
	    printf("Interface is empty\n");
	    return S_FAIL;
	}
	else if(bClientFlag && *cServerIP == '\0')
	{
	    printf("server IP incorrect\n");
	    return S_FAIL;
	}
	
	if(bClientFlag)
	{
		if(!authentication(cPassword))
		{
			return S_FAIL;
		}
		
		if(!connect_server(cInterface, cServerIP))
		{
			return S_FAIL;
		}
		
		return S_OK;
	}

	if(!establish_connection(cInterface, bEnableTun))
	{
		return S_FAIL;
	}
	
	return S_OK;
}
