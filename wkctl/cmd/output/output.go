package output

import "github.com/spf13/cobra"

var OutputCmd = &cobra.Command{
	Use:   "output <action>",
	Short: "Options to interact with multi monitor setup",
}

func init() {
	OutputCmd.AddCommand(listCmd)
	OutputCmd.AddCommand(enableCmd)
	OutputCmd.AddCommand(disableCmd)
}
