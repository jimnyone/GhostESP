#ifndef TERMINAL_VIEW_H
#define TERMINAL_VIEW_H

#include "lvgl.h"
#include "managers/display_manager.h"
#include "managers/ap_manager.h"

extern View terminal_view;

void terminal_view_add_text(const char *text);
size_t terminal_view_log_count(void);
bool terminal_view_log_get(size_t index, char *out, size_t out_len);
size_t terminal_view_log_count(void);
bool terminal_view_log_get(size_t index, char *out, size_t out_len);

void terminal_view_create(void);

void terminal_view_destroy(void);

#ifdef CONFIG_WITH_SCREEN
#define TERMINAL_VIEW_ADD_TEXT(fmt, ...)                                       \
  do {                                                                         \
    char buffer[350];                                                          \
    snprintf(buffer, sizeof(buffer), fmt, ##__VA_ARGS__);                      \
    terminal_view_add_text(buffer);                                            \
    ap_manager_add_log(buffer);                                               \
  } while (0)
#else
#define TERMINAL_VIEW_ADD_TEXT(fmt, ...)                                       \
  do {                                                                         \
    char buffer[350];                                                         \
    snprintf(buffer, sizeof(buffer), fmt, ##__VA_ARGS__);                      \
    ap_manager_add_log(buffer);                                               \
  } while (0)
#endif

<<<<<<< HEAD
#endif // TERMINAL_VIEW_H
=======
void terminal_screen_create(lv_obj_t* parent);

#endif // TERMINAL_VIEW_H
>>>>>>> 73ca60d6 (Merge branch 'scripts' into pr/338)
