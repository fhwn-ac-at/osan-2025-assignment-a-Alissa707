#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>

#define EXIT_FAILURE 1
#define CHILD_COUNT 10
#define TASKS_PER_CHILD 2

typedef struct command_line_arguments {
  int i;
  char const *s;
  bool b;
} cli_args;

struct work_message {
  int work;
};

cli_args parse_command_line(int const argc, char *argv[]) {
  cli_args args = {0, NULL, false};

  int optgot = -1;
  do {
    optgot = getopt(argc, argv, "i:s:b");
    switch (optgot) {
      case 'i':
        args.i = atoi(optarg);
        break;
      case 's':
        args.s = optarg;
        break;
      case 'b':
        args.b = true;
        break;
      case '?':
        printf("Usage: %s -i <number> -s <string> -b\n", argv[0]);
        exit(EXIT_FAILURE);
    }
  } while (optgot != -1);

  return args;
}

int child_labour(mqd_t command_queue) {
  struct work_message instructions;

  for (int task = 0; task < TASKS_PER_CHILD; task++) {
    printf("[%d] Waiting for task %d...\n", getpid(), task + 1);

    int received = mq_receive(command_queue, (char*)&instructions, sizeof(struct work_message), NULL);

    if (received == -1) {
      fprintf(stderr, "[%d] Failed to receive instructions.\n", getpid());
      return EXIT_FAILURE;
    }

    printf("[%d] Got task %d: work value %d\n", getpid(), task + 1, instructions.work);

    srand(getpid() + task);
    sleep(rand() % 5);
    printf("[%d] Task %d done.\n", getpid(), task + 1);
  }

  mq_close(command_queue);
  return getpid() % 256;
}

int main(int argc, char *argv[]) {
  cli_args const args = parse_command_line(argc, argv);

  struct mq_attr queue_options = {
    .mq_maxmsg = CHILD_COUNT * TASKS_PER_CHILD,
    .mq_msgsize = sizeof(struct work_message),
  };

  mqd_t command_queue = mq_open("/mq_r44565", O_CREAT | O_RDWR, S_IRWXU, &queue_options);
  printf("[%d] mq_open returned %d\n", getpid(), command_queue);

  if (command_queue == -1) {
    fprintf(stderr, "[%d] mq_open failed\n", getpid());
    return EXIT_FAILURE;
  }

  printf("[%d] Sending children into the mine...\n", getpid());

  for (int i = 0; i < CHILD_COUNT; i++) {
    pid_t forked = fork();
    if (forked == 0) {
      return child_labour(command_queue);
    }

    for (int t = 0; t < TASKS_PER_CHILD; t++) {
      struct work_message instructions = {.work = (i + 1) * 10 + t};
      int sent = mq_send(command_queue, (char*)&instructions, sizeof(struct work_message), 0);
      if (sent == -1) {
        fprintf(stderr, "[%d] Failed to send instructions.\n", getpid());
        return EXIT_FAILURE;
      }
      printf("[%d] Sent task %d to child %d\n", getpid(), t + 1, forked);
    }
  }

  printf("[%d] Enjoying some brandy...\n", getpid());
  printf("[%d] Where the fudge is my coal?\n", getpid());

  int wstatus = 0;
  pid_t waited;

  for (int i = 0; i < CHILD_COUNT; i++) {
    waited = wait(&wstatus);
    if (WIFEXITED(wstatus)) {
      printf("[%d] Child %d exited normally with code %d\n", getpid(), waited, WEXITSTATUS(wstatus));
    } else if (WIFSIGNALED(wstatus)) {
      printf("[%d] Child %d terminated by signal %d\n", getpid(), waited, WTERMSIG(wstatus));
    } else {
      printf("[%d] Child %d terminated abnormally\n", getpid(), waited);
    }
  }

  mq_close(command_queue);
  mq_unlink("/mq_r44565");

  printf("[%d] All children have returned.\n", getpid());
  return 0;
}
