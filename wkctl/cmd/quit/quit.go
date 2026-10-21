package quit

import (
	"encoding/json"
	"wkctl/internal"
	"wkctl/internal/ipc"

	"github.com/spf13/cobra"
)

var QuitCmd = &cobra.Command{
	Use:   "quit",
	Short: "Close the wallkan daemon",
	PreRun: func(cmd *cobra.Command, args []string) {
		cmd.SilenceUsage = true
	},
	SilenceErrors: true,
	RunE: func(cmd *cobra.Command, args []string) error {
		return Quit()
	},
}

func Quit() error {
	resp, err := ipc.OneshotSend[map[string]any]("quit", map[string]string{})
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
