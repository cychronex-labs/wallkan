package cmd

import (
	"wkctl/cmd/build"
	"wkctl/cmd/output"
	"wkctl/cmd/quit"
	"wkctl/cmd/run"

	"github.com/spf13/cobra"
)

var rootCmd = &cobra.Command{
	Use:   "wkctl",
	Short: "Wallkan renderer CLI",
	Long:  "wkctl compiles, packages, and controls the behaviour of the Wallkan daemon.",
}

func init() {
	rootCmd.AddCommand(build.BuildCmd)
	rootCmd.AddCommand(run.RunCmd)
	rootCmd.AddCommand(output.OutputCmd)
	rootCmd.AddCommand(quit.QuitCmd)
}

func Execute() error {
	if err := rootCmd.Execute(); err != nil {
		return err
	}
	return nil
}
