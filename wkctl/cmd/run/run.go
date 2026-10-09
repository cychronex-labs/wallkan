package run

import (
	"path/filepath"
	"wkctl/cmd/build"
	"wkctl/internal"
	"wkctl/internal/ipc"

	"github.com/spf13/cobra"
)

var RunCmd = &cobra.Command{
	Use:           "run <folder>",
	Short:         "Run .wksp from shader directory",
	Long:          "Build & load a shader directory containing manifest.toml and glsl files",
	Args:          cobra.ExactArgs(1),
	SilenceErrors: true,
	PreRunE: func(cmd *cobra.Command, args []string) error {
		cmd.SilenceUsage = true
		wk_ipc := ipc.New()
		err := wk_ipc.Connect()
		if err != nil {
			internal.Error("If you just want to build to get .wksp file use build command instead")
			return err
		}
		sb := build.InitShaderBuilder(args[0])
		return sb.ValidateShaderDir()
	},
	RunE: func(cmd *cobra.Command, args []string) error {
		sb := build.InitShaderBuilder(args[0])
		filepath, err := sb.Build()
		if err != nil {
			return err
		}
		Run(filepath)
		return nil
	},
}

type result struct {
}

func Run(path string) error {
	path, err := filepath.Abs(path)
	if err != nil {
		return internal.Error("Failed to get absolute path of %s!", path)
	}
	_, err = ipc.OneshotSend[result]("scene.load_wksp", map[string]string{
		"path": path,
	})
	if err != nil {
		internal.Success("Success, ")
	} else {
		internal.Error("%s", err.Error())
	}
	return err
}
