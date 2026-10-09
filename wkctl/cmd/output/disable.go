package output

import "github.com/spf13/cobra"

var disableCmd = &cobra.Command{
	Use:   "disable <target>",
	Short: "Disable an output by name or index",
	Long:  "Disable an output by name or index, index is retrieved using list command and its not dependable",
}

func DisableOutput() {

}
