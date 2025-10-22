#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <poll.h>
#include <time.h>

#define GREEN "\x1b[32m"
#define DEFAULT "\x1b[0m"
#define SERVER_INPUT "/tmp/chat/server_input"
#define CLIENT_INPUT "/tmp/chat/client.%d"
#define CLIENT_OUTPUT "/tmp/chat/client.%d.out"
#define MAX_CLIENTS 100

typedef struct {
  pid_t pid;
  char nickname[32];
} handshake_t;

typedef struct {
  pid_t pid;
  char nickname[32];
  char message[128];
} message_t;

typedef struct {
  pid_t pid;
  int out_fd;
  int in_fd;
} client_entry_t;

typedef struct {
  int count;
  client_entry_t entries[MAX_CLIENTS];
} client_list_t;

void print_message(message_t *msg) {
  printf("%s%s:%s %s", GREEN, msg->nickname, DEFAULT, msg->message);
}

int create_open_fifo(char *path, int open_flags) {
  if (mkfifo(path, 0777) != 0 && errno != EEXIST) {
    perror("mkfifo");
    exit(1);
  }
  int fd = open(path, open_flags);
  if (fd == -1) {
    perror("open");
    exit(1);
  }
  return fd;
}

void add_client(client_list_t *clients, struct pollfd *pfds, handshake_t *hs, int *nfds) {
  if (clients->count >= MAX_CLIENTS) {
    fprintf(stderr, "Max clients reached\n");
    return;
  }
  char feedback_path[256];
  char output_path[256];
  sprintf(feedback_path, CLIENT_INPUT, hs->pid);
  sprintf(output_path, CLIENT_OUTPUT, hs->pid);
  client_entry_t *entry = &clients->entries[clients->count++];
  entry->pid = hs->pid;
  entry->in_fd = create_open_fifo(feedback_path, O_WRONLY);
  entry->out_fd = create_open_fifo(output_path, O_RDONLY);
  pfds[*nfds].fd = entry->out_fd;
  pfds[*nfds].events = POLLIN;
  (*nfds)++;
  printf("New client connected: %s (PID %d)\n", hs->nickname, hs->pid);
}

void remove_client(client_list_t *clients, struct pollfd *pfds, int *nfds, int idx) {
  close(clients->entries[idx].in_fd);
  close(clients->entries[idx].out_fd);
  printf("Client %d disconnected\n", clients->entries[idx].pid);
  clients->entries[idx] = clients->entries[--clients->count];
  *nfds = 1;
  for (int i = 0; i < clients->count; i++) {
    pfds[*nfds].fd = clients->entries[i].out_fd;
    pfds[*nfds].events = POLLIN;
    (*nfds)++;
  }
}

void send_feedback(client_list_t *clients, message_t *msg) {
  for (int i = 0; i < clients->count; i++) {
    if (clients->entries[i].pid == msg->pid) continue;
    write(clients->entries[i].in_fd, msg, sizeof(*msg));
  }
}

void server_main() {
  if (mkdir("/tmp/chat", 0777) != 0 && errno != EEXIST) {
    perror("mkdir");
    exit(1);
  }
  client_list_t clients = { .count = 0 };
  int server_input = create_open_fifo(SERVER_INPUT, O_RDONLY);
  struct pollfd pfds[MAX_CLIENTS + 1];
  pfds[0].fd = server_input;
  pfds[0].events = POLLIN;
  int nfds = 1;
  while (1) {
    int ready = poll(pfds, nfds, -1);
    if (ready == -1) {
      perror("poll");
      exit(1);
    }
    if (pfds[0].revents & POLLIN) {
      handshake_t hs;
      ssize_t n = read(server_input, &hs, sizeof(hs));
      if (n == sizeof(hs)) add_client(&clients, pfds, &hs, &nfds);
    }
    for (int i = 0; i < clients.count; i++) {
      int idx = i + 1;
      if (pfds[idx].revents & POLLIN) {
        message_t msg;
        ssize_t n = read(clients.entries[i].out_fd, &msg, sizeof(msg));
        if (n == sizeof(msg)) {
          print_message(&msg);
          send_feedback(&clients, &msg);
        } else if (n == 0) {
          remove_client(&clients, pfds, &nfds, i);
          break;
        }
      }
    }
  }
}

void client_main(char *nickname) {
  handshake_t hs;
  hs.pid = getpid();
  if (nickname) strcpy(hs.nickname, nickname);
  else sprintf(hs.nickname, "%d", hs.pid);
  char feedback_path[256];
  char output_path[256];
  sprintf(feedback_path, CLIENT_INPUT, hs.pid);
  sprintf(output_path, CLIENT_OUTPUT, hs.pid);
  int server_input = create_open_fifo(SERVER_INPUT, O_WRONLY);
  int feedback = create_open_fifo(feedback_path, O_RDONLY);
  int output = create_open_fifo(output_path, O_WRONLY);
  write(server_input, &hs, sizeof(hs));
  struct pollfd pfds[2];
  pfds[0].fd = STDIN_FILENO;
  pfds[0].events = POLLIN;
  pfds[1].fd = feedback;
  pfds[1].events = POLLIN;
  message_t msg;
  msg.pid = hs.pid;
  strcpy(msg.nickname, hs.nickname);
  while (1) {
    int ready = poll(pfds, 2, -1);
    if (ready == -1) {
      perror("poll");
      exit(1);
    }
    if (pfds[0].revents & POLLIN) {
      if (fgets(msg.message, sizeof(msg.message), stdin) == NULL) break;
      write(output, &msg, sizeof(msg));
    }
    if (pfds[1].revents & POLLIN) {
      message_t incoming;
      ssize_t n = read(feedback, &incoming, sizeof(incoming));
      if (n == sizeof(incoming)) print_message(&incoming);
      else break;
    }
  }
  close(output);
  close(feedback);
}

int main(int argc, char **argv) {
  char *name = basename(argv[0]);
  if (strcmp(name, "server") == 0) server_main();
  else if (strcmp(name, "client") == 0) client_main(argc > 1 ? argv[1] : NULL);
  else {
    fprintf(stderr, "Argv[0] must be server or client\n");
    return 1;
  }
  return 0;
}
