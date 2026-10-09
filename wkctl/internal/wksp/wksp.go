package wksp

import (
	"encoding/binary"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"
	"wkctl/internal"
	"wkctl/internal/shader_manifest"
	"wkctl/internal/wksp/meta"

	"golang.org/x/sync/errgroup"
)

type WKSPContainer struct {
	MetadataSize int
	Metadata     meta.WKSPManifest
	PassCount    int
	SPIRV_files  []string
	GLSL_files   []string
}

func Init(container *WKSPContainer, manifest *shader_manifest.Manifest) {
	container.Metadata = *meta.Build(manifest)
	container.PassCount = len(manifest.ShaderPasses)
}

func (container *WKSPContainer) AddGLSL(glsl_source string, passIndex int, builddir string,
	manifest *shader_manifest.Manifest) error {

	preprocessed_glsl_f := filepath.Join(builddir, fmt.Sprintf("processed_pass_%d.glsl", passIndex))
	if _, err := Preprocess(glsl_source, preprocessed_glsl_f, manifest, passIndex); err != nil {
		return err
	}
	compiled_filename := filepath.Join(builddir, fmt.Sprintf("%d.spv", passIndex))
	container.SPIRV_files = append(container.SPIRV_files, compiled_filename)
	container.GLSL_files = append(container.GLSL_files, preprocessed_glsl_f)
	return nil
}

func (container *WKSPContainer) CompileAll(builddir string, manifest *shader_manifest.Manifest) error {
	var errGroup errgroup.Group
	internal.Log("Compiling all shaders...")
	container.SPIRV_files = append(container.SPIRV_files, filepath.Join(builddir, "vertex_shader.spv"))
	for i := range container.GLSL_files {
		errGroup.Go(func() error {
			if err := Compile(container.GLSL_files[i], container.SPIRV_files[i],
				manifest.ShaderPasses[i].Type); err != nil {
				return err
			}
			return nil
		})
	}
	errGroup.Go(func() error {
		if _, err := vertexCompile(builddir); err != nil {
			return err
		}
		return nil
	})
	if err := errGroup.Wait(); err != nil {
		return err
	}
	return nil
}

func vertexCompile(builddir string) (string, error) {
	vertex_shader_file := filepath.Join(builddir, "vertex_shader.glsl")
	out_file := filepath.Join(builddir, "vertex_shader.spv")
	file, err := os.OpenFile(vertex_shader_file, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, 0755)
	if err != nil {
		return "", internal.Error("Failed to open file: %s (%s)", vertex_shader_file, err.Error())
	}
	defer file.Close()
	file.WriteString(WkVertexShader)
	if err := Compile(vertex_shader_file, out_file, "vertex"); err != nil {
		return "", err
	}
	return out_file, nil
}

func (container *WKSPContainer) Output(builddir string, metadata []byte) (string, error) {
	output_filename := filepath.Join(builddir, strings.ReplaceAll(strings.ToLower(container.Metadata.Meta.Name), " ", "_")+".wksp")

	file, err := os.OpenFile(output_filename, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0755)
	if err != nil {
		return "", internal.Error("Failed to open file: %s (%s)", output_filename, err.Error())
	}
	// 4 byte for header, 4 byte represents the size of json
	json_size := len(metadata)
	buf := []byte{}
	buf = binary.LittleEndian.AppendUint32(buf, binary.LittleEndian.Uint32([]byte("WKSP")))
	buf = binary.LittleEndian.AppendUint32(buf, uint32(json_size))
	buf = append(buf, metadata...)
	padding := ((json_size + 3) &^ 3) - json_size
	for _ = range padding {
		buf = append(buf, ' ')
	}

	buf = binary.LittleEndian.AppendUint32(buf, uint32(len(container.SPIRV_files)))
	for i := range container.SPIRV_files {
		filename := container.SPIRV_files[i]
		file, err := os.Open(filename)
		if err != nil {
			return "", internal.Error("Failed to open file: %s (%s)", output_filename, err.Error())
		}
		spirv_data, err := io.ReadAll(file)
		if err != nil {
			return "", internal.Error("Failed to read file: %s (%s)", output_filename, err.Error())
		}
		buf = binary.LittleEndian.AppendUint32(buf, uint32(len(spirv_data)))
		buf = append(buf, spirv_data...)
		file.Close()
	}

	// Vertex shader
	vertexSPIRV := container.SPIRV_files[container.PassCount]
	vert_file, err := os.Open(vertexSPIRV)
	if err != nil {
		return "", internal.Error("Failed to open file: %s (%s)", vertexSPIRV, err.Error())
	}
	vert_spirv_data, err := io.ReadAll(vert_file)
	if err != nil {
		return "", internal.Error("Failed to read file: %s (%s)", vertexSPIRV, err.Error())
	}
	buf = binary.LittleEndian.AppendUint32(buf, uint32(len(vert_spirv_data)))
	buf = append(buf, vert_spirv_data...)
	file.Write(buf)
	internal.Log("%s them into final wksp file: %s", internal.BoldText.Render("Bundled"), internal.BoldText.Render(output_filename))
	return output_filename, nil
}
