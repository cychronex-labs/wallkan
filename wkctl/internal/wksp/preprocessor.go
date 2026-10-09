package wksp

import (
	"io"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"wkctl/internal"
	"wkctl/internal/shader_manifest"
)

var WkShaderHeader string = `#version 450
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_cpp_style_line_directive : require
`

var WkShaderFragmentFooter string = `void main() {
	vec2 fragCoord = vec2(gl_FragCoord.x, gl_FragCoord.y);
	mainImage(fragColor, fragCoord);
}
`

var WkShaderComputeFooter string = `void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    if (coord.x >= int(iResolution.x) || coord.y >= int(iResolution.y)) {
        return;
    }
    vec2 fragCoord = vec2(float(coord.x) + 0.5, float(coord.y) + 0.5);
    vec4 color;
    mainImage(color, fragCoord);
    imageStore(outImage, coord, color);
}`

var WkVertexShader = `#version 450

void main() {
	vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
	gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}`

type UniformName string

const (
	UNIFORM_RESOLUTIONS = "PassesResolution"
	// We put this as it is because we do not need a direct different #define
	UNIFORM_CHANNEL_RES = "iChannelResolution"
	UNIFORM_MOUSE       = "iMouse"
	UNIFORM_FRAME_COUNT = "iFrame"
	UNIFORM_TIME        = "iTime"
)

type Uniform struct {
	Name     string
	GlslType string
	ArrayLen int
}

func generateFragmentHeader(pass *shader_manifest.ShaderPass) string {
	var header strings.Builder
	header.WriteString(WkShaderHeader)
	header.WriteString("layout(location = 0) out vec4 fragColor;")
	return header.String()
}

func generateComputeHeader(pass *shader_manifest.ShaderPass) string {
	var header strings.Builder
	header.WriteString(WkShaderHeader)
	header.WriteString("layout(local_size_x = ")
	header.WriteString(strconv.Itoa(pass.Workgroup[0]))
	header.WriteString(", local_size_y = ")
	header.WriteString(strconv.Itoa(pass.Workgroup[1]))
	header.WriteString(") in;")
	header.WriteByte('\n')
	header.WriteString("layout(set = 3, binding = 0) uniform writeonly image2D outImage;")
	return header.String()
}

func generateUniforms(manifest *shader_manifest.Manifest) []Uniform {
	return []Uniform{
		Uniform{
			Name:     UNIFORM_RESOLUTIONS,
			GlslType: "vec3",
			ArrayLen: len(manifest.ShaderPasses),
		},
		Uniform{
			Name:     UNIFORM_CHANNEL_RES,
			GlslType: "vec3",
		},
		Uniform{
			Name:     UNIFORM_MOUSE,
			GlslType: "vec3",
			ArrayLen: 0,
		},
		Uniform{
			Name:     UNIFORM_FRAME_COUNT,
			GlslType: "int",
			ArrayLen: 0,
		},
		Uniform{
			Name:     UNIFORM_TIME,
			GlslType: "float",
			ArrayLen: 0,
		},
	}
}

func generateFrameUBO(uniforms []Uniform, dynArrayLen func(name string, current int) int) string {
	var frame_ubo strings.Builder
	frame_ubo.WriteString("layout(scalar, set = 0, binding = 0) uniform WkFrameData {\n")
	for i := range uniforms {
		uniform := uniforms[i]

		frame_ubo.WriteByte('\t')
		frame_ubo.WriteString(uniform.GlslType)
		frame_ubo.WriteByte(' ')
		frame_ubo.WriteString(uniform.Name)
		uniform.ArrayLen = dynArrayLen(uniform.Name, uniform.ArrayLen)
		if uniform.ArrayLen > 0 {
			frame_ubo.WriteByte('[')
			frame_ubo.WriteString(strconv.Itoa(uniform.ArrayLen))
			frame_ubo.WriteByte(']')
		}
		frame_ubo.WriteByte(';')
		frame_ubo.WriteByte('\n')
	}
	frame_ubo.WriteByte('}')
	frame_ubo.WriteByte(';')
	return frame_ubo.String()
}

func generateShaderParamsUBO(params []shader_manifest.ShaderParameter) string {

	var param_layout strings.Builder
	param_layout.WriteString("layout(scalar, set = 1, binding = 0) uniform ShaderParameters {\n")
	for i := range params {
		param := &params[i]
		param_layout.WriteByte('\t')
		param_layout.WriteString(param.Type)
		param_layout.WriteByte(' ')
		param_layout.WriteString(param.Name)
		param_layout.WriteByte(';')
		param_layout.WriteByte('\n')
	}
	param_layout.WriteByte('}')
	param_layout.WriteByte(';')
	param_layout.WriteByte('\n')
	return param_layout.String()
}

func generateChannelsLayout(manifest *shader_manifest.Manifest, passIndex int) string {

	var channels strings.Builder
	for i := range manifest.ShaderPasses[passIndex].Channels {
		channels.WriteString("layout(set = 2, binding = ")
		channels.WriteString(strconv.Itoa(i))
		channels.WriteString(") uniform sampler2D ")
		channels.WriteString("iChannel")
		channels.WriteString(strconv.Itoa(i))
		channels.WriteByte(';')
		channels.WriteByte('\n')
	}
	return channels.String()
}

func writeDefinition(definitions *strings.Builder, uniform *Uniform,
	name string, arrayIndex int) {
	definitions.WriteString("#define ")
	definitions.WriteString(name)
	definitions.WriteByte(' ')
	definitions.WriteString(uniform.Name)
	if uniform.ArrayLen > 0 && arrayIndex > -1 {
		definitions.WriteByte('[')
		definitions.WriteString(strconv.Itoa(arrayIndex))
		definitions.WriteByte(']')
	}
	definitions.WriteByte('\n')

}

func generateDefinitions(uniforms []Uniform, manifest *shader_manifest.Manifest,
	passIndex int) string {

	var definitions strings.Builder
	for i := range uniforms {
		uniform := uniforms[i]
		if uniform.Name == UNIFORM_RESOLUTIONS {
			writeDefinition(&definitions, &uniform, "iResolution", passIndex)
		} else if uniform.Name == UNIFORM_CHANNEL_RES {
			for j := range manifest.ShaderPasses[passIndex].Channels {
				// Name
				channel := &manifest.ShaderPasses[passIndex].Channels[j]
				if channel.As != "" {
					definitions.WriteString("#define ")
					definitions.WriteString(channel.As)
					definitions.WriteByte(' ')
					definitions.WriteString("iChannel")
					definitions.WriteString(strconv.Itoa(j))
					definitions.WriteByte('\n')
					// Resolution
					definitions.WriteString("#define ")
					definitions.WriteString(channel.As)
					definitions.WriteString("Resolution")
					definitions.WriteByte(' ')
					definitions.WriteString(uniform.Name)
					definitions.WriteByte('[')
					definitions.WriteString(strconv.Itoa(j))
					definitions.WriteByte(']')
					definitions.WriteByte('\n')
				}
			}
			// i* is considered same as ShaderToy So no additional define is required
		} else if !strings.HasPrefix(uniform.Name, "i") {
			writeDefinition(&definitions, &uniform, uniform.Name, -1)
		}
	}

	return definitions.String()
}

func Preprocess(glsl_input_file string, glsl_output_file string,
	manifest *shader_manifest.Manifest, passIndex int) (string, error) {

	pass := &manifest.ShaderPasses[passIndex]
	uniforms := generateUniforms(manifest)
	var preprocessed_glsl strings.Builder
	var preprocessed_glsl_str string

	input_glsl_f, err := os.Open(glsl_input_file)
	if err != nil {
		internal.Error("Failed to open glsl file %s (%s)", glsl_input_file, err.Error())
		return "", err
	}
	defer input_glsl_f.Close()

	input_glsl, err := io.ReadAll(input_glsl_f)
	if err != nil {
		return "", internal.Error("Failed to read include file: %s", glsl_input_file)
	}
	if pass.Type == "fragment" {
		preprocessed_glsl.WriteString(generateFragmentHeader(pass))
	} else {
		preprocessed_glsl.WriteString(generateComputeHeader(pass))
	}
	preprocessed_glsl.WriteByte('\n')
	preprocessed_glsl.WriteString(generateChannelsLayout(manifest, passIndex))
	preprocessed_glsl.WriteByte('\n')
	preprocessed_glsl.WriteString(generateShaderParamsUBO(manifest.ShaderParams))
	preprocessed_glsl.WriteByte('\n')

	preprocessed_glsl.WriteString(generateFrameUBO(uniforms,
		func(name string, current int) int {
			if name == UNIFORM_CHANNEL_RES {
				return len(pass.Channels)
			}
			return current
		}))
	preprocessed_glsl.WriteByte('\n')
	preprocessed_glsl.WriteString(generateDefinitions(uniforms, manifest, passIndex))
	preprocessed_glsl.WriteByte('\n')
	for i := range manifest.Meta.Includes {
		glsl_inc_file := filepath.Join(manifest.ShaderPath, manifest.Meta.Includes[i])
		preprocessed_glsl.WriteString(`#line 0 "`)
		preprocessed_glsl.WriteString(glsl_inc_file)
		preprocessed_glsl.WriteByte('"')

		preprocessed_glsl.WriteByte('\n')
		inc_glsl_f, err := os.Open(glsl_inc_file)
		if err != nil {
			return "", internal.Error("Failed to open glsl file %s (%s)", glsl_inc_file, err.Error())
		}
		defer inc_glsl_f.Close()
		include_data, err := io.ReadAll(inc_glsl_f)
		if err != nil {
			return "", internal.Error("Failed to read include file: %s", glsl_inc_file)
		}
		preprocessed_glsl.WriteString(string(include_data))
		preprocessed_glsl.WriteByte('\n')
	}
	preprocessed_glsl.WriteString(`#line 0 "`)
	preprocessed_glsl.WriteString(glsl_input_file)
	preprocessed_glsl.WriteByte('"')

	preprocessed_glsl.WriteByte('\n')

	preprocessed_glsl.Write(input_glsl)
	preprocessed_glsl.WriteByte('\n')
	if pass.Type == "fragment" {
		preprocessed_glsl.WriteString(WkShaderFragmentFooter)
	} else {
		preprocessed_glsl.WriteString(WkShaderComputeFooter)
	}

	preprocessed_glsl_str = preprocessed_glsl.String()

	err = os.WriteFile(glsl_output_file, []byte(preprocessed_glsl_str), 0755)
	if err != nil {
		internal.Error("Failed to write glsl file %s (%s)", glsl_output_file, err.Error())
		return "", err
	}

	return preprocessed_glsl_str, nil
}
