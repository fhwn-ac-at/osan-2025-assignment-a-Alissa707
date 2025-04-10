#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

typedef struct command_line_arguments {
  int i;
  char const *s;
  bool b;
} cli_args;

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
        printf("Usage: %s <number> -s <string> -b\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  } while (optgot != -1);
  return args;
}

int child_labour() {
  printf("[%d] Doing some work for %d...\n", getpid(), getppid());
  srand(getpid());
  sleep(rand() % 5);
  printf("[%d] Job's done!\n", getpid());
  printf("[%d] Bringing coal to %d...\n", getpid(), getppid());
  return getpid() % 256; // exit code max 255
}

int main(int argc, char *argv[]) {
  printf("[%d] Sending children into the mine...\n", getpid());

  for(int i = 0; i < 10; i++) {
    pid_t forked = fork();
    if (forked == 0) {
      exit(child_labour());
    }
  }

  printf("[%d] Enjoying some brandy...\n", getpid());
  printf("[%d] Where the fudge is my coal?\n", getpid());

  int wstatus = 0;
  pid_t waited;

  for(int i = 0; i < 10; i++) {
    waited = wait(&wstatus);
    if (WIFEXITED(wstatus)) {
      printf("[%d] Child %d exited normally with code %d\n", getpid(), waited, WEXITSTATUS(wstatus));
    } else if (WIFSIGNALED(wstatus)) {
      printf("[%d] Child %d terminated by signal %d\n", getpid(), waited, WTERMSIG(wstatus));
    } else {
      printf("[%d] Child %d terminated abnormally\n", getpid(), waited);
    }
  }

  printf("[%d] All children have returned.\n", getpid());
  return 0;
}
