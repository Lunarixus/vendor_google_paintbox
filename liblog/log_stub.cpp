/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Dummy log API to work around build error.
// Reference:
// https://cs.corp.google.com/android/system/core/debuggerd/libdebuggerd/test/log_fake.cpp

#include <errno.h>
#include <stdarg.h>

#include <string>

#include <android-base/stringprintf.h>
#include <log/log.h>

// Forward declarations.
struct EventTagMap;
struct AndroidLogEntry;

extern "C" int __android_log_buf_write(int, int, const char*, const char*) {
  return 0;
}

extern "C" log_id_t android_name_to_log_id(const char*) {
  return LOG_ID_SYSTEM;
}

extern "C" struct logger_list* android_logger_list_open(log_id_t, int, unsigned int, pid_t) {
  errno = EACCES;
  return nullptr;
}

extern "C" int android_logger_list_read(struct logger_list*, struct log_msg*) {
  return 0;
}

extern "C" EventTagMap* android_openEventTagMap(const char*) {
  return nullptr;
}

extern "C" int android_log_processBinaryLogBuffer(
    struct logger_entry*,
    AndroidLogEntry*, const EventTagMap*, char*, int) {
  return 0;
}

extern "C" void android_logger_list_free(struct logger_list*) {
}