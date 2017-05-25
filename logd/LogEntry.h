#ifndef PAINTBOX_LOG_ENTRY_H
#define PAINTBOX_LOG_ENTRY_H

// Struct to decompose log message to prio, tag and text
struct LogEntry {
  const int prio;
  const char *tag;
  const char *text;
};

// Parses msg to LogEntry.
LogEntry parseEntry(const char* msg, unsigned short len);

#endif  // PAINTBOX_LOG_ENTRY_H