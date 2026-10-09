package wksp

import (
	"errors"
	"os/exec"
	"wkctl/internal"
)

func checkGlslc() (string, error) {
	path, err := exec.LookPath("glslc")
	if err != nil {
		if errors.Is(err, exec.ErrNotFound) {
			return "", internal.Error("glslc is not installed or not in PATH (install the Vulkan SDK or shaderc)")
		}
		return "", internal.Error("failed to locate glslc: %s", err.Error())
	}
	return path, nil
}

func Compile(glsl_file string, output_path string, shaderStage string) error {
	internal.Log("Compile %s --> %s (%s)", glsl_file, output_path, shaderStage)
	glslc_path, err := checkGlslc()
	if err != nil {
		return err
	}
	out, err := exec.Command(glslc_path, "-fshader-stage="+shaderStage, glsl_file, "-o", output_path, "--target-env=vulkan1.3").CombinedOutput()
	if err != nil {
		return internal.Error("Compilation failed:\n%s", string(out))
	}
	return nil
}
