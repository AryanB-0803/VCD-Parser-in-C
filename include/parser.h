#ifndef PARSER_H
#define PARSER_H

#define name_width 50

struct time_value_pair {
  char timescale[16]; // because max no of chars + null term
  long time;
  unsigned long val;
};

struct signal_details {
  char id; // this will work only for single char ids
  // designs with >94 signals will MISPARSE
  int width;
  char signal_name[name_width];
  struct time_value_pair *samples;
  int num_samples;
  int cap_samples;
};

struct vcd_data {
  struct signal_details *signals;
  int count_of_lines;
};

struct vcd_data *parse_vcd(const char *filename);

void free_data(struct vcd_data *db);

#endif
