#include <string>

#include "LogEntry.h"

LogEntry parseEntry(const char* msg, unsigned short len) {
  int prio = *msg;
  const char *tag = msg + 1;
  const char *text = tag + strnlen(tag, len - 1) + 1;
  return {prio, tag, text};
}
