#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>

typedef struct command_line_arguments {
  int i;
  char const *s;
  bool b;
} cli_args;

cli_args parse_command_line(int const argc, char * argv []){
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

int main(int argc, char * argv[]) {
  cli_args const args = parse_command_line(argc, argv);
  printf("i: %d, s: %s, b: %d\n", args.i, args.s,args.b);

  return 0;
}
