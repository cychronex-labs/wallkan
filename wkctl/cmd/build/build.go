package build

import (
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"wkctl/internal"
	"wkctl/internal/shader_manifest"
	"wkctl/internal/wksp"

	"github.com/spf13/cobra"
)

var BuildCmd = &cobra.Command{
	Use:           "build <folder>",
	Short:         "Build .wksp from shader directory",
	Long:          "Build the shader folder containing a manifest.toml and Produce .wksm file at build/",
	Args:          cobra.ExactArgs(1),
	SilenceErrors: true,
	PreRunE: func(cmd *cobra.Command, args []string) error {
		cmd.SilenceUsage = true
		sb := InitShaderBuilder(args[0])
		return sb.ValidateShaderDir()
	},
	RunE: func(cmd *cobra.Command, args []string) error {
		sb := InitShaderBuilder(args[0])
		_, err := sb.Build()
		return err
	},
}

type ShaderBuilder struct {
	path string
}

func InitShaderBuilder(path string) ShaderBuilder {
	return ShaderBuilder{
		path: path,
	}
}

func (sb *ShaderBuilder) ValidateShaderDir() error {
	info, err := os.Stat(sb.path)
	if err != nil {
		if errors.Is(err, os.ErrNotExist) {
			internal.Error("Directory does not exist: %q", sb.path)
			return err
		}
		internal.Error("Cannot access path %q: %s", sb.path, err.Error())
		return err
	}

	if !info.IsDir() {
		internal.Error("Path is a file, not a directory: %q", sb.path)
		return fmt.Errorf("Path %q is not a directory", sb.path)
	}

	manifest := filepath.Join(sb.path, "manifest.toml")
	manifestInfo, err := os.Stat(manifest)
	if err != nil {
		if errors.Is(err, os.ErrNotExist) {
			internal.Error("Invalid shader folder: %q is missing manifest.toml", sb.path)
			return err
		}
		internal.Error("Cannot access %q: %s", manifest, err.Error())
		return err
	}
	if manifestInfo.IsDir() {
		internal.Error("Expected %q to be a file, but found a directory", manifest)
		return fmt.Errorf("path %q is not a directory", sb.path)
	}

	return nil
}

func (sb *ShaderBuilder) Build() (string, error) {
	wksp_container := wksp.WKSPContainer{}
	manifest, err := shader_manifest.New(filepath.Join(sb.path, "manifest.toml"))
	if err != nil {
		return "", err
	}
	manifest.ShaderPath = sb.path
	if err := manifest.Validate(); err != nil {
		return "", internal.Error("Validation failed!")
	}
	// Setup build directory
	builddir := filepath.Join(sb.path, "build")

	err = os.Mkdir(builddir, 0755)
	if err != nil && !errors.Is(err, os.ErrExist) {
		return "", internal.Error("Failed to create build directory at: %s (%s)", builddir, err.Error())
	}

	metadata_file := filepath.Join(sb.path, "build", "metadata.json")
	file, err := os.Create(metadata_file)
	if err != nil {
		internal.Warn("Failed to create metadata.json at %s", metadata_file)
	}

	// Make the metadata
	wksp.Init(&wksp_container, manifest)

	metadata_bytes, err := json.Marshal(wksp_container.Metadata)
	if err != nil {
		return "", internal.Error("Failed to build wksp file!")
	}

	size, err := file.Write(metadata_bytes)
	if err != nil {
		return "", internal.Error("Failed to build wksp file!")
	}
	wksp_container.MetadataSize = size

	for i := range manifest.ShaderPasses {
		glsl_source := filepath.Join(sb.path, manifest.ShaderPasses[i].Source)
		wksp_container.AddGLSL(glsl_source, i, builddir, manifest)
	}
	err = wksp_container.CompileAll(builddir, manifest)
	if err != nil {
		return "", err
	}
	return wksp_container.Output(builddir, metadata_bytes)
}
