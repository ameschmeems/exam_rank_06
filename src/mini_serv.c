#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct s_client
{
	int id;
	int fd;
	struct s_client *next;
} t_client;

t_client *g_clients = NULL;
int sock_fd, g_id = 0;
fd_set curr_sock, cpy_read, cpy_write;
char msg[42], str[42*4096], tmp[42*4096], buf[42*4096+42];

void fatal()
{
	write(2, "Fatal error\n", strlen("Fatal error\n"));
	close(sock_fd);
	exit(1);
}

int get_id(int fd)
{
	t_client *temp = g_clients;

	while (temp)
	{
		if (temp->fd == fd)
			return (temp->id);
		temp = temp->next;
	}
	return (-1);
}

int get_max_fd()
{
	t_client *temp = g_clients;
	int max = sock_fd;

	while (temp)
	{
		if (temp->fd > max)
			max = temp->fd;
		temp = temp->next;
	}
	return (max);
}

void send_all(int fd, char *s)
{
	t_client *temp = g_clients;

	while (temp)
	{
		if (temp->fd != fd && FD_ISSET(temp->fd, &cpy_write))
		{
			if (send(temp->fd, s, strlen(s), 0) < 0)
				fatal();
		}
		temp = temp->next;
	}
}

int add_client(int fd)
{
	t_client *temp = g_clients;
	t_client *new;

	if (!(new = calloc(1, sizeof(t_client))))
		fatal();
	new->fd = fd;
	new->id = g_id++;
	new->next = NULL;
	if (!g_clients)
		g_clients = new;
	else
	{
		while (temp && temp->next)
			temp = temp->next;
		temp->next = new;
	}
	return (new->id);
}

void accept_connection()
{
	struct sockaddr_in clientaddr;
	socklen_t len = sizeof(clientaddr);
	int client_fd;

	if ((client_fd = accept(sock_fd, (struct sockaddr *)&clientaddr, &len)) < 0)
		fatal();
	bzero(&msg, strlen(msg));
	sprintf(msg, "server: client %d just arrived\n", add_client(client_fd));
	send_all(client_fd, msg);
	FD_SET(client_fd, &curr_sock);
}

int rm_client(int fd)
{
	t_client *temp = g_clients;
	t_client *del;
	int id = get_id(fd);

	if (temp->fd == fd)
	{
		g_clients = temp->next;
		free(temp);
	}
	else
	{
		while (temp && temp->next && temp->next->fd != fd)
			temp = temp->next;
		del = temp->next;
		temp->next = temp->next->next;
		free(del);
	}
	return (id);
}

void extract_message(int fd)
{
	int i = 0;
	int j = 0;

	while (str[i])
	{
		tmp[j] = str[i];
		j++;
		if (str[i] == '\n')
		{
			sprintf(buf, "client %d: %s", get_id(fd), tmp);
			send_all(fd, buf);
			j = 0;
			bzero(&tmp, strlen(tmp));
			bzero(&buf, strlen(buf));
		}
		i++;
	}
	if (j)
	{
		sprintf(buf, "client %d: %s", get_id(fd), tmp);
		send_all(fd, buf);
		bzero(&tmp, strlen(tmp));
		bzero(&buf, strlen(buf));
	}
	bzero(&str, strlen(str));
}

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
		exit(1);
	}
	struct sockaddr_in servaddr;
	uint16_t port = atoi(argv[1]);

	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port);

	// socket create and verification 
	if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		fatal();
	// Binding newly created socket to given IP and verification 
	if ((bind(sock_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		fatal();
	if (listen(sock_fd, 0) != 0)
		fatal();

	bzero(&msg, sizeof(msg));
	bzero(&buf, sizeof(buf));
	bzero(&str, sizeof(str));
	bzero(&tmp, sizeof(tmp));

	FD_ZERO(&curr_sock);
	FD_SET(sock_fd, &curr_sock);

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	while (1)
	{
		cpy_read = cpy_write = curr_sock;
		if (select(get_max_fd() + 1, &cpy_read, &cpy_write, NULL, &tv) < 0)
			fatal();
		for (int fd = 0; fd <= get_max_fd(); fd++)
		{
			if (FD_ISSET(fd, &cpy_read))
			{
				if (fd == sock_fd)
				{
					accept_connection();
					break ;
				}
				else
				{
					int ret_recv = 1000;
					while (ret_recv == 1000 || str[strlen(str) - 1] != '\n')
					{
						ret_recv = recv(fd, str + strlen(str), 1000, 0);
						if (ret_recv <= 0)
							break ;
					}
					if (ret_recv < 0)
						fatal();
					if (ret_recv == 0)
					{
						if (strlen(str))
							extract_message(fd);
						bzero(&msg, strlen(msg));
						sprintf(msg, "server: client %d just left\n", rm_client(fd));
						send_all(fd, msg);
						close(fd);
						FD_CLR(fd, &curr_sock);
						break ;
					}
					else
						extract_message(fd);
				}
			}
		}
	}
}