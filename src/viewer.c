#include "viewer.h"
#include <ncurses.h>
#include <string.h>

#define MAX_SIGNALS 128
void create_waveform(struct signal_details *sig, int y, int offset_idx) {
  if (sig->num_samples == 0)
    return;
  int max_offset = sig->num_samples - 1;
  if (offset_idx > max_offset)
    offset_idx = max_offset;
  if (offset_idx < 0)
    offset_idx = 0;
  int x1 = 0;
  int x2 = 0;
  int waveform_start = 35; // start coordinate of waveform
  long t_ref = sig->samples[offset_idx].time;
  if (sig->width > 1) {
    for (int i = 0; i < sig->num_samples - 1 - offset_idx; i++) {
      x1 = waveform_start + (sig->samples[i + offset_idx].time - t_ref);
      x2 = waveform_start + (sig->samples[i + 1 + offset_idx].time - t_ref);
      mvhline(y, x1, ACS_HLINE, x2 - x1);
      mvhline(y + 2, x1, ACS_HLINE, x2 - x1);
      char label[16];
      snprintf(label, 16, "%lx", sig->samples[i + offset_idx].val);
      mvprintw(y + 1, x1 + (x2 - x1) / 2 - (int)strlen(label) / 2, "%s", label);
      if (sig->samples[i + offset_idx].val !=
          sig->samples[i + 1 + offset_idx].val) {
        mvaddch(y, x2 - 1, ACS_TTEE);
        mvaddch(y + 1, x2 - 1, ACS_VLINE);
        mvaddch(y + 2, x2 - 1, ACS_BTEE);
      }
    }
    int last = sig->num_samples - 1;
    int x_last = waveform_start + (sig->samples[last].time - t_ref);
    int x_end = getmaxx(stdscr);

    mvhline(y, x_last, ACS_HLINE, x_end - x_last);
    mvhline(y + 2, x_last, ACS_HLINE, x_end - x_last);
    char label[16];
    snprintf(label, sizeof(label), "%lx", sig->samples[last].val);
    mvprintw(y + 1, x_last + (x_end - x_last) / 2 - (int)strlen(label) / 2,
             "%s", label);
  } else {
    for (int i = 0; i < sig->num_samples - 1 - offset_idx; i++) {
      long t1 = sig->samples[i].time;
      long t2 = sig->samples[i + 1].time;
      unsigned long val = sig->samples[i + offset_idx].val;
      unsigned long next_val = sig->samples[i + 1 + offset_idx].val;
      x1 = waveform_start + (sig->samples[i + offset_idx].time - t_ref);
      x2 = waveform_start + (sig->samples[i + 1 + offset_idx].time - t_ref);
      for (int x = x1; x < x2; x++) {
        if (val)
          mvaddch(y, x, ACS_HLINE);
        else
          mvaddch(y + 1, x, ACS_HLINE);
      }

      if (val != next_val) {
        mvaddch(y, x2 - 1, ACS_VLINE);
        mvaddch(y + 1, x2 - 1, ACS_VLINE);
        if (next_val == 0) {
          mvaddch(y, x2 - 1, ACS_URCORNER);
          mvaddch(y + 1, x2 - 1, ACS_LLCORNER);
        } else {
          mvaddch(y + 1, x2 - 1, ACS_LRCORNER);
          mvaddch(y, x2 - 1, ACS_ULCORNER);
        }
      }
    }
    int last = sig->num_samples - 1;
    long t_last = sig->samples[last].time;
    unsigned long val_last = sig->samples[last].val;
    int x_last = waveform_start + (t_last - t_ref);
    int x_end = getmaxx(stdscr);

    for (int x = x_last; x < x_end; x++) {
      if (val_last)
        mvaddch(y, x, ACS_HLINE);
      else
        mvaddch(y + 1, x, ACS_HLINE);
    }
  }
}

void viewer(const struct vcd_data *db) {
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);

  int height = getmaxy(stdscr) - 2;
  int width = 30;

  WINDOW *win = newwin(height, width, 0, 0);

  if (win == NULL) {
    endwin();
    return;
  }

  int selected = 0;
  int offset_idx = 0;
  int cursor_x = 35; // cursor starts from 35
  long cursor_time;
  int waveform_start = 35;

  int shown[MAX_SIGNALS] = {0};
  while (1) {

    werase(win);
    erase();
    mvprintw(0, width, "TS=%s", db->signals->samples->timescale);

    int x_end = getmaxx(stdscr);
    {
      struct signal_details *ref = &db->signals[0];
      int safe_offset = offset_idx;
      if (safe_offset > ref->num_samples - 1)
        safe_offset = ref->num_samples - 1;
      if (safe_offset < 0)
        safe_offset = 0;

      long t_ref = ref->samples[safe_offset].time;
      cursor_time = (cursor_x - waveform_start) + t_ref;
      for (int i = 0; i < ref->num_samples - 1 - safe_offset; i++) {
        int x1 = waveform_start + ref->samples[i].time;
        int x2 = waveform_start + ref->samples[i + 1].time;
        mvprintw(0, x1, "%lu", ref->samples[i + safe_offset].time);
        mvprintw(0, x2, "%lu", ref->samples[i + 1 + safe_offset].time);
      }
    }

    box(win, 0, 0);

    mvwprintw(win, 0, (width - strlen("SIGNAL LIST")) / 2, " SIGNAL LIST ");
    mvprintw(getmaxy(stdscr) - 1, 0,
             "PRESS q to EXIT | Key_Up or Key_Down to NAVIGATE | Backspace to "
             "SHOW WAVE "
             "(PRESS AGAIN TO HIDE WAVE) | <- -> to SCROLL | h to move cursor "
             "left & l to move cursor right");

    mvprintw(getmaxy(stdscr) - 2, 0, "time = %ld", cursor_time);

    for (int i = 0; i < db->count_of_lines; i++) {
      if (i == selected)
        wattron(win, A_REVERSE);

      mvwprintw(win, i * 3 + 1, 2, "%s", db->signals[i].signal_name);
      if (i == selected)
        wattroff(win, A_REVERSE);
    }

    wrefresh(win);

    for (int i = 0; i < db->count_of_lines; i++) {
      if (shown[i]) {
        create_waveform(&db->signals[i], i * 3 + 1, offset_idx);
      }
    }
    if (cursor_x >= waveform_start && cursor_x < x_end) {
      mvwvline(stdscr, 1, cursor_x, ACS_VLINE, height);
    }

    touchwin(win);
    wnoutrefresh(stdscr);
    wnoutrefresh(win);
    doupdate();
    int ch = getch();

    switch (ch) {

    case KEY_UP:
      if (selected > 0)
        selected--;
      else if (selected == 0)
        selected = db->count_of_lines - 1;
      break;

    case KEY_DOWN:
      if (selected < db->count_of_lines - 1)
        selected++;
      else if (selected == db->count_of_lines - 1)
        selected = 0;
      break;

    case KEY_BACKSPACE:
      shown[selected] = !shown[selected];
      // deselect if the waveform is already showing
      break;

    case KEY_RIGHT:
      if (offset_idx < db->count_of_lines)
        offset_idx++;
      break;

    case KEY_LEFT:
      if (offset_idx > 0)
        offset_idx--;
      break;

    case 'h':
      if (cursor_x > 35)
        cursor_x--;
      break;

    case 'l':
      if (cursor_x < x_end - 1)
        cursor_x++;
      break;

    case 'q':
      delwin(win);
      endwin();
      return;
    }
  }
}
