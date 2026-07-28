#include "parser.h"
#include "viewer.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Format : %s <vcd_file>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  struct vcd_data *db = parse_vcd(argv[1]);

  viewer(db);
  free_data(db);
  return 0;
}
