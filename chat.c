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

typedef enum {
  MSG_CONNECT,
  MSG_SEND_MESSAGE,
} message_kind_t;

typedef struct {
  pid_t pid;
  message_kind_t kind;
  char nickname[32];
  char message[128];
} message_t;

typedef struct {
  pid_t pid;
  int fd;
} feedback_entry_t;

typedef struct {
  int count;
  feedback_entry_t entries[100];
} feedback_t;

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

void add_feedback(feedback_t *feedback, message_t *msg) {
  char feedback_path[256];
  sprintf(feedback_path, CLIENT_INPUT, msg->pid);
  feedback->entries[feedback->count].pid = msg->pid;
  feedback->entries[feedback->count].fd = create_open_fifo(feedback_path, O_WRONLY);
  feedback->count++;
}

void send_feedback(feedback_t *feedback, message_t *msg) {
  for (int i = 0; i < feedback->count; i++) {
    if (feedback->entries[i].pid == msg->pid) {
      continue;
    }
    write(feedback->entries[i].fd, msg, sizeof(*msg)); 
  }
}

void server_main() {
  if(mkdir("/tmp/chat", 0777) != 0 && errno != EEXIST) {
    perror("mkdir");
    exit(1);
  }
  message_t msg;
  feedback_t feedback;
  feedback.count = 0;
  int server_input = create_open_fifo(SERVER_INPUT, O_RDONLY);
  while (1) {
    if (read(server_input, &msg, sizeof(msg)) != sizeof(msg)) {
      exit(1);
    }
    if (msg.kind == MSG_CONNECT) {
      add_feedback(&feedback, &msg);
    } else if (msg.kind == MSG_SEND_MESSAGE) {
      print_message(&msg);
      send_feedback(&feedback, &msg);
    }
  }
}

void client_main(char *nickname) {
  message_t msg;
  msg.pid = getpid();
  if (nickname == NULL) {
    sprintf(msg.nickname, "%d", msg.pid);
  } else {
    strcpy(msg.nickname, nickname);
  }
  int server_input = create_open_fifo(SERVER_INPUT, O_WRONLY);
  msg.kind = MSG_CONNECT;
  write(server_input, &msg, sizeof(msg));
  msg.kind = MSG_SEND_MESSAGE;
  char feedback_path[256];
  sprintf(feedback_path, CLIENT_INPUT, getpid());
  int feedback = create_open_fifo(feedback_path, O_RDONLY);
  struct pollfd pfds[2];
  pfds[0].fd = STDIN_FILENO;
  pfds[0].events = POLLIN;
  pfds[1].fd = feedback;
  pfds[1].events = POLLIN;
  while (1) {
    int ready = poll(pfds, 2, -1);
    if (ready == -1) {
      perror("poll");
      exit(1);
    }
    if (pfds[0].revents & POLLIN) {
      if (fgets(msg.message, sizeof(msg.message), stdin) == NULL) {
        perror("fgets");
        exit(1);
      }
      write(server_input, &msg, sizeof(msg)); 
    }
    if (pfds[1].revents & POLLIN) {
      message_t input_msg;
      if (read(feedback, &input_msg, sizeof(input_msg)) != sizeof(input_msg)) {
        printf("I get error here!\n");
        exit(1);
      }
      print_message(&input_msg);
    }
  }
}

int main(int argc, char **argv) {
  char *name = basename(argv[0]);
  if (strcmp(name, "server") == 0) {
    server_main();
  } else if (strcmp(name, "client") == 0) {
    client_main(argc > 1 ? argv[1] : NULL);
  } else {
    fprintf(stderr, "Argv[0] must be server or client\n");
    return 1;
  }
  return 0;
}
