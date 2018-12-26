#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "lserver.h"

static int lserver_setvalues(lserver_t *server, size_t ports_n,
                             size_t client_buffer_size)
{
  memset(server, 0, sizeof(*server));
  server->client_buffer_size = client_buffer_size;
  server->epoll = epoll_create1(EPOLL_CLOEXEC);
  if (server->epoll == -1)
    return (-1);
  if (gtab_create(&server->clients, 8) == -1
      || gtab_create(&server->listeners, ports_n) == -1)
    return (-1);
  return (0);
}

int lserver_create(lserver_t *server, const uint16_t *ports, size_t size,
                   size_t client_buffer_size)
{
  lclient_t *listener;
  struct epoll_event evt;

  if (lserver_setvalues(server, size, client_buffer_size) == -1)
    return (-1);
  for (size_t i = 0; i < size; ++i) {
    listener = malloc(sizeof(*listener));
    if (listener == NULL)
      return (-1);
    if (lclient_create(listener, 0, NULL, 0) == -1)
      return (-1);
    evt.data.ptr = listener;
    evt.events = EPOLLIN;
    if (lsocket_server(&listener->socket, ports[i], 64) == -1)
      return (-1);
    if (gtab_sappend(&server->listeners, listener) == -1
        || epoll_ctl(server->epoll, EPOLL_CTL_ADD, listener->socket.fd, &evt)
                   == -1)
      return (-1);
  }
  return (0);
}

void _lserver_lclient_destructor(void *ptr)
{
  lclient_t *client = ptr;

  lsocket_shutdown(&client->socket);
  lclient_destroy(client);
  free(ptr);
}

void lserver_destroy(lserver_t *server)
{
  gtab_destroy(&server->listeners, _lserver_lclient_destructor);
  gtab_destroy(&server->clients, _lserver_lclient_destructor);
  free(server->events);
  close(server->epoll);
}
