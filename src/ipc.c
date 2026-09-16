#include <linux/limits.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <cwalk.h>
#include <yyjson.h>
#include "common.h"
#include "err.h"
#include "events.h"
#include "subprojects/yyjson/yyjson.h"
#include "ipc.h"

typedef enum ReplyCodes {
    WK_IPC_REPLY_OK,
    WK_IPC_REPLY_INVALID_DATA,
    WK_IPC_REPLY_UNKNOWN_COMMAND,
} ReplyCodes;

typedef struct WkIPCReply{
    char message[128];
    ReplyCodes reply_code;
} WkIPCReply;

static WkResult
get_socket_path(char *sock_path, size_t max_len)
{
    char path[PATH_MAX];
    const char *run_time_dir = getenv("XDG_RUNTIME_DIR");
    if(run_time_dir){
        size_t len = cwk_path_join(run_time_dir, "wallkan.sock", path, PATH_MAX);
        if(len < max_len){
            strcpy(sock_path, path);
            return WK_OK;
        }
    }

    snprintf(sock_path, max_len, "/tmp/wallkan-%ju.sock", (uintmax_t)getuid());
    return WK_OK;
}

static WkResult
wk_ipc_ping(struct sockaddr_un *addr)
{
    int ping_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(ping_fd < 0){
        return WK_ERR(WK_ERR_IPC_SOCKET_CREATION_FAILED, "Failed to create a temporary socket!");
    }
    int sock_stat = connect(ping_fd, (const struct sockaddr *)addr, sizeof(*addr));
    if(sock_stat == 0){
        close(ping_fd);
        return WK_ERR(WK_ERR_IPC_ALREADY_RUNNING,
            "Another instance of wallkan is already running!");
    }
    if(sock_stat < 0){
        close(ping_fd);
        unlink(addr->sun_path);
    }

    return WK_OK;
}

static WkResult
setup_server_sock(WallkanIpc *wk_ipc, struct sockaddr_un *addr)
{
    wk_ipc->server_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if(wk_ipc->server_fd < 0){
        return WK_ERR(WK_ERR_IPC_SOCKET_CREATION_FAILED, "Failed to create a server socket!");
    }
    if(bind(wk_ipc->server_fd, (struct sockaddr *)addr, sizeof(*addr)) < 0){
        return WK_ERR(WK_ERR_IPC_SOCKET_BIND_FAILED, "Failed to bind the server socket!");
    }
    if(listen(wk_ipc->server_fd, 16) < 0){
        return WK_ERR(WK_ERR_IPC_SOCKET_LISTEN_FAILED, "Failed to bind the server socket!");
    }
    return WK_OK;
}

static WkResult
wk_ipc_parse_commands(WallkanIpc *wk_ipc, yyjson_doc *doc, WkIPCReply *out_reply)
{
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        out_reply->reply_code = WK_IPC_REPLY_INVALID_DATA;
        strncpy(out_reply->message, "Invalid json format!", sizeof(out_reply->message));
        return WK_OK;
    }
    const char *cmd = yyjson_get_str(yyjson_obj_get(root, "cmd"));
    if(!cmd){
        out_reply->reply_code = WK_IPC_REPLY_INVALID_DATA;
        strncpy(out_reply->message, "Invalid json format, 'cmd' is missing!",
            sizeof(out_reply->message));
        return WK_OK;
    }
    if(strcmp(cmd, "quit") == 0){
        wk_ev_handler_emit(wk_ipc->wk_ev_handler, &(WkEvent){.type = WK_EVENT_CLOSE});
    }else{
        out_reply->reply_code = WK_IPC_REPLY_UNKNOWN_COMMAND;
        strncpy(out_reply->message, "Unknown command", sizeof(out_reply->message));
    }
    return WK_OK;
}


static WkResult
wk_ipc_reply(WallkanIpc *wk_ipc, const WkIPCReply *reply, int client_fd)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&wk_ipc->cmd_processor.json_reply_alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    if(reply->reply_code == WK_IPC_REPLY_OK){
        yyjson_mut_obj_add_str(doc, root, "status", "ok");
    }else {
        yyjson_mut_obj_add_str(doc, root, "status", "error");
        if (reply->message[0] != '\0') {
            yyjson_mut_obj_add_str(doc, root, "message", reply->message);
        }
    }
    size_t json_len = 0;
    char *json_str =
        yyjson_mut_write_opts(doc, 0, &wk_ipc->cmd_processor.json_reply_alc, &json_len, NULL);
    if (json_str && json_len > 0) {
        write(client_fd, json_str, json_len);
        write(client_fd, "\n", 1);
    }
    yyjson_mut_doc_free(doc);
    return WK_OK;
}

WkResult
wk_ipc_handle_connection(WallkanIpc *wk_ipc)
{
    char buf[MAX_IPC_BUFFER_SIZE];
    int client_fd = accept4(wk_ipc->server_fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
    yyjson_doc *doc = NULL;
    WkResult wkres = WK_OK;
    if(client_fd < 0){
        if(errno != EAGAIN && errno != EWOULDBLOCK){
            wkres = WK_ERR(WK_ERR_IPC_SOCKET_ACCEPT_FAILURE,
                "Failed to accept connection, errno: %s", strerror(errno));
        };
        goto cleanup;
    }

    ssize_t bytes_read = read(client_fd, buf, sizeof(buf) - 1);
    if(bytes_read < 0){
        if(errno != EAGAIN && errno != EWOULDBLOCK){
            wkres = WK_ERR(WK_ERR_IPC_SOCKET_READ_FAILURE,
                "Failed to read from client, errno: %s", strerror(errno));
        }
        goto cleanup;
    }

    if(bytes_read == 0){
        goto cleanup;
    };
    // 'Post-process' the command
    if(buf[bytes_read - 1] == '\n'){
        buf[bytes_read - 1] = '\0';
        bytes_read--;
    }else{
        buf[bytes_read] = '\0';
    }
    doc = yyjson_read_opts(buf, bytes_read, 0, &wk_ipc->cmd_processor.json_parse_alc, NULL);
    WkIPCReply reply = {0};
    LOG("wk_ipc_handle_connection: Recieved: %s", buf);
    if(wk_ipc_parse_commands(wk_ipc, doc, &reply) != WK_OK) goto cleanup;
    if(wk_ipc_reply(wk_ipc, &reply, client_fd) != WK_OK) goto cleanup;

cleanup:
    if(client_fd >= 0) close(client_fd);
    yyjson_doc_free(doc);
    return wkres;
}

static WkResult
wk_ipc_init_cmd_processor(WallkanIpc *wk_ipc)
{
    wk_ipc->cmd_processor.json_pool_size = yyjson_read_max_memory_usage(MAX_IPC_BUFFER_SIZE, 0);
    wk_ipc->cmd_processor.json_pool_size = (wk_ipc->cmd_processor.json_pool_size + 15) & ~15;

    wk_ipc->cmd_processor.json_mem = malloc(wk_ipc->cmd_processor.json_pool_size * 2);
    if(!wk_ipc->cmd_processor.json_mem){
        return WK_ERR(WK_ERR_ALLOCATION_ERROR, "Failed to malloc!");
    }
    if(!yyjson_alc_pool_init(&wk_ipc->cmd_processor.json_parse_alc, wk_ipc->cmd_processor.json_mem,
        wk_ipc->cmd_processor.json_pool_size)){
            return WK_ERR(WK_ERR_ALLOCATION_ERROR, "Failed to init yyjson pool!");
    }
    void *next_block = (uint8_t*)wk_ipc->cmd_processor.json_mem + wk_ipc->cmd_processor.json_pool_size;
    if(!yyjson_alc_pool_init(&wk_ipc->cmd_processor.json_reply_alc,next_block,
        wk_ipc->cmd_processor.json_pool_size)){
            return WK_ERR(WK_ERR_ALLOCATION_ERROR, "Failed to init yyjson pool!");
    }
    return WK_OK;
}

WkResult
wk_ipc_init(WallkanIpc *wk_ipc, WallkanEventHandler *wk_ev_handler)
{
    wk_ipc->server_fd = -1;
    wk_ipc->wk_ev_handler = wk_ev_handler;
    struct sockaddr_un addr = {
        .sun_family = AF_UNIX
    };
    WK_TRY(get_socket_path(addr.sun_path, sizeof(addr.sun_path)));
    memcpy(wk_ipc->socket_path, addr.sun_path, sizeof(addr.sun_path));
    WK_TRY(wk_ipc_ping(&addr));
    WK_TRY(setup_server_sock(wk_ipc, &addr));
    WK_TRY(wk_ipc_init_cmd_processor(wk_ipc));
    return WK_OK;
}

void
wk_ipc_cleanup(WallkanIpc *wk_ipc)
{
    if(wk_ipc->server_fd >= 0){
        close(wk_ipc->server_fd);
        wk_ipc->server_fd = -1;
    }
    if(wk_ipc->socket_path[0] != '\0'){
        unlink(wk_ipc->socket_path);
        wk_ipc->socket_path[0] = '\0';
    }
    if(wk_ipc->cmd_processor.json_mem){
        free(wk_ipc->cmd_processor.json_mem);
        wk_ipc->cmd_processor.json_mem = NULL;
    }
}
