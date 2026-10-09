package ipc

import (
	"bufio"
	"encoding/json"
	"fmt"
	"net"
	"os"
	"path/filepath"
	"wkctl/internal"
)

type WallkanIPC struct {
	Path   string
	sock   net.Conn
	reader *bufio.Reader
}

func getSocketPath() string {
	path := os.Getenv("XDG_RUNTIME_DIR")
	if path != "" {
		return filepath.Join(path, "wallkan.sock")
	}
	return fmt.Sprintf("/tmp/wallkan-%d.sock", os.Getuid())
}

func New() WallkanIPC {
	return WallkanIPC{
		Path: getSocketPath(),
	}
}

func (ipc *WallkanIPC) Connect() error {
	sock, err := net.Dial("unix", ipc.Path)
	if err != nil {
		return internal.Error("Failed to connect to socket. Is wallkan running yet?")
	}
	ipc.sock = sock
	ipc.reader = bufio.NewReader(sock)
	return nil
}

func (ipc *WallkanIPC) SendCmd(cmd string, args map[string]string) error {
	args["cmd"] = cmd
	bytes, err := json.Marshal(args)
	if err != nil {
		return internal.Error("Failed to marshal json!")
	}
	_, err = ipc.sock.Write(append(bytes, '\n'))
	if err != nil {
		return internal.Error("Failed to send command!")
	}
	return nil
}

type ResponseError struct {
	Code    string `json:"code"`
	Message string `json:"message"`
}

type rawResponse[T any] struct {
	Status int            `json:"status"`
	Result T              `json:"result"`
	Error  *ResponseError `json:"error"`
}

func (e *ResponseError) Error() string {
	return fmt.Sprintf("Wallkan daemoned returned error (code: %s): %s", e.Code, e.Message)
}

func (ipc *WallkanIPC) Response[ResultObj any]() (*ResultObj, error) {
	cmd, err := ipc.reader.ReadBytes('\n')
	if err != nil {
		return nil, internal.Error("Failed to read response!")
	}

	resp := rawResponse[ResultObj]{}
	if err := json.Unmarshal(cmd, &resp); err != nil {
		return nil, internal.Error("Failed to unmarshal json, err: %s", err.Error())
	}

	if err := json.Unmarshal(cmd, &resp); err != nil {
		return nil, err
	}

	if resp.Status != 0 {
		return nil, resp.Error
	}
	return &resp.Result, nil
}

func (ipc *WallkanIPC) Close() {
	if ipc.sock != nil {
		ipc.sock.Close()
	}
}

func OneshotSend[ResultObj any](cmd string, args map[string]string) (*ResultObj, error) {
	wk_ipc := New()
	err := wk_ipc.Connect()
	if err != nil {
		return nil, err
	}
	defer wk_ipc.Close()
	err = wk_ipc.SendCmd(cmd, args)
	if err != nil {
		return nil, err
	}

	resp, err := wk_ipc.Response[ResultObj]()
	if err != nil {
		return nil, err
	}
	return resp, nil
}
