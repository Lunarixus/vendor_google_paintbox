#define _BSD_SOURCE /* for endian.h macros */

#include "easelcomm.h"
#ifdef MOCKEASEL
#include "mockeaselcomm.h"
#endif

#include <thread>

#include <assert.h>
#include <endian.h>
#include <fcntl.h>
#include <pthread.h>
#include <pty.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace {
static const int kMaxTtyDataBufferSize = 2048;

#ifdef ANDROID
#define SHELL_PATH      "/system/bin/sh"
#else
#define SHELL_PATH      "/bin/sh"
#endif

enum Command {
    CMD_TTY_DATA,  // Data for writing to local TTY
    CMD_CLOSE,     // Close the connection
};

// Common message header for all messages containing the command and data len
struct MsgHeader {
    uint32_t command;
    uint32_t datalen;
};

// TTY data message contains bytes to write to remote TTY/PTY
struct TtyDataMsg {
    struct MsgHeader h;
    char data[kMaxTtyDataBufferSize];
};

// Close connection message from server to client, no further data
struct CloseMsg {
    struct MsgHeader h;
};

// Local tty/pty file descriptor
int tty_fd;
// Message handler thread
std::thread *msg_handler_thread;

#ifdef MOCKEASEL
EaselCommClientNet easel_comm_client;
EaselCommServerNet easel_comm_server;
#else
EaselCommClient easel_comm_client;
EaselCommServer easel_comm_server;
#endif

termios saved_terminal_state;

void exit_shell(bool client, int exitcode) {
    if (client) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_terminal_state);
        fprintf(stderr, "\rezlsh exiting\n");
        easel_comm_client.close();
    } else {
        easel_comm_server.close();
    }

    exit(exitcode);
}

// --------------------------------------------------------------------------
// Incoming message handler (both client and server)

void msgHandlerThread(EaselComm *easel_comm, bool client) {
    bool close_conn = false;
    int exitcode = 0;

    // Read messages from remote and write the data to local tty.
    while (!close_conn) {
        EaselComm::EaselMessage msg;
        int ret = easel_comm->receiveMessage(&msg);
        if (ret) {
            if (client && errno != ESHUTDOWN)
                perror("");
            exitcode = errno;
            break;
        }

        if (msg.message_buf_size) {
            MsgHeader *h = (MsgHeader *)msg.message_buf;

            switch(be32toh(h->command)) {
            case CMD_TTY_DATA:
            {
                TtyDataMsg *d = (TtyDataMsg *)msg.message_buf;
                write(tty_fd, d->data, be32toh(d->h.datalen));
                break;
            }
            case CMD_CLOSE:
                close_conn = true;
                break;
            default:
                fprintf(stderr, "ERROR: unrecognized command %d\n",
                        be32toh(h->command));
            }

            free(msg.message_buf);
        }
    }
    exit_shell(client, exitcode);
}

void shell_client_session() {
    tty_fd = STDOUT_FILENO;

    if (tcgetattr(STDIN_FILENO, &saved_terminal_state)) {
        perror("tcgetattr");
        exit(1);
    }

    termios tio;
    tcgetattr(STDIN_FILENO, &tio);
    cfmakeraw(&tio);
    // No timeout but request at least one character per read.
    tio.c_cc[VTIME] = 0;
    tio.c_cc[VMIN] = 1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &tio);

    msg_handler_thread = new std::thread(msgHandlerThread, &easel_comm_client,
                                         true);

    TtyDataMsg data_msg;
    int ret;

    data_msg.h.command = htobe32(CMD_TTY_DATA);

    while ((ret = read(STDIN_FILENO, &data_msg.data,
                       kMaxTtyDataBufferSize)) > 0) {
        data_msg.h.datalen = htobe32(ret);

        EaselComm::EaselMessage msg;
        msg.message_buf = &data_msg;
        msg.message_buf_size = ret + sizeof(MsgHeader);
        msg.dma_buf = 0;
        msg.dma_buf_size = 0;
        ret = easel_comm_client.sendMessage(&msg);
        if (ret)
            break;
    }

    exit_shell(true, 0);
}

void shell_server_session() {
    int ret;

    pid_t shell_pid;
    shell_pid = forkpty(&tty_fd, NULL, NULL, NULL);
    if (shell_pid < 0) {
        perror("forkpty");
        exit(1);
    } else if (shell_pid == 0) {
        ret = execle(SHELL_PATH, SHELL_PATH, "-", nullptr, environ);
        if (ret < 0) {
            perror("execle");
            exit(2);
        }
    } else {
        ret = easel_comm_server.open(EaselComm::EASEL_SERVICE_SHELL);
        assert(ret == 0);

        msg_handler_thread =
            new std::thread(msgHandlerThread, &easel_comm_server, false);
        TtyDataMsg data_msg;
        int ret;

        data_msg.h.command = htobe32(CMD_TTY_DATA);

        while ((ret = read(tty_fd, &data_msg.data,
                           kMaxTtyDataBufferSize)) > 0) {
            data_msg.h.datalen = htobe32(ret);

            EaselComm::EaselMessage msg;
            msg.message_buf = &data_msg;
            msg.message_buf_size = ret + sizeof(MsgHeader);
            msg.dma_buf = 0;
            msg.dma_buf_size = 0;
            ret = easel_comm_server.sendMessage(&msg);
            if (ret)
                break;
        }

        // Tell client to close its connection
        CloseMsg close_msg;
        close_msg.h.command = htobe32(CMD_CLOSE);
        close_msg.h.datalen = 0;

        EaselComm::EaselMessage msg;
        msg.message_buf = &close_msg;
        msg.message_buf_size = sizeof(MsgHeader);
        msg.dma_buf = 0;
        msg.dma_buf_size = 0;
        easel_comm_server.sendMessage(&msg);
        /*
         * Wait for client to close connection first; we will exit when client
         * close is seen (as EOF from socket read).
         */
        sleep(2);
        // Not expected to reach here.
        exit(3);
   }
}
} // anonymous namespace

int main(int argc, char **argv) {
    int ch;
    int client = 1;
    int ret;

    while ((ch = getopt(argc, argv, "d")) != -1) {
        switch (ch) {
          case 'd':
            client = 0;
            break;
          default:
            fprintf(stderr, "Usage: server: ezlsh -d\n");
            fprintf(stderr, "       client: ezlsh [host]\n");
            exit(1);
        }
    }

    if (client) {
        const char *localhost = "localhost";
        ret = easel_comm_client.open(EaselComm::EASEL_SERVICE_SHELL);
        assert(ret == 0);
#ifdef MOCKEASEL
        easel_comm_client.connect(
            optind < argc ? argv[optind] : localhost);
#endif
        shell_client_session();
    } else {
        shell_server_session();
    }

    // Not expected to reach here.
    exit(3);
}
