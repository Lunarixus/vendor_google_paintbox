#include "easelcomm.h"

#include <thread>

#include <assert.h>
#include <condition_variable>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
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

// Dynamically generated files truncated to this max size in bytes
static const int kDynamicMaxSize = 8 * 1024; // 8KB

#define SHELL_PATH      "/bin/sh"

enum Command {
    CMD_OPEN_SHELL,    // Open new shell session
    CMD_TTY_DATA,      // Data for writing to local TTY
    CMD_CLOSE_SHELL,   // Close the shell session
    CMD_PULL_REQUEST,  // Request pull file from Easel
    CMD_PULL_RESPONSE, // Pull file Easel-side response
    CMD_PUSH_REQUEST,  // Request push file to Easel
    CMD_PUSH_RESPONSE, // Push file Easel-side response
};

// Common message header for all messages containing the command and data len
struct MsgHeader {
    uint32_t command;
    uint32_t datalen;
    MsgHeader(enum Command cmd, uint32_t dlen): command(cmd), datalen(dlen) {};
    MsgHeader(): command(-1), datalen(0) {};
};

// Open shell session message from server to client, no further data
struct OpenShellMsg {
    struct MsgHeader h;
    OpenShellMsg(): h(CMD_OPEN_SHELL, 0) {};
};

// TTY data message contains bytes to write to remote TTY/PTY
struct TtyDataMsg {
    struct MsgHeader h;
    char data[kMaxTtyDataBufferSize];
    TtyDataMsg(): h(CMD_TTY_DATA, 0) {};
};

// Close session message from server to client, no further data
struct CloseShellMsg {
    struct MsgHeader h;
    CloseShellMsg(): h(CMD_CLOSE_SHELL, 0) {};
};

// File pull request from client to server contains server file path
struct FilePullRequest {
    struct MsgHeader h;
    char path[PATH_MAX];
    FilePullRequest(): h(CMD_PULL_REQUEST, 0) {};
};

// File pull response from server to client contains response code
struct FilePullResponse {
    struct MsgHeader h;
    int response_code;
    FilePullResponse(int respcode):
        h(CMD_PULL_RESPONSE, sizeof(response_code)), response_code(respcode)
        {};
};

// File push request from client to server contains server file path
struct FilePushRequest {
    struct MsgHeader h;
    char path[PATH_MAX];
    FilePushRequest(): h(CMD_PUSH_REQUEST, 0) {};
};

// File push response from server to client contains response code
struct FilePushResponse {
    struct MsgHeader h;
    int response_code;
    FilePushResponse(int respcode):
        h(CMD_PUSH_RESPONSE, sizeof(response_code)), response_code(respcode)
        {};
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

// client: file transfer info shared between main and command handler
char *file_xfer_path_remote;            // remote file path
char *file_xfer_path_local;             // local file path
std::mutex file_xfer_lock;              // protects following fields
bool file_xfer_done = false;            // is file transfer done?
std::condition_variable file_xfer_cond; // signals transfer done to waiter

void client_exit(int exitcode) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_terminal_state);
    fprintf(stderr, "\rezlsh exiting\n");
    easel_comm_client.close();
    exit(exitcode);
}

void client_xfer_done() {
    // File transfer done, wake up main thread
    {
        std::lock_guard<std::mutex> xferlock(file_xfer_lock);
        file_xfer_done = true;
    }
    file_xfer_cond.notify_one();
}

// Client receives file push response.
void client_push_response_handler(EaselComm::EaselMessage *msg) {
    FilePushResponse *resp = (FilePushResponse *)msg->message_buf;

    if (resp->response_code) {
        fprintf(stderr, "ezlsh: %s: %s\n", file_xfer_path_remote,
                strerror(resp->response_code));
    }

    client_xfer_done();
}

// Client saves pulled file based on response from server.
void client_save_pulled_file(EaselComm::EaselMessage *msg) {
    FilePullResponse *resp = (FilePullResponse *)msg->message_buf;

    if (resp->response_code) {
        fprintf(stderr, "ezlsh: %s: %s\n", file_xfer_path_remote,
                strerror(resp->response_code));
        return;
    }

    char *file_data = nullptr;

    if (msg->dma_buf_size) {
        file_data = (char *)malloc(msg->dma_buf_size);

        if (file_data == nullptr) {
            perror("malloc");
            // Discard DMA transfer
            msg->dma_buf = nullptr;
            (void) easel_comm_client.receiveDMA(msg);
            return;
        }

        msg->dma_buf = file_data;
        int ret = easel_comm_client.receiveDMA(msg);
        if (ret) {
            perror("EaselComm receiveDMA");
            free(file_data);
            return;
        }
    }

    int fd = creat(file_xfer_path_local, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        perror(file_xfer_path_local);
    } else if (msg->dma_buf_size) {
        ssize_t len = write(fd, file_data, msg->dma_buf_size);
        if (len < 0)
            perror(file_xfer_path_local);
    }

    close(fd);
    free(file_data);
}

// Client receives file pull response.
void client_pull_response_handler(EaselComm::EaselMessage *msg) {
    client_save_pulled_file(msg);
    client_xfer_done();
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

            switch(h->command) {
            case CMD_TTY_DATA:
            {
                TtyDataMsg *d = (TtyDataMsg *)msg.message_buf;
                ret = write(tty_fd, d->data, d->h.datalen);
                if (ret < 0)
                    perror("write");
                break;
            }

            case CMD_CLOSE_SHELL:
                close_conn = true;
                break;

            case CMD_PUSH_RESPONSE:
                client_push_response_handler(&msg);
                break;

            case CMD_PULL_RESPONSE:
                client_pull_response_handler(&msg);
                break;

            default:
                fprintf(stderr, "ERROR: unrecognized command %d\n",
                        h->command);
            }

            free(msg.message_buf);
        }
    }
    client_exit(exitcode);
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
    if (msg_handler_thread == nullptr) {
        fprintf(stderr, "failed to allocate thread for message handler\n");
        client_exit(1);
    }

    // Tell server to start a new session
    OpenShellMsg open_msg;
    EaselComm::EaselMessage msg;
    msg.message_buf = &open_msg;
    msg.message_buf_size = sizeof(MsgHeader);
    msg.dma_buf = 0;
    msg.dma_buf_size = 0;
    easel_comm_client.sendMessage(&msg);

    TtyDataMsg data_msg;

    while ((ret = read(STDIN_FILENO, &data_msg.data,
                       kMaxTtyDataBufferSize)) > 0) {
        data_msg.h.datalen = ret;

        EaselComm::EaselMessage msg;
        msg.message_buf = &data_msg;
        msg.message_buf_size = ret + sizeof(MsgHeader);
        msg.dma_buf = 0;
        msg.dma_buf_size = 0;
        ret = easel_comm_client.sendMessage(&msg);
        if (ret)
            break;
    }

    client_exit(0);
}

// Client file pull command processing. Send pull request and wait for
// incoming message handler to process the response from server.
void client_pull_file(char *remote_path, char *dest_arg) {
    file_xfer_path_remote = remote_path;
    char *local_path_str;

    if (dest_arg) {
        local_path_str = strdup(dest_arg);
        file_xfer_path_local = local_path_str;
    } else {
        local_path_str = strdup(remote_path);
        file_xfer_path_local = basename(local_path_str);
    }

    int ret = easel_comm_client.open(EaselComm::EASEL_SERVICE_SHELL);
    assert(ret == 0);
    easel_comm_client.flush();

    std::thread *msg_handler_thread;
    msg_handler_thread = new std::thread(client_message_handler);
    if (msg_handler_thread == nullptr) {
        fprintf(stderr, "failed to allocate thread for message handler\n");
        client_exit(1);
    }

    // Send request
    FilePullRequest pull_msg;
    strlcpy(pull_msg.path, remote_path, PATH_MAX);
    pull_msg.h.datalen = strlen(pull_msg.path) + 1;
    EaselComm::EaselMessage msg;
    msg.message_buf = &pull_msg;
    msg.message_buf_size = sizeof(MsgHeader) + pull_msg.h.datalen;
    msg.dma_buf = 0;
    msg.dma_buf_size = 0;
    easel_comm_client.sendMessage(&msg);

    // Wait for transfer done
    std::unique_lock<std::mutex> lk(file_xfer_lock);
    file_xfer_cond.wait(lk, [&]{return file_xfer_done;});

    free(local_path_str);
}

// Client file push command processing. Send push request and wait for
// incoming message handler to process the response from server.
void client_push_file(char *local_path, char *remote_path) {
    file_xfer_path_remote = remote_path;
    file_xfer_path_local = local_path;

    int ret = easel_comm_client.open(EaselComm::EASEL_SERVICE_SHELL);
    assert(ret == 0);
    easel_comm_client.flush();

    std::thread *msg_handler_thread;
    msg_handler_thread = new std::thread(client_message_handler);
    if (msg_handler_thread == nullptr) {
        fprintf(stderr, "failed to allocate thread for message handler\n");
        client_exit(1);
    }

    int fd = open(local_path, O_RDONLY);
    if (fd < 0) {
        perror(local_path);
        client_exit(1);
    }
    struct stat stat_buf;
    ssize_t data_len = 0;

    if (fstat(fd, &stat_buf) < 0) {
        perror(local_path);
        close(fd);
        client_exit(1);
    }

    // Read file data
    char *file_data = (char *)malloc(stat_buf.st_size);
    if (file_data == nullptr) {
        perror("malloc");
        close(fd);
        client_exit(1);
    }
    data_len = read(fd, file_data, stat_buf.st_size);

    // Send request, send file data as DMA buffer
    FilePushRequest push_msg;
    strlcpy(push_msg.path, remote_path, PATH_MAX);
    push_msg.h.datalen = strlen(push_msg.path) + 1;
    EaselComm::EaselMessage msg;
    msg.message_buf = &push_msg;
    msg.message_buf_size = sizeof(MsgHeader) + push_msg.h.datalen;
    msg.dma_buf = file_data;
    msg.dma_buf_size = data_len;
    easel_comm_client.sendMessage(&msg);

    // Wait for transfer done
    std::unique_lock<std::mutex> lk(file_xfer_lock);
    file_xfer_cond.wait(lk, [&]{return file_xfer_done;});
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
            perror(SHELL_PATH);
            exit(2);
        }
    } else {
        TtyDataMsg data_msg;
        int ret;

        while ((ret = read(tty_fd, &data_msg.data,
                           kMaxTtyDataBufferSize)) > 0) {
            data_msg.h.datalen = ret;

            EaselComm::EaselMessage msg;
            msg.message_buf = &data_msg;
            msg.message_buf_size = ret + sizeof(MsgHeader);
            msg.dma_buf = 0;
            msg.dma_buf_size = 0;
            ret = easel_comm_server.sendMessage(&msg);
            if (ret)
                break;
        }

        // EOF from server shell PTY; tell client to close its connection
        CloseShellMsg close_msg;
        EaselComm::EaselMessage msg;
        msg.message_buf = &close_msg;
        msg.message_buf_size = sizeof(MsgHeader);
        msg.dma_buf = 0;
        msg.dma_buf_size = 0;
        easel_comm_server.sendMessage(&msg);
   }
}

// Server receives and saves file pushed from client
int server_recv_push_file(EaselComm::EaselMessage *msg) {
    FilePushRequest *req = (FilePushRequest *)msg->message_buf;
    char *file_data = nullptr;
    int ret = 0;

    if (msg->dma_buf_size) {
        file_data = (char *)malloc(msg->dma_buf_size);
        if (file_data == nullptr) {
            perror("malloc");
            // Discard DMA transfer
            msg->dma_buf = nullptr;
            (void) easel_comm_server.receiveDMA(msg);
            return ENOMEM;
        }

        msg->dma_buf = file_data;
        ret = easel_comm_server.receiveDMA(msg);
        if (ret) {
            perror("EaselComm receiveDMA");
            free(file_data);
            return ret;
        }
    }

    int fd = creat(req->path, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        ret = errno;
        perror(req->path);
    } else if (msg->dma_buf_size) {
        ssize_t len = write(fd, file_data, msg->dma_buf_size);
        if (len < 0) {
            ret = errno;
            perror(req->path);
        }
    }

    close(fd);
    free(file_data);
    return ret;
}

// Server receives file push request from client
void server_push_file(EaselComm::EaselMessage *push_msg) {
    int resp = server_recv_push_file(push_msg);
    FilePushResponse push_resp(resp);
    EaselComm::EaselMessage resp_msg;
    resp_msg.message_buf = &push_resp;
    resp_msg.message_buf_size = sizeof(FilePushResponse);
    resp_msg.dma_buf = NULL;
    resp_msg.dma_buf_size = 0;
    easel_comm_server.sendMessage(&resp_msg);
}

// Server receives file pull request, send file data as DMA transfer
void server_pull_file(FilePullRequest *req) {
    int fd = open(req->path, O_RDONLY);
    off_t file_size = -1;
    struct stat stat_buf;
    ssize_t data_len = 0;
    ssize_t data_max_len;

    if (fd >= 0) {
        if (fstat(fd, &stat_buf) == 0)
            file_size = stat_buf.st_size;
    }

    if (file_size < 0) {
        // Send errno as response code
        FilePullResponse pull_resp(errno);
        EaselComm::EaselMessage msg;
        msg.message_buf = &pull_resp;
        msg.message_buf_size = sizeof(FilePullResponse);
        msg.dma_buf = 0;
        msg.dma_buf_size = 0;
        easel_comm_server.sendMessage(&msg);
        close(fd);
        return;
    }

    // If size is zero it may be dynamically generated, send up to a
    // maximum number of bytes configured above.
    data_max_len = file_size ? file_size : kDynamicMaxSize;

    // Read file, send errno as response code and file data as DMA buffer
    // (if successful).
    errno = 0;
    char *file_data = (char *)malloc(data_max_len);
    if (file_data != nullptr)
        data_len = read(fd, file_data, data_max_len);
    // If dynamically-generated file larger than our max then send error
    if (!file_size && data_len == kDynamicMaxSize)
        errno = EFBIG;

    FilePullResponse pull_resp(errno);
    EaselComm::EaselMessage msg;
    msg.message_buf = &pull_resp;
    msg.message_buf_size = sizeof(FilePullResponse);
    msg.dma_buf = (errno || !data_len) ? NULL : file_data;
    msg.dma_buf_size = errno ? 0 : data_len;
    easel_comm_server.sendMessage(&msg);

    free(file_data);
    close(fd);
}

void server_run(bool flush) {
    int ret;

    ret = easel_comm_server.open(EaselComm::EASEL_SERVICE_SHELL);
    assert(ret == 0);
    if (flush) {
        easel_comm_server.flush();
    }

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

            switch(h->command) {
            case CMD_OPEN_SHELL:
                server_kill_shell();
                shell_session_thread =
                  new std::thread(shell_server_session);
                if (shell_session_thread == nullptr)
                    fprintf(stderr,
                          "failed to allocate thread for shell session\n");
                break;

            case CMD_TTY_DATA:
            {
                TtyDataMsg *d = (TtyDataMsg *)msg.message_buf;
                ret = write(tty_fd, d->data, d->h.datalen);
                if (ret < 0)
                    perror("write");
                break;
            }

            case CMD_CLOSE_SHELL:
                server_kill_shell();
                break;

            case CMD_PUSH_REQUEST:
                server_push_file(&msg);
                break;

            case CMD_PULL_REQUEST:
                server_pull_file((FilePullRequest *)msg.message_buf);
                break;

            default:
                fprintf(stderr, "ERROR: unrecognized command %d\n",
                        h->command);
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
    int server_needs_flush = 0;  // 0 is not to flush.  Default is 0.

    const char *short_opt = "dh";
    struct option long_opt[] =
        {
          {"daemon",  no_argument,   0, 'd'},
          {"flush",   no_argument,   &server_needs_flush, 1},
          {"help",    no_argument,   0, 'h'},
          {0, 0, 0, 0}
        };

    while ((ch = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1) {
        switch (ch) {
          case 0:
            /* getopt_long() set a value */
            break;
          case 'd':
            client = 0;
            break;
          case 'h':
            /* FALLTHRU */
            ;
          default:
            fprintf(stderr,
                    "Usage: server: ezlsh <-d|--daemon> [--flush]\n");
            fprintf(stderr,
                    "       client: ezlsh\n"
                    "       client: ezlsh pull <remote-path> [<local-path>]\n"
                    "       client: ezlsh push <local-path> <remote-path>\n"
                    );
            exit(1);
        }
    }

    if (client) {
        if (optind < argc) {
            if (!strcmp(argv[optind], "pull")) {
                char *remote_path;
                char *local_path = NULL;

                if (++optind >= argc) {
                    fprintf(stderr,
                          "ezlsh: pull: remote-path missing\n");
                    exit(1);
                }

                remote_path = argv[optind];
                if (++optind < argc)
                    local_path = argv[optind];
                client_pull_file(remote_path, local_path);

            } else if (!strcmp(argv[optind], "push")) {
                char *remote_path;
                char *local_path;

                if (++optind >= argc) {
                    fprintf(stderr,
                          "ezlsh: push: local-path missing\n");
                    exit(1);
                }

                local_path = argv[optind];

                if (++optind >= argc) {
                  fprintf(stderr,
                          "ezlsh: push: remote-path missing\n");
                  exit(1);
                }
                remote_path = argv[optind];
                client_push_file(local_path, remote_path);

           } else {
                fprintf(stderr,
                        "ezlsh: unknown command \"%s\"\n", argv[optind]);
                exit(1);
            }
        } else {
            // No command, run shell session
            shell_client_session();
        }
    } else {
        server_run(server_needs_flush);
    }
}
