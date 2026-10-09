package output

import "github.com/spf13/cobra"

var enableCmd = &cobra.Command{
	Use:   "enable <target>",
	Short: "Enable an output by name or index",
	Long:  "Enable an output by name or index, index is retrieved using list command and its not dependable",
}
