#include "parser.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int num_of_signals = 100;

struct vcd_data *db;

void calc_width(struct signal_details *sig, char *ptr) {
  sig->width = sig->width * 10 + ((int)(*ptr) - (int)'0');
  //-(int)'0' will properly offset the int equivalent of *ptr to give the
  // integer
}

void string_name(struct signal_details *sig, char *trav_ptr) {
  int j = 0;
  while (!isspace(*trav_ptr) && j < name_width - 1) {
    sig->signal_name[j] = *trav_ptr;
    trav_ptr++;
    j++;
  }
  sig->signal_name[j] = '\0';
}

unsigned long val_calc(char *trav_ptr) {
  int j = 0;
  unsigned long return_val = 0;
  char *temp = trav_ptr;
  while (!isspace(*trav_ptr)) {
    j++;
    trav_ptr++;
  }
  while (!isspace(*temp)) {
    return_val += (int)(*temp - '0') * pow(2, j - 1);
    j--;
    temp++;
  }
  return return_val;
}

void free_data(struct vcd_data *db) {
  for (int i = 0; i < db->count_of_lines; i++) {
    free(db->signals[i].samples);
  }
  free(db->signals);
  free(db);
}

struct vcd_data *parse_vcd(const char *filename) {

  db = malloc(sizeof(*db));
  db->signals = malloc(num_of_signals * sizeof(*db->signals));

  FILE *fp = fopen(filename, "r");

  int count_of_lines = 0;
  long current_time = 0;
  char *id_array = malloc(num_of_signals * sizeof(char));

  const int MAX_DEPTH = 32; // stack depth
  const int MAX_WIDTH = 50; // max width of instance name
  char scope_stack[MAX_DEPTH][MAX_WIDTH];
  int stack_top = 0;
  char stack_str[256];
  size_t scope_path_len = 0;
  bool timescale_detect = false;
  int timescale_width = sizeof(db->signals->samples->timescale);
  char timescale_assign[timescale_width];

  if (fp == NULL) {
    fprintf(stderr, "Error opening %s : %s\n", filename, strerror(errno));
    exit(EXIT_FAILURE);
  };

  char *line = NULL;
  size_t size = 0;
  ssize_t nread;

  while ((nread = getline(&line, &size, fp)) != -1) {
    line[nread - 1] = '\0';
    char *type = line;

    if (timescale_detect) {
      while (isspace((unsigned char)*type)) {
        type++;
      }
      type++; // points to timescales first char

      int i = 0;
      while (!isspace((unsigned char)*type) && i < timescale_width - 1) {
        timescale_assign[i] = *type;
        i++;
        type++;
      }
      timescale_assign[i] = '\0';
      timescale_detect = false;
      // deassign after one time cos timescale is only once /vcd file
    }

    if ((*(type + 1) == 't') && (*(type + 2) == 'i') && (*(type + 3) == 'm')) {
      timescale_detect = true;
    }

    else if ((*(type + 1) == 's') && (*(type + 2) == 'c') &&
             (*(type + 3) == 'o')) //$scope
    {
      while (!isspace((unsigned char)*(type))) {
        type++;
      }
      type++; // pointer now points to first char of module or whatever other
              // inst

      while (!isspace((unsigned char)*(type))) {
        type++;
      }
      type++; // pointer now points to first char of scope

      int stack_name_idx = 0;
      while (!isspace((unsigned char)*(type))) {
        scope_stack[stack_top][stack_name_idx] = *type;
        stack_name_idx++;
        type++;
      }
      scope_stack[stack_top][stack_name_idx] = '\0';
      stack_top++; // increment the top of the stack so that next scope takes
                   // that value

      stack_str[0] =
          '\0'; // so that it resets everytime a new append must take place
      for (int i = 0; i < stack_top; i++) {
        strcat(stack_str, scope_stack[i]);
        strcat(stack_str, ".");
      }

      scope_path_len = strlen(stack_str);
    }

    //$upscope
    else if ((*(type + 1) == 'u') && (*(type + 3) == 's') &&
             (*(type + 4) == 'c') && (*(type + 5) == 'o')) {
      stack_str[0] = '\0';
      stack_top--;
      for (int i = 0; i < stack_top; i++) {
        strcat(stack_str, scope_stack[i]);
        strcat(stack_str, ".");
      }
      scope_path_len = strlen(stack_str);
    }

    else if ((*(type + 1) == 'v') && (*(type + 2) == 'a') &&
             (*(type + 3) == 'r')) {
      count_of_lines++;
      if (count_of_lines > num_of_signals) {
        num_of_signals *= 2;
        db->signals =
            realloc(db->signals, num_of_signals * sizeof(*db->signals));
      }

      db->signals[count_of_lines - 1].width = 0;
      db->signals[count_of_lines - 1].id = '\0';
      db->signals[count_of_lines - 1].signal_name[0] = '\0';
      db->signals[count_of_lines - 1].cap_samples = 16;
      db->signals[count_of_lines - 1].num_samples = 0;
      db->signals[count_of_lines - 1].samples =
          malloc(db->signals[count_of_lines - 1].cap_samples *
                 sizeof(struct time_value_pair));
      for (int i = 0; i < timescale_width; i++)
        db->signals->samples->timescale[i] = timescale_assign[i];

      // isspace() check for $ strings
      while (!isspace((unsigned char)*(type))) {
        type++;
      }
      type++; // now the pointer will point at first character of Verilog
              // data type

      // isspace() check for Verilog data types
      while (!isspace((unsigned char)*(type))) {
        type++;
      }

      type++; // because itll stop at when ' ' is seen and wont increment the
              // pointer
      while (*type != ' ') {
        calc_width(&db->signals[count_of_lines - 1], type);
        // count-1 cos it first increments so for 0 it becomes 1 and so on
        type++;
      }

      type++; // point at the id char surpassing the whitespace
      while (!isspace((unsigned char)*(type))) {
        db->signals[count_of_lines - 1].id = *(type++);
      }
      // post inc will move this to whitespace
      type++; // go to pointer of first char of signal_name surpassing
              // whitespace

      string_name(&db->signals[count_of_lines - 1], type);
      stack_str[scope_path_len] = '\0';
      // this is because without this the subsequent var states get added
      // to the previous signal names
      strcat(stack_str, db->signals[count_of_lines - 1].signal_name);
      strncpy(db->signals[count_of_lines - 1].signal_name, stack_str,
              name_width - 1);
      db->signals[count_of_lines - 1].signal_name[name_width - 1] = '\0';

    }

    else if (*(type + 1) == 'e' && *(type + 4) == 'd') {
      free(id_array);
      id_array = malloc(count_of_lines * sizeof(char));
      for (int j = 0; j < count_of_lines; j++) {
        *(id_array + j) =
            db->signals[j].id; // this will create just an id array acc
        // to signal numbers
      }
    }

    else {
      if (*type == '#') {
        current_time = atol(type + 1);
      }

      else if (*type == 'b') {
        int idx = 0;
        char *temp = type;
        while (!isspace((unsigned char)*(temp))) {
          temp++;
        }
        temp++; // to go to the index of the id char

        char target = *(temp);
        for (int i = 0; i < count_of_lines; i++) {
          if (id_array[i] == target) {
            idx = i;
            break;
          }
        }

        if (db->signals[idx].num_samples >= db->signals[idx].cap_samples) {
          db->signals[idx].cap_samples *= 2;
          db->signals[idx].samples = realloc(
              db->signals[idx].samples,
              db->signals[idx].cap_samples * sizeof(struct time_value_pair));
        }

        db->signals[idx].samples[db->signals[idx].num_samples].time =
            current_time;
        db->signals[idx].samples[db->signals[idx].num_samples].val =
            val_calc(type + 1);
        db->signals[idx].num_samples++;
      }

      else if (*type == '1' || *type == '0' || *type == 'x' || *type == 'z') {
        char val_char = *type; // either '1','0','x' or 'z'
        int idx = 0;
        char target = *(type + 1);
        for (int i = 0; i < count_of_lines; i++) {
          if (id_array[i] == target) {
            idx = i;
            break;
          }
        }

        unsigned long val = (val_char == '1') ? 1 : 0;
        if (val_char == 'x' || val_char == 'z')
          val = 0; // let x and z be 0

        if (db->signals[idx].num_samples >= db->signals[idx].cap_samples) {
          db->signals[idx].cap_samples *= 2;
          db->signals[idx].samples = realloc(
              db->signals[idx].samples,
              db->signals[idx].cap_samples * sizeof(struct time_value_pair));
        }

        db->signals[idx].samples[db->signals[idx].num_samples].time =
            current_time;
        db->signals[idx].samples[db->signals[idx].num_samples].val = val;
        db->signals[idx].num_samples++;
      }

      else {
        continue; // for any other thing beside $var and $enddefinitions
      }
    }
  }
  db->count_of_lines = count_of_lines;
  free(id_array);
  free(line);
  fclose(fp);

  return db;
}
