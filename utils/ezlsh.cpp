#define LOG_TAG "ezlsh"

#include "easelcomm.h"

#include <thread>

#include <algorithm>
#include <assert.h>
#include <condition_variable>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <pthread.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <log/log.h>

#include "LogClient.h"

namespace {
static const int kMaxTtyDataBufferSize = 2048;

// Dynamically generated files truncated to this max size in bytes
static const int kDynamicMaxSize = 8 * 1024; // 8KB

const char* kPowerOn = "/sys/devices/virtual/misc/mnh_sm/download";
const char* kPowerOff = "/sys/devices/virtual/misc/mnh_sm/poweroff";
const char* kStageFw = "/sys/devices/virtual/misc/mnh_sm/stage_fw";
const char* kSysState = "/sys/devices/virtual/misc/mnh_sm/state";

enum State {
    POWER_ON = 1,
    POWER_OFF = 0,
};

#define SHELL_PATH      "/bin/sh"

enum Command {
    CMD_OPEN_SHELL,    // Open new shell session
    CMD_TTY_DATA,      // Data for writing to local TTY
    CMD_CLOSE_SHELL,   // Close the shell session
    CMD_PULL_REQUEST,  // Request pull file from Easel
    CMD_PULL_RESPONSE, // Pull file Easel-side response
    CMD_PUSH_REQUEST,  // Request push file to Easel
    CMD_PUSH_RESPONSE, // Push file Easel-side response
    CMD_LS_REQUEST,    // Request ls directory in Easel
    CMD_LS_RESPONSE,   // Response ls diretory from Easel
    CMD_EXEC_REQUEST,  // Request to execute a command
    CMD_EXEC_RESPONSE, // Response to execute a command
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
    mode_t st_mode;
    FilePullResponse(int respcode):
        h(CMD_PULL_RESPONSE, sizeof(response_code)), response_code(respcode)
        {};
};

// File push request from client to server contains server file path
struct FilePushRequest {
    struct MsgHeader h;
    mode_t st_mode;
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

struct FileLsRequest {
    struct MsgHeader h;
    char path[PATH_MAX];
    FileLsRequest(): h(CMD_LS_REQUEST, 0) {};
};

struct FileLsResponse {
    struct MsgHeader h;
    FileLsResponse(): h(CMD_LS_RESPONSE, 0) {};
};

struct ExecRequest {
    struct MsgHeader h;
    char cmd[kMaxTtyDataBufferSize];
    ExecRequest(): h(CMD_EXEC_REQUEST, 0) {};
};

struct ExecResponse {
    struct MsgHeader h;
    bool done;
    int exit;
    char output[kMaxTtyDataBufferSize];
    ExecResponse(): h(CMD_EXEC_RESPONSE,
            sizeof(done) + sizeof(exit)), done(false), exit(0) {};
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

char *file_recursive_path_remote;              // remote file path
char *file_recursive_path_local;               // local file path
std::mutex file_recursive_lock;                // protects following fields
bool file_recursive_done = false;              // is ls transfer done?
std::condition_variable file_recursive_cond;   // signals ls done to waiter

std::mutex exec_lock;
bool exec_done = false;
std::condition_variable exec_cond;

void server_exec_cmd(ExecRequest *request);
int server_exec_cmd(std::string &cmd);

void client_exit(int exitcode) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_terminal_state);
    easel_comm_client.close();
    exit(exitcode);
}

// Returns true if easel power state matches expect_state.
bool client_check_state(State expect_state) {
    int state_value = 0;
    std::ifstream f(kSysState);
    f >> state_value;
    State state = static_cast<State>(state_value);
    if (state == expect_state) {
        return true;
    } else {
        fprintf(stderr,
                "Could not run ezlsh. Easel power state is %d, expect %d.\n",
                state,
                expect_state);
        return false;
    }
}

// Reads a sysfs node.
void read_sysfs_node(const char *node) {
    std::string s;
    std::ifstream f(node);
    f >> s;
}

// Writes a int to sysfs node.
void write_sysfs_node(const char *node, int value) {
    std::ofstream f(node);
    f << value;
}

void client_xfer_done() {
    // File transfer done, wake up main thread
    {
        std::lock_guard<std::mutex> lock(file_xfer_lock);
        file_xfer_done = true;
    }
    file_xfer_cond.notify_one();
}

void client_recursive_done() {
    // File transfer done, wake up main thread
    {
        std::lock_guard<std::mutex> lock(file_recursive_lock);
        file_recursive_done = true;
    }
    file_recursive_cond.notify_one();
}

void client_exec_done() {
    {
        std::lock_guard<std::mutex> lock(exec_lock);
        exec_done = true;
    }
    exec_cond.notify_one();
}

// Client receives file push response.
void client_push_response_handler(EaselComm::EaselMessage *msg) {
    FilePushResponse *resp = (FilePushResponse *)msg->message_buf;

    if (resp->response_code) {
        fprintf(stderr, "ERROR: ezlsh %s: %s: %s\n", __FUNCTION__,
                file_xfer_path_remote, strerror(resp->response_code));
    }

    client_xfer_done();
}

// Client saves pulled file based on response from server.
void client_save_pulled_file(EaselComm::EaselMessage *msg) {
    FilePullResponse *resp = (FilePullResponse *)msg->message_buf;

    if (resp->response_code) {
        fprintf(stderr, "ezlsh: %s: %s: %s\n", __FUNCTION__,
                file_xfer_path_remote, strerror(resp->response_code));
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

    int fd = creat(file_xfer_path_local, resp->st_mode);
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

const static std::string kFileSeparator = "/";
void client_pull_file(char *remote_path, char *dest_arg);

void client_pull_recursive_response_handler(EaselComm::EaselMessage *msg) {

    if (msg->dma_buf_size == 0) {
        fprintf(stderr, "ezlsh: %s: no file found\n", __FUNCTION__);
        client_recursive_done();
        return;
    } else if (msg->dma_buf_size == 1) {
        // Remote is a file not dir
        // Discard DMA transfer
        msg->dma_buf = nullptr;
        msg->dma_buf_size = 0;
        easel_comm_client.receiveDMA(msg);
        std::string mkdir = "mkdir -p " + std::string(dirname(file_recursive_path_local));
        system(mkdir.c_str());
        client_pull_file(file_recursive_path_remote, file_recursive_path_local);
        client_recursive_done();
        return;
    }

    char *files_buffer = (char *)malloc(msg->dma_buf_size);
    if (files_buffer == NULL) {
        fprintf(stderr, "ezlsh: %s: malloc failed", __FUNCTION__);
        // Discard DMA transfer
        msg->dma_buf = nullptr;
        msg->dma_buf_size = 0;
        easel_comm_client.receiveDMA(msg);
        client_recursive_done();
        return;
    }

    msg->dma_buf = files_buffer;
    int ret = easel_comm_client.receiveDMA(msg);

    if (ret) {
        fprintf(stderr, "ezlsh: %s: EaselComm receiveDMA failed (%d)",
                __FUNCTION__, ret);
        free(files_buffer);
        client_recursive_done();
        return;
    }

    // Pull files recursive.
    std::stringstream ss;
    ss << files_buffer;
    free(files_buffer);
    std::string file;
    while (std::getline(ss, file, '\n')) {
        std::string remote = std::string(file_recursive_path_remote) + kFileSeparator + file;
        std::string local = std::string(file_recursive_path_local) + kFileSeparator +
                            std::string(basename(file_recursive_path_remote)) +
                            kFileSeparator + file;
        fprintf(stderr, "Pulling %s as %s\n", file.c_str(), local.c_str());
        std::string mkdir = "mkdir -p " + std::string(dirname(local.c_str()));
        system(mkdir.c_str());
        client_pull_file(const_cast<char*>(remote.c_str()), const_cast<char*>(local.c_str()));
    }

    client_recursive_done();
}

void client_exec_response_handler(EaselComm::EaselMessage *msg) {
    ExecResponse *resp = (ExecResponse *)msg->message_buf;
    if (resp->done) {
        if (resp->exit != 0) {
            fprintf(stderr, "exit %d\n", resp->exit);
        }
        client_exec_done();
    } else {
        printf("%s", resp->output);
    }
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
                exec_cond.notify_one();
                break;

            case CMD_PUSH_RESPONSE:
                client_push_response_handler(&msg);
                break;

            case CMD_PULL_RESPONSE:
                client_pull_response_handler(&msg);
                break;

            case CMD_LS_RESPONSE:
                client_pull_recursive_response_handler(&msg);
                break;

            case CMD_EXEC_RESPONSE:
                client_exec_response_handler(&msg);
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

    ret = easel_comm_client.open(EASEL_SERVICE_SHELL);
    if (ret) {
        fprintf(stderr, "Failed to open client, service=%d, error=%d\n",
                EASEL_SERVICE_SHELL, ret);
    }

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

void client_exec_cmd(const char *cmd) {
    int ret = easel_comm_client.open(EASEL_SERVICE_SHELL);
    assert(ret == 0);
    easel_comm_client.flush();

    std::thread *msg_handler_thread;
    msg_handler_thread = new std::thread(client_message_handler);
    if (msg_handler_thread == nullptr) {
        fprintf(stderr, "failed to allocate thread for message handler\n");
        client_exit(1);
    }

    ExecRequest request;
    size_t size = strlcpy(request.cmd, cmd, kMaxTtyDataBufferSize);
    if (size == 0 || size >= kMaxTtyDataBufferSize) {
        fprintf(stderr, "ezlsh: %s invalid command %s\n", __FUNCTION__, cmd);
        client_exit(1);
    }

    request.h.datalen = size + 1;
    EaselComm::EaselMessage msg;
    msg.message_buf = &request;
    msg.message_buf_size = sizeof(MsgHeader) + request.h.datalen;
    msg.dma_buf = 0;
    msg.dma_buf_size = 0;

    std::unique_lock<std::mutex> lk(exec_lock);
    exec_done = false;
    easel_comm_client.sendMessage(&msg);
    exec_cond.wait(lk, [&]{return exec_done;});
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

    std::thread *msg_handler_thread;
    msg_handler_thread = new std::thread(client_message_handler);
    if (msg_handler_thread == nullptr) {
        fprintf(stderr, "failed to allocate thread for message handler\n");
        client_exit(1);
    }

    // Send request
    FilePullRequest pull_msg;
    size_t size = strlcpy(pull_msg.path, remote_path, PATH_MAX);
    pull_msg.h.datalen = size + 1;
    EaselComm::EaselMessage msg;
    msg.message_buf = &pull_msg;
    msg.message_buf_size = sizeof(MsgHeader) + pull_msg.h.datalen;
    msg.dma_buf = 0;
    msg.dma_buf_size = 0;

    // Wait for transfer done
    std::unique_lock<std::mutex> lk(file_xfer_lock);
    file_xfer_done = false;

    auto start = std::chrono::system_clock::now();
    easel_comm_client.sendMessage(&msg);
    file_xfer_cond.wait(lk, [&]{return file_xfer_done;});
    auto end = std::chrono::system_clock::now();
    long duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count();
    fprintf(stderr, "pull file %s (remote) to %s (local) in %lu ms\n",
            remote_path, file_xfer_path_local, duration);

    free(local_path_str);
}

int server_exec_cmd(std::string &cmd) {
    auto pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        ALOGE("%s: %s could not execute %s", "ezlsh", __FUNCTION__, cmd.c_str());
        return -1;
    }
    if (pclose(pipe) == -1) {
        ALOGE("%s: %s pclose returns an error after executing %s",
                "ezlsh", __FUNCTION__, cmd.c_str());
        return -1;
    }
    return 0;
}

static void list_dir_recursive(
        const std::string &root_path,
        const std::string &dir_path,
        std::stringstream &files) {
    DIR *dir;
    struct dirent *entry;
    dir = opendir((root_path + dir_path).c_str());
    if (dir == NULL) {
        return;
    } else {
        while ((entry = readdir(dir)) != NULL) {
            std::string entry_name(entry->d_name);
            if (entry_name != "." && entry_name != "..") {
                if (entry->d_type == DT_DIR) {
                    list_dir_recursive(root_path, dir_path + kFileSeparator + entry_name, files);
                } else if (entry->d_type == DT_REG) {
                    if (dir_path.empty()) {
                        files << entry_name + "\n";
                    } else {
                        files << dir_path + kFileSeparator + entry_name + "\n";
                    }
                }
            }
        }
    }
    closedir(dir);
}

int is_regular_file(const char *path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

void client_push_file_worker(char *local_path, char *remote_path) {
    file_xfer_path_remote = remote_path;
    file_xfer_path_local = local_path;

    easel_comm_client.flush();

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
    size_t size = strlcpy(push_msg.path, remote_path, PATH_MAX);
    push_msg.h.datalen = size + 1;
    push_msg.st_mode = stat_buf.st_mode;
    EaselComm::EaselMessage msg;
    msg.message_buf = &push_msg;
    msg.message_buf_size = sizeof(MsgHeader) + sizeof(mode_t) + push_msg.h.datalen;
    msg.dma_buf = file_data;
    msg.dma_buf_size = data_len;

    // Wait for transfer done
    std::unique_lock<std::mutex> lk(file_xfer_lock);
    file_xfer_done = false;
    auto start = std::chrono::system_clock::now();
    easel_comm_client.sendMessage(&msg);
    file_xfer_cond.wait(lk, [&]{return file_xfer_done;});
    auto end = std::chrono::system_clock::now();
    long duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count();
    fprintf(stderr, "push file %s (local) to %s (remote) in %lu ms\n",
            file_xfer_path_local, file_xfer_path_remote, duration);
}

// Client file push command processing. Send push request and wait for
// incoming message handler to process the response from server.
void client_push_file(char *local_path, char *remote_path) {
    int ret = easel_comm_client.open(EASEL_SERVICE_SHELL);
    if (ret) {
        fprintf(stderr, "Failed to open client, service=%d, error=%d\n",
                EASEL_SERVICE_SHELL, ret);
    }

    std::thread *msg_handler_thread;
    msg_handler_thread = new std::thread(client_message_handler);
    if (msg_handler_thread == nullptr) {
        fprintf(stderr, "failed to allocate thread for message handler\n");
        client_exit(1);
    }

    if (is_regular_file(local_path)) {
        client_push_file_worker(local_path, remote_path);
    } else {
        std::stringstream files;
        list_dir_recursive(std::string(local_path), "", files);
        std::string file;
        while (std::getline(files, file, '\n')) {
            std::string local_full_path = std::string(local_path) + kFileSeparator + file;
            std::string local = std::string(basename(local_path)) + kFileSeparator + file;
            std::string remote = std::string(remote_path) + kFileSeparator + local;
            client_push_file_worker(const_cast<char*>(local_full_path.c_str()),
                                    const_cast<char*>(remote.c_str()));
        }
    }
}

// Client file ls command processing. Send ls request and wait for
// incoming message handler to process the response from server.
void client_pull_recursive_file(char *remote_path, char *dest_arg) {
    file_recursive_path_remote = remote_path;
    char *local_path_str;

    if (dest_arg) {
        local_path_str = strdup(dest_arg);
    } else {
        local_path_str = strdup(".");
    }
    file_recursive_path_local = local_path_str;

    int ret = easel_comm_client.open(EASEL_SERVICE_SHELL);
    if (ret) {
        fprintf(stderr, "Failed to open client, service=%d, error=%d\n",
                EASEL_SERVICE_SHELL, ret);
    }

    easel_comm_client.flush();

    std::thread *msg_handler_thread;
    msg_handler_thread = new std::thread(client_message_handler);
    if (msg_handler_thread == nullptr) {
        fprintf(stderr, "failed to allocate thread for message handler\n");
        client_exit(1);
    }

    // Send request
    FileLsRequest ls_msg;
    size_t size = strlcpy(ls_msg.path, remote_path, PATH_MAX);
    ls_msg.h.datalen = size + 1;
    EaselComm::EaselMessage msg;
    msg.message_buf = &ls_msg;
    msg.message_buf_size = sizeof(MsgHeader) + ls_msg.h.datalen;
    msg.dma_buf = 0;
    msg.dma_buf_size = 0;

    // Wait for transfer done
    std::unique_lock<std::mutex> lk(file_recursive_lock);
    file_recursive_done = false;
    easel_comm_client.sendMessage(&msg);
    file_recursive_cond.wait(lk, [&]{return file_recursive_done;});

    free(local_path_str);
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

void server_open_shell() {
    shell_pid = forkpty(&tty_fd, NULL, NULL, NULL);
    if (shell_pid < 0) {
        perror("forkpty");
        exit(1);
    } else if (shell_pid == 0) {
        int ret = execle(SHELL_PATH, SHELL_PATH, "-", nullptr, environ);
        if (ret < 0) {
            perror(SHELL_PATH);
            exit(2);
        }
    }
}

void shell_server_session() {
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

    // Create directory if not exist
    std::string mkdir = "mkdir -p " + std::string(dirname(req->path));
    ret = server_exec_cmd(mkdir);
    if (ret) {
        return ret;
    }

    int fd = creat(req->path, req->st_mode);
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
    int ret_code = 0;

    if (fd >= 0) {
        if (fstat(fd, &stat_buf) == 0)
            file_size = stat_buf.st_size;
    }

    if (file_size < 0) {
        // Send errno as response code
        FilePullResponse pull_resp(errno);
        pull_resp.st_mode = stat_buf.st_mode;
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

    // Read file, send errno as response code if failed.
    // Send file data as DMA buffer if successful.
    ret_code = 0;
    char *file_data = (char *)malloc(data_max_len);
    if (!file_data) {
        ret_code = errno;
    } else {
        data_len = read(fd, file_data, data_max_len);
        if (data_len < 0) {
            ret_code = errno;
        }
    }
    // If dynamically-generated file larger than our max then send error
    if (!file_size && data_len == kDynamicMaxSize)
        ret_code = EFBIG;

    FilePullResponse pull_resp(ret_code);
    EaselComm::EaselMessage msg;
    msg.message_buf = &pull_resp;
    msg.message_buf_size = sizeof(FilePullResponse);
    msg.dma_buf = (ret_code || !data_len) ? NULL : file_data;
    msg.dma_buf_size = ret_code ? 0 : data_len;
    easel_comm_server.sendMessage(&msg);

    free(file_data);
    close(fd);
}

// Server receives file ls request, send file list back.
void server_ls_file(FileLsRequest *req) {
    FileLsResponse ls_resp;

    std::stringstream files;
    list_dir_recursive(std::string(req->path), "", files);
    std::string file_list = files.str();

    EaselComm::EaselMessage msg;
    msg.message_buf = &ls_resp;
    msg.message_buf_size = sizeof(MsgHeader) + ls_resp.h.datalen;
    msg.dma_buf = (void *)file_list.c_str();
    msg.dma_buf_size = file_list.length() + 1;
    int ret = easel_comm_server.sendMessage(&msg);
    if (ret != 0) {
        ALOGE("ezlsh: %s: failed to sendMessage (%d)\n",
                __FUNCTION__, ret);
    }
}

static int server_send_exec_response(const char *output, int size, bool done, int exit) {
    ExecResponse response;
    if (output != nullptr && size > 0) {
        size = strlcpy(response.output, output, kMaxTtyDataBufferSize);
        response.h.datalen += size + 1;
    }
    response.done = done;
    response.exit = exit;
    EaselComm::EaselMessage msg;
    msg.message_buf = &response;
    msg.message_buf_size = sizeof(response);
    msg.dma_buf = 0;
    msg.dma_buf_size = 0;
    int ret = easel_comm_server.sendMessage(&msg);
    if (ret != 0) {
        ALOGE("ezlsh: %s: failed to sendMessage (%d)\n",
                __FUNCTION__, ret);
    }
    return ret;
}

void server_exec_cmd(ExecRequest *request) {
    auto pipe = popen(request->cmd, "r");
    int ret = -1;
    if (!pipe) {
        ALOGE("ezlsh: %s could not execute cmd %s\n",
                __FUNCTION__, request->cmd);
        return;
    } else {
        char output[kMaxTtyDataBufferSize];
        while ((ret = read(fileno(pipe), output, kMaxTtyDataBufferSize)) > 0) {
            // read contains null terminator: ret == strlen(output) + 1
            if (ret > kMaxTtyDataBufferSize) {
                ALOGE("ezlsh: %s output too long (%d)",
                        __FUNCTION__, ret);
                ret = kMaxTtyDataBufferSize;
            }
            server_send_exec_response(output, ret, false, 0);
        }
        ret = pclose(pipe);
    }
    server_send_exec_response(nullptr, 0, true, ret);
}

void server_run(bool flush) {
    int ret;

    ret = easel_comm_server.open(EASEL_SERVICE_SHELL);
    if (ret) {
        ALOGE("Failed to open server, service=%d, error=%d\n",
                EASEL_SERVICE_SHELL, ret);
    }

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
                ALOGE("ERROR: receiveMessage returns %d\n", ret);
            }
            continue;
        }

        if (msg.message_buf_size) {
            MsgHeader *h = (MsgHeader *)msg.message_buf;
            switch(h->command) {
            case CMD_OPEN_SHELL:
                server_kill_shell();
                server_open_shell();
                shell_session_thread =
                  new std::thread(shell_server_session);
                if (shell_session_thread == nullptr)
                    ALOGE("failed to allocate thread for shell session\n");
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

            case CMD_LS_REQUEST:
                server_ls_file((FileLsRequest *)msg.message_buf);
                break;

            case CMD_EXEC_REQUEST:
                server_exec_cmd((ExecRequest *)msg.message_buf);
                break;

            default:
                ALOGE("ERROR: unrecognized command %d\n",
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
                    "       client: ezlsh poweron\n"
                    "       client: ezlsh poweroff\n"
                    "       client: ezlsh pull <remote-path> [<local-path>]\n"
                    "       client: ezlsh push <local-path> <remote-path>\n"
                    "       cliect: ezlsh exec \"<cmd>\",\n"
                    "               to catch stderr, please append \"2>&1\" after cmd.\n"
                    );
            exit(1);
        }
    }

    if (client) {
        EaselLog::LogClient logClient;
        if (optind < argc) {
            if (!strcmp(argv[optind], "poweron")) {
                if (!client_check_state(POWER_OFF)) exit(1);
                write_sysfs_node(kStageFw, 1);
                read_sysfs_node(kPowerOn);
            } else if (!strcmp(argv[optind], "poweroff")) {
                if (!client_check_state(POWER_ON)) exit(1);
                read_sysfs_node(kPowerOff);
            } else if (!strcmp(argv[optind], "pull")) {
                if (!client_check_state(POWER_ON)) exit(1);
                logClient.start();

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
                client_pull_recursive_file(remote_path, local_path);
            } else if (!strcmp(argv[optind], "push")) {
                if (!client_check_state(POWER_ON)) exit(1);
                logClient.start();

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
            } else if (!strcmp(argv[optind], "exec")) {
                if (!client_check_state(POWER_ON)) exit(1);
                logClient.start();

                if (++optind >= argc) {
                    fprintf(stderr,
                          "ezlsh: exec: cmd missing\n");
                    exit(1);
                }

                client_exec_cmd(argv[optind]);
            } else {
                fprintf(stderr,
                        "ezlsh: unknown command \"%s\"\n", argv[optind]);
                exit(1);
            }
        } else {
            if (!client_check_state(POWER_ON)) exit(1);
            logClient.start();
            // No command, run shell session
            shell_client_session();
        }
        logClient.stop();
    } else {
        server_run(server_needs_flush);
    }
}
