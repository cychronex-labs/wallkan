#include <limits.h>
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
#include "ipc_cmd/ipc_cmd.h"


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
        return WK_ERR(WK_ERR_IPC_SOCKET_LISTEN_FAILED, "Failed to listen on the server socket!");
    }
    return WK_OK;
}

bool
wk_ipc_reply_pending(WallkanIpc *wk_ipc, uint32_t client_idx)
{
    uint8_t client_mask = (1 << client_idx);
    return (wk_ipc->reply_is_due_bits & client_mask) &&
        (wk_ipc->active_client_bits & client_mask) &&
        wk_ipc->client_fd[client_idx] != -1;
}

static yyjson_mut_doc*
build_default_reply(WallkanIpc *wk_ipc, const WkIPCReply *reply)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&wk_ipc->cmd_processor.json_reply_alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    if(reply->reply_code == WK_IPC_REPLY_OK){
        yyjson_mut_obj_add_str(doc, root, "status", "ok");
        yyjson_mut_obj_add_str(doc, root, "message", reply->message);
    }else {
        yyjson_mut_obj_add_str(doc, root, "status", "error");
        if (reply->message[0] != '\0') {
            yyjson_mut_obj_add_str(doc, root, "message", reply->message);
        }
    }
    return doc;
}

// Will free the doc!
static WkResult
wk_ipc_reply_blind(WallkanIpc *wk_ipc, uint32_t client_idx, const WkIPCReply *reply)
{
    yyjson_mut_doc *doc;
    if(reply->response_doc)
        doc = reply->response_doc;
    else
        doc = build_default_reply(wk_ipc, reply);

    size_t json_len = 0;
    char *json_str =
        yyjson_mut_write_opts(doc, 0, &wk_ipc->cmd_processor.json_reply_alc, &json_len, NULL);
    if (json_str && json_len > 0) {
        send(wk_ipc->client_fd[client_idx], json_str, json_len, MSG_NOSIGNAL);
        send(wk_ipc->client_fd[client_idx], "\n", 1, MSG_NOSIGNAL);
    }
    yyjson_mut_doc_free(doc);
    return WK_OK;
}

WkResult
wk_ipc_reply(WallkanIpc *wk_ipc, uint32_t client_idx, const WkIPCReply *reply)
{
    if(!wk_ipc_reply_pending(wk_ipc, client_idx)){
        WARN("wk_ipc_reply: Reply too late. Connection lost!");
        wk_ipc_disconnect_client(wk_ipc, client_idx);
        return WK_OK;
    }
    wk_ipc->reply_is_due_bits &= ~(1 << client_idx);
    WK_TRY(wk_ipc_reply_blind(wk_ipc, client_idx, reply));
    return WK_OK;
}

WkResult
wk_ipc_accept_connection(Wallkan *wk, uint32_t *out_client_idx)
{
    WkResult wkres = WK_OK;
    uint32_t client_idx = 0;
    uint8_t inactive_slots = ~(wk->ipc.active_client_bits);
    if(inactive_slots == 0){
        WARN("wk_ipc_accept_connection: Max amount of clients reached!");
        // accept connection so POLLIN is consumed
        int rejected_fd = accept4(wk->ipc.server_fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (rejected_fd >= 0) {
            close(rejected_fd);
        }
        return WK_OK;
    }else{
        // Find first inactive slot
        client_idx = __builtin_ctz(inactive_slots);
    }
    int client_fd = accept4(wk->ipc.server_fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);

    if(client_fd < 0){
        if(errno == EAGAIN || errno == EWOULDBLOCK){
            return WK_OK;
        };
        wkres = WK_ERR(WK_ERR_IPC_SOCKET_ACCEPT_FAILURE,
            "Failed to accept connection, errno: %s", strerror(errno));
        goto err;
    }

    wk->ipc.client_fd[client_idx] = client_fd;
    wk->ipc.active_client_bits |= (1 << client_idx);
    *out_client_idx = client_idx;
    return WK_OK;
err:
    wk_ipc_disconnect_client(&wk->ipc, client_idx);
    return wkres;
}

WkResult
wk_ipc_read_client(ArenaAllocator *alloc, Wallkan *wk, uint32_t client_idx)
{
    WkResult wkres = WK_OK;
    yyjson_doc *doc = NULL;
    char buf[MAX_IPC_BUFFER_SIZE];
    ssize_t bytes_read = read(wk->ipc.client_fd[client_idx], buf, sizeof(buf) - 1);
    if (bytes_read == 0) {
        wk_ipc_disconnect_client(&wk->ipc, client_idx);
        return WK_OK;
    }
    if(bytes_read <= 0){
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return WK_OK;
        }
        wkres = WK_ERR(WK_ERR_IPC_SOCKET_READ_FAILURE,
            "Failed to read from client, errno: %s", strerror(errno));
        goto err;
    }
    // Remove newline delimiter if exists
    if(buf[bytes_read - 1] == '\n'){
        buf[bytes_read - 1] = '\0';
        bytes_read--;
    }else{
        buf[bytes_read] = '\0';
    }
    doc = yyjson_read_opts(buf, bytes_read, 0, &wk->ipc.cmd_processor.json_parse_alc, NULL);
    LOG("wk_ipc_read_client: Recieved: %s", buf);
    if(ipc_cmd_handle(alloc, doc, wk, client_idx) != WK_OK) goto err;
    goto cleanup;
err:
    wk_ipc_disconnect_client(&wk->ipc, client_idx);
cleanup:
    yyjson_doc_free(doc);
    return wkres;
}

void
wk_ipc_disconnect_client(WallkanIpc *wk_ipc, uint32_t client_idx)
{
    if(wk_ipc->client_fd[client_idx] > -1){
        if (wk_ipc->reply_is_due_bits & (1 << client_idx)) {
            wk_ipc_reply_blind(wk_ipc, client_idx, &(const WkIPCReply){
                .reply_code = WK_IPC_REPLY_INTERNAL_ERROR,
                .message = "Internal error occurred! command unhandled or reply dropped",
            });
        }
        close(wk_ipc->client_fd[client_idx]);
        LOG("wk_ipc_disconnect_client: Client, fd: %d disconnected!", wk_ipc->client_fd[client_idx]);
    }
    wk_ipc->client_fd[client_idx] = -1;
    wk_ipc->active_client_bits &= ~(1 << client_idx);
    wk_ipc->reply_is_due_bits &= ~(1 << client_idx);
}

static WkResult
wk_ipc_init_cmd_processor(WallkanIpc *wk_ipc)
{
    wk_ipc->cmd_processor.json_pool_size = yyjson_read_max_memory_usage(MAX_IPC_BUFFER_SIZE, 0);
    wk_ipc->cmd_processor.json_pool_size = (wk_ipc->cmd_processor.json_pool_size + 15) & ~15;

    wk_ipc->cmd_processor.json_mem = malloc(wk_ipc->cmd_processor.json_pool_size * 2);
    if(!wk_ipc->cmd_processor.json_mem){
        return WK_ERR(WK_ERR_ALLOCATION_FAILURE, "Failed to malloc!");
    }
    if(!yyjson_alc_pool_init(&wk_ipc->cmd_processor.json_parse_alc, wk_ipc->cmd_processor.json_mem,
        wk_ipc->cmd_processor.json_pool_size)){
            return WK_ERR(WK_ERR_ALLOCATION_FAILURE, "Failed to init yyjson pool!");
    }
    void *next_block = (uint8_t*)wk_ipc->cmd_processor.json_mem + wk_ipc->cmd_processor.json_pool_size;
    if(!yyjson_alc_pool_init(&wk_ipc->cmd_processor.json_reply_alc,next_block,
        wk_ipc->cmd_processor.json_pool_size)){
            return WK_ERR(WK_ERR_ALLOCATION_FAILURE, "Failed to init yyjson pool!");
    }
    return WK_OK;
}

WkResult
wk_ipc_init(WallkanIpc *wk_ipc, WallkanEventHandler *wk_ev_handler)
{
    wk_ipc->server_fd = -1;
    for (uint32_t i=0; i<MAX_IPC_CLIENTS; i++) {
        wk_ipc->client_fd[i] = -1;
    }
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
    for (uint32_t i=0; i<MAX_IPC_CLIENTS; i++) {
        wk_ipc_disconnect_client(wk_ipc, i);
        wk_ipc->client_fd[i] = -1;
    }
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
