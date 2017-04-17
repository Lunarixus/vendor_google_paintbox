/*
 * Copyright (C) 2017 The Android Open Source Project
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

/*
 * A static class with helper functions used to control subsystem clocks.
 */

#ifndef __EASEL_CLOCK_CONTROL_H__
#define __EASEL_CLOCK_CONTROL_H__

#include <stdio.h>
#include <unistd.h>

class EaselClockControl {

public:
    enum Subsystem {
        CPU,
        IPU,
        LPDDR,
    };

    /*
     * Gets the current frequency of the given subsystem.
     *
     * Returns the frequency in MHz for success; otherwise it returns error
     * code.
     */
    static int getFrequency(enum Subsystem system);

    /*
     * Sets the frequency of the given subsystem at the supplied frequency in
     * MHz.
     *
     * Returns zero for success or -1 for failure.
     */
    static int setFrequency(enum Subsystem, int freq);

    /*
     * Return the current state of sys200 mode.
     *
     * Returns zero for success or -1 for failure.
     */
    static int getSys200Mode(bool *enable);

    /*
     * Sets the state of sys200 mode.
     *
     * Returns zero for success or -1 for failure.
     */
    static int setSys200Mode();

    /* Sets the state of bypass clock gating.
     *
     * Returns zero for success of -1 for failure.
     */
    static int setBypassMode(bool enable);

private:
    static int openSysFile(char *file);
    static int closeSysFile(int fd);
    static int readSysFile(char *file, char *buf, size_t count);
    static int writeSysFile(char *file, char *buf, size_t count);

    static int getProcessorFrequency(char *sysFile);
    static int getLpddrFrequency();

    static int setProcessorFrequency(char *sysFile, const int *validFrequencies,
                                     int validCnt, int freq);
    static int setLpddrFrequency(int freq);
};

#endif // __EASEL_CLOCK_CONTROL_H__
