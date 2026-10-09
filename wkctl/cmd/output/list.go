package output

import (
	"encoding/json"
	"wkctl/internal"
	"wkctl/internal/ipc"

	"github.com/spf13/cobra"
)

var listCmd = &cobra.Command{
	Use:   "list",
	Short: "List all available monitors",
	PreRun: func(cmd *cobra.Command, args []string) {
		cmd.SilenceUsage = true
	},
	SilenceErrors: true,
	RunE: func(cmd *cobra.Command, args []string) error {
		return ListOutputs()
	},
}

func ListOutputs() error {
	resp, err := ipc.OneshotSend[map[string]any]("output.list", map[string]string{})
	if err != nil {
		return err
	}
	resp_bytes, err := json.MarshalIndent(resp, "", "    ")
	if err != nil {
		return err
	}
	internal.Success("%s", string(resp_bytes))
	return nil
}
