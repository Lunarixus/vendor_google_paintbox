#define _BSD_SOURCE /* for endian.h macros */

#include "easelcomm.h"

#include <thread>

#include <assert.h>
#include <endian.h>
#include <fcntl.h>
#include <pthread.h>
#include <pty.h>
#include <signal.h>
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
    CMD_OPEN,      // Open new shell session
    CMD_TTY_DATA,  // Data for writing to local TTY
    CMD_CLOSE,     // Close the shell session
};

// Common message header for all messages containing the command and data len
struct MsgHeader {
    uint32_t command;
    uint32_t datalen;
};

// Open session message from server to client, no further data
struct OpenMsg {
    struct MsgHeader h;
};

// TTY data message contains bytes to write to remote TTY/PTY
struct TtyDataMsg {
    struct MsgHeader h;
    char data[kMaxTtyDataBufferSize];
};

// Close session message from server to client, no further data
struct CloseMsg {
    struct MsgHeader h;
};

// Local tty/pty file descriptor
int tty_fd;

EaselCommClient easel_comm_client;
EaselCommServer easel_comm_server;

// client saved terminal state
termios saved_terminal_state;

// server current shell PID
pid_t shell_pid = 0;
// server shell session thread
std::thread *shell_session_thread = nullptr;

void client_exit_shell(int exitcode) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_terminal_state);
    fprintf(stderr, "\rezlsh exiting\n");
    easel_comm_client.close();
    exit(exitcode);
}

// Client incoming message handler
void client_message_handler() {
    bool close_conn = false;
    int exitcode = 0;

    // Read messages from remote and write the data to local tty.
    while (!close_conn) {
        EaselComm::EaselMessage msg;
        int ret = easel_comm_client.receiveMessage(&msg);
        if (ret) {
            if (errno != ESHUTDOWN)
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
                ret = write(tty_fd, d->data, be32toh(d->h.datalen));
                if (ret < 0)
                    perror("write");
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
    client_exit_shell(exitcode);
}

void shell_client_session() {
    std::thread *msg_handler_thread;
    int ret;

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

    ret = easel_comm_client.open(EaselComm::EASEL_SERVICE_SHELL);
    assert(ret == 0);
    easel_comm_client.flush();

    msg_handler_thread = new std::thread(client_message_handler);

    // Tell server to start a new session
    OpenMsg open_msg;
    open_msg.h.command = htobe32(CMD_OPEN);
    open_msg.h.datalen = 0;

    EaselComm::EaselMessage msg;
    msg.message_buf = &open_msg;
    msg.message_buf_size = sizeof(MsgHeader);
    msg.dma_buf = 0;
    msg.dma_buf_size = 0;
    easel_comm_client.sendMessage(&msg);

    TtyDataMsg data_msg;

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

    client_exit_shell(0);
}

void server_kill_shell() {
    if (shell_pid) {
        if (kill(shell_pid, SIGHUP) < 0)
            perror("kill");
        shell_pid = 0;
    }

    if (shell_session_thread) {
        shell_session_thread->join();
        delete shell_session_thread;
        shell_session_thread = nullptr;
    }
}

void shell_server_session() {
    int ret;

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

        // EOF from server shell PTY;  tell client to close its connection
        CloseMsg close_msg;
        close_msg.h.command = htobe32(CMD_CLOSE);
        close_msg.h.datalen = 0;

        EaselComm::EaselMessage msg;
        msg.message_buf = &close_msg;
        msg.message_buf_size = sizeof(MsgHeader);
        msg.dma_buf = 0;
        msg.dma_buf_size = 0;
        easel_comm_server.sendMessage(&msg);
   }
}

void server_run() {
    int ret;

    ret = easel_comm_server.open(EaselComm::EASEL_SERVICE_SHELL);
    assert(ret == 0);
    easel_comm_server.flush();

    while (true) {
        EaselComm::EaselMessage msg;
        int ret = easel_comm_server.receiveMessage(&msg);
        if (ret) {
            if (errno == ESHUTDOWN) {
                server_kill_shell();
            } else {
                fprintf(stderr, "ERROR: receiveMessage returns %d\n", ret);
            }
            continue;
        }

        if (msg.message_buf_size) {
            MsgHeader *h = (MsgHeader *)msg.message_buf;

            switch(be32toh(h->command)) {
            case CMD_OPEN:
                server_kill_shell();
                shell_session_thread =
                  new std::thread(shell_server_session);
                break;

            case CMD_TTY_DATA:
            {
                TtyDataMsg *d = (TtyDataMsg *)msg.message_buf;
                ret = write(tty_fd, d->data, be32toh(d->h.datalen));
                if (ret < 0)
                    perror("write");
                break;
            }

            case CMD_CLOSE:
                server_kill_shell();
                break;

            default:
                fprintf(stderr, "ERROR: unrecognized command %d\n",
                        be32toh(h->command));
            }

            free(msg.message_buf);
        }
    }
    /*NOTREACHED*/
}

} // anonymous namespace

int main(int argc, char **argv) {
    int ch;
    int client = 1;

    while ((ch = getopt(argc, argv, "d")) != -1) {
        switch (ch) {
          case 'd':
            client = 0;
            break;
          default:
            fprintf(stderr, "Usage: server: ezlsh -d\n");
            fprintf(stderr, "       client: ezlsh\n");
            exit(1);
        }
    }

    if (client) {
        shell_client_session();
    } else {
        server_run();
    }

    // Not expected to reach here.
    exit(3);
}
