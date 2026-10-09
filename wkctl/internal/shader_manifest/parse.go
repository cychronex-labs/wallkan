package shader_manifest

import (
	"errors"
	"fmt"
	"maps"
	"math"
	"os"
	"path/filepath"
	"slices"
	"strings"
	"wkctl/internal"

	"github.com/pelletier/go-toml/v2"
)

const MAX_META_NAME_LEN = 64
const MAX_IN_SHADER_NAMES_LEN = 64

// Vulkan enum values
var ImageFormatMap = map[string]int{
	"r8":         9,
	"rg8":        16,
	"rgba8":      37,
	"r16f":       76,
	"rg16f":      83,
	"rgba16f":    97,
	"r32ui":      98,
	"r32f":       100,
	"rg32f":      103,
	"rgba32f":    109,
	"rgba8_srgb": 43,
}

var AllowedPresentationFormats = []string{
	"rgba8",
	"rgba8_srgb",
	"rgba16f",
	"rgb10a2",
}

var ChannelFilterMap = map[string]uint32{
	"linear":  0,
	"nearest": 1,
}

var ChannelWrapMap = map[string]uint32{
	"clamp":  2,
	"repeat": 0,
	"mirror": 1,
	"border": 3,
}

type ShaderParamTypeDef struct {
	ComponentCount int
	Kind           string
}

var ShaderParameterTypes = map[string]ShaderParamTypeDef{
	"float": {ComponentCount: 1, Kind: "float"},
	"vec2":  {ComponentCount: 2, Kind: "float"},
	"vec3":  {ComponentCount: 3, Kind: "float"},
	"vec4":  {ComponentCount: 4, Kind: "float"},

	"int":   {ComponentCount: 1, Kind: "int"},
	"ivec2": {ComponentCount: 2, Kind: "int"},
	"ivec3": {ComponentCount: 3, Kind: "int"},
	"ivec4": {ComponentCount: 4, Kind: "int"},

	"uint":  {ComponentCount: 1, Kind: "uint"},
	"uvec2": {ComponentCount: 2, Kind: "uint"},
	"uvec3": {ComponentCount: 3, Kind: "uint"},
	"uvec4": {ComponentCount: 4, Kind: "uint"},

	"bool":  {ComponentCount: 1, Kind: "bool"},
	"bvec2": {ComponentCount: 2, Kind: "bool"},
	"bvec3": {ComponentCount: 3, Kind: "bool"},
	"bvec4": {ComponentCount: 4, Kind: "bool"},
}

func New(path string) (*Manifest, error) {
	var manifest Manifest
	file, err := os.Open(path)
	if err != nil {
		return nil, internal.Error("Failed to open manifest %s", path)
	}
	defer file.Close()

	dec := toml.NewDecoder(file)
	dec.DisallowUnknownFields()
	if err := dec.Decode(&manifest); err != nil {
		var strictErr *toml.StrictMissingError
		if errors.As(err, &strictErr) {
			return nil, internal.Error("Failed to parse toml!\n%s", strictErr.String())
		}
		return nil, internal.Error("Failed to parse toml %s", err.Error())
	}
	return &manifest, err
}

func validateGlslFloat(i any) error {
	switch val := i.(type) {
	case float64:
		if math.IsNaN(val) || math.IsInf(val, 0) {
			return internal.Error("Value cannot be NaN or Infinity!")
		}
		if val > math.MaxFloat32 && val < -math.MaxFloat32 {
			return internal.Error("Value must be valid float32 (float64 not supported)!")
		}
	case int64:
		fl_v := float64(val)
		if fl_v > math.MaxFloat32 && fl_v < -math.MaxFloat32 {
			return internal.Error("Value must be valid float32 (float64 not supported)!")
		}
	default:
		return internal.Error("Value not a float!")
	}
	return nil
}

func validateGlslInt(i any) error {
	switch val := i.(type) {
	case int64:
		if val > math.MaxInt32 && val > math.MinInt32 {
			return internal.Error("Value must be valid int32 (int64 not supported)!")
		}
	default:
		return internal.Error("Value not an int!")
	}
	return nil
}

func validateGlslBool(i any) error {
	switch i.(type) {
	case bool:
		return nil
	default:
		return internal.Error("Value not an bool!")
	}
}

func validateScalarType(i any, kind string) error {
	switch kind {
	case "float":
		return validateGlslFloat(i)
	case "int":
		return validateGlslInt(i)
	case "bool":
		return validateGlslBool(i)
	}
	return nil
}

func validateVector(slice []any, kind string) error {
	for i := range slice {
		internal.Log("Validate component (pos: %d)", i)
		err := validateScalarType(slice[i], kind)
		if err != nil {
			return err
		}
	}
	return nil
}

func findNameCollisions(manifest *Manifest) error {
	// Global Scope
	names := []string{}
	// Parameter names
	for i := range manifest.ShaderParams {
		name := manifest.ShaderParams[i].Name
		if !slices.Contains(names, name) {
			names = append(names, name)
		} else {
			return internal.Error("Shader parameter name collides with previously defined name: %s", name)
		}
	}
	// Shader pass names
	for i := range manifest.ShaderPasses {
		// A single Glsl scope
		scope_names := names
		for j := range manifest.ShaderPasses[i].Channels {
			name := manifest.ShaderPasses[i].Channels[j].As
			if !slices.Contains(scope_names, name) {
				scope_names = append(names, name)
			} else {
				return internal.Error("Shader pass[%d].channel[%d] 'as' collides with previously defined parameter: %s", i, j, name)
			}
		}

	}
	return nil
}

func validateShaderParamType(param *ShaderParameter, kind string) error {
	// Components count validation
	internal.Log("Validate Default value")
	if param.ComponentCount > 1 {
		// Slice or Not is validated before this function gets called
		vec, _ := param.Default.([]any)

		if err := validateVector(vec, kind); err != nil {
			return err
		}

		internal.Log("Validate Range values")
		min_range_vec, min_range_is_vec := param.Range[0].([]any)
		max_range_vec, max_range_is_vec := param.Range[1].([]any)
		if !min_range_is_vec || !max_range_is_vec {
			return internal.Error("Range values must be an array matching the type: %s", param.Type)
		}
		internal.Log("Validate Range minimum value")
		if err := validateVector(min_range_vec, kind); err != nil {
			return err
		}
		internal.Log("Validate Range maximum value")
		if err := validateVector(max_range_vec, kind); err != nil {
			return err
		}
	} else {
		if err := validateScalarType(param.Default, kind); err != nil {
			return err
		}
		internal.Log("Validate Range minimum value")
		if err := validateScalarType(param.Range[0], kind); err != nil {
			return err
		}
		internal.Log("Validate Range maximum value")
		if err := validateScalarType(param.Range[1], kind); err != nil {
			return err
		}
	}

	return nil
}

func validateShaderParameters(manifest *Manifest) error {
	internal.Log("Validate shader parameters")
	for i := range manifest.ShaderParams {
		param := &manifest.ShaderParams[i]
		internal.Log("Validate Shader parameter: %s", param.Name)
		internal.Log("Validate type")
		validateShaderParamType(param, ShaderParameterTypes[param.Type].Kind)
		fmt.Fprintf(os.Stderr, "%s\n", internal.BoldText.Render("----------"))
	}
	return nil
}

func validateEngineConfig(conf *EngineConfig) error {
	if conf == nil {
		return nil
	}
	internal.Log("Validate engine config")
	if conf.Fps.Preferred == "auto" {
		internal.Log("FPS.preferred = 'auto'")
		conf.Fps.Preferred = 0
	}
	return nil
}

func validateAsset(path string) {
	internal.Log("Validate asset: '%s'", path)
}

func validateChannelSamplerOptions(pass *ShaderPass, ch_index int) error {
	channel := &pass.Channels[ch_index]
	internal.Log("Validate Channel filter mode")
	if channel.Filter == "" {
		internal.Log("No Channel filter mode specified, default: linear")
		channel.Filter = "linear"
	}
	if channel.Wrap == "" {
		internal.Log("No Channel wrap mode specified, default: clamp")
		channel.Wrap = "clamp"
	}
	if _, exists := ChannelFilterMap[channel.Filter]; !exists {
		return internal.Error("Unknown channel filter option: '%s', available filter modes: %q", channel.Filter, slices.Collect(maps.Keys(ChannelFilterMap)))
	}
	internal.Log("Validate Channel wrap mode")
	if _, exists := ChannelWrapMap[channel.Wrap]; !exists {
		return internal.Error("Unknown channel wrap option: '%s', available: %q", channel.Wrap, slices.Collect(maps.Keys(ChannelWrapMap)))
	}
	return nil
}

func validateChannels(manifest *Manifest, index int) error {
	pass := &manifest.ShaderPasses[index]
	internal.Log("Validate Channels of shader pass[%d] (%s)", index, pass.Name)
	shaderPassCount := len(manifest.ShaderPasses)

	channels_len := len(pass.Channels)

	for i := range channels_len {
		internal.Log("channel[%d] - %s", i, pass.Channels[i].As)
		channel := &pass.Channels[i]
		if channel.From == "self" {
			channel.Type = WK_CHANNEL_TYPE_SHADER_PASS
			channel.ProducerIndex = index
			internal.Log("Channel type: self (%s)", pass.Name)
			continue
		}
		if strings.HasPrefix(channel.From, "asset:") {
			internal.Log("Channel type: Asset")
			channel.Type = WK_CHANNEL_TYPE_SHADER_ASSET
			validateAsset(channel.From)
			continue
		}
		channel.Type = WK_CHANNEL_TYPE_SHADER_PASS
		internal.Log("Channel type: Shader Pass")
		channel.ProducerIndex = -1
		for j := range shaderPassCount {
			if manifest.ShaderPasses[j].Name == channel.From {
				internal.Log("Channel producer buffer: pass[%d] - %s", j, channel.From)
				channel.ProducerIndex = j
				break
			}
		}
		if channel.ProducerIndex == -1 {
			internal.Error("Producer: Unknown. Could not find shader pass: '%s'", channel.From)
		}
		if err := validateChannelSamplerOptions(pass, i); err != nil {
			return err
		}
	}
	return nil
}

func validateShaderPassFormat(manifest *Manifest, index int) error {
	pass := &manifest.ShaderPasses[index]
	formatLen := len(pass.Formats)
	totalPasses := len(manifest.ShaderPasses)
	var newPassFormats []string = pass.Formats
	for i := range pass.Formats {
		imgFormat := pass.Formats[i]
		if _, exists := ImageFormatMap[imgFormat]; !exists {
			return internal.Error("Unknown format: '%s', available formats: %s", imgFormat, slices.Collect(maps.Keys(ImageFormatMap)))
		}

		if index == totalPasses-1 {
			if res := slices.Contains(AllowedPresentationFormats, imgFormat); !res {
				return internal.Error("Image format: '%s' is not allowed in the final image pass!", imgFormat)
			}
			continue
		}

	}
	if len(newPassFormats) == 0 {
		pass.Formats = append(pass.Formats, "rgba8")
		formatLen = 1
	}
	if pass.Formats[formatLen-1] == "rgba8_srgb" {
		if index != totalPasses-1 {
			return internal.Error("Image format: rgba8_srgb is only supported for image pass")
		}
		internal.Warn("To prevent shader visual issues, if a device does not support rgba8_srgb, The shader will be prevented from running! Only rgba8 guarantees universal compatibility!")
	} else {
		if pass.Formats[formatLen-1] != "rgba8" {
			internal.Warn("Shader implicitly fallbacks to rgba8 if %s is not supported by the device!", pass.Formats[formatLen-1])
			newPassFormats = append(pass.Formats, "rgba8")
		}
	}
	pass.Formats = newPassFormats
	return nil
}
func validateShaderPassType(manifest *Manifest, index int) error {
	pass := &manifest.ShaderPasses[index]
	internal.Log("Validate type")
	if pass.Type != "fragment" && pass.Type != "compute" {
		if pass.Type == "" {
			internal.Log("No type specified, Default to fragment")
		} else {
			return internal.Error("Unknown type: '%s', available: [fragment (default), compute]", pass.Type)
		}
	} else {
		internal.Log("Shader pass type selected: '%s'", pass.Type)
	}
	if pass.Type == "compute" {
		if index == len(manifest.ShaderPasses)-1 {
			return internal.Error("Final image must be a fragment shader. Compute shader not supported Yet!")
		}
		if pass.Workgroup == [2]int{0, 0} {
			internal.Log("No workgroup for compute specified, Default to 16x16")
			pass.Workgroup = [2]int{16, 16}
		} else {
			internal.Log("Selected workgroup: %d:%d", pass.Workgroup[0], pass.Workgroup[1])
		}
	}
	return nil
}

func validateShaderPass(manifest *Manifest, index int) error {
	pass := &manifest.ShaderPasses[index]
	if len(pass.Name) > MAX_IN_SHADER_NAMES_LEN {
		return internal.Error("A shader pass name must be under %d characters", MAX_IN_SHADER_NAMES_LEN)
	}
	internal.Log("Validate shader pass[%d] - %s", index, pass.Name)
	pass_source := filepath.Join(manifest.ShaderPath, pass.Source)
	internal.Log("Validate pass source: %s", pass_source)
	if file, err := os.Open(pass_source); err == nil {
		file.Close()
	} else {
		return internal.Error("Could not access file: %s", pass_source)
	}
	internal.Log("Validate scale, range: [0.0f, 1.0f]")
	if pass.Scale == 0 {
		pass.Scale = 1.0
	} else {
		if pass.Scale < 0.0 {
			return internal.Error("Scale < 0.0, Scale must be within 0.0 and 1.0f")

		} else if pass.Scale > 1.0 {
			return internal.Error("Scale > 1.0, Scale must be within 0.0 and 1.0f")
		}
	}
	internal.Log("Validate formats")
	if err := validateShaderPassFormat(manifest, index); err != nil {
		return err
	}
	if err := validateShaderPassType(manifest, index); err != nil {
		return err
	}
	if err := validateChannels(manifest, index); err != nil {
		return err
	}

	fmt.Fprintf(os.Stderr, "%s\n", internal.BoldText.Render("----------"))
	return nil
}

func validateMeta(meta *Meta) error {
	if len(meta.Name) > MAX_IN_SHADER_NAMES_LEN {
		return internal.Error("A shader name must be under %d characters!", MAX_IN_SHADER_NAMES_LEN)
	}
	return nil
}

func (manifest *Manifest) Validate() error {
	if err := validateMeta(&manifest.Meta); err != nil {
		return err
	}
	if err := validateEngineConfig(manifest.Engine); err != nil {
		return err
	}
	for i := range manifest.ShaderPasses {
		if err := validateShaderPass(manifest, i); err != nil {
			return err
		}
	}

	paramTypes := slices.Collect(maps.Keys(ShaderParameterTypes))
	paramNames := slices.Collect(maps.Keys(manifest.TomlShaderParams))
	slices.Sort(paramNames)
	for i := range paramNames {
		name := paramNames[i]
		if len(name) > MAX_IN_SHADER_NAMES_LEN {
			return internal.Error("Shader parameter name must be under %d characters", MAX_IN_SHADER_NAMES_LEN)
		}
		param := manifest.TomlShaderParams[name]
		internal.Log("Validate type data of shader parameter: %s", name)
		parsed_param := ShaderParameter{
			Name:    name,
			Type:    param.Type,
			Default: param.Default,
			Range:   param.Range,
		}
		param_type, param_exists := ShaderParameterTypes[param.Type]
		if !param_exists {
			return internal.Error("Invalid parameter type: %s', available types: %q", param.Type, paramTypes)
		}

		parsed_param.ComponentCount = param_type.ComponentCount
		// Vector types validation
		if parsed_param.ComponentCount > 1 {
			if slice, ok := param.Default.([]any); ok {
				if len(slice) != parsed_param.ComponentCount {
					return internal.Error("Default value does not match the type: %s",
						param.Type)
				}
			} else {
				return internal.Error("Default value is not an array even though type is: %s",
					param.Type)
			}

			if slice, ok := param.Range[0].([]any); ok {
				if len(slice) != parsed_param.ComponentCount {
					return internal.Error("Range[0] value does not match the type: %s",
						param.Type)
				}
			} else {
				return internal.Error("Range[0] value is not an array even though type is: %s",
					param.Type)
			}

			if slice, ok := param.Range[1].([]any); ok {
				if len(slice) != parsed_param.ComponentCount {
					return internal.Error("Range[1] value does not match the type: %s",
						param.Type)
				}
			} else {
				return internal.Error("Range[1] value is not an array even though type is: %s",
					param.Type)
			}
		}
		manifest.ShaderParams = append(manifest.ShaderParams, parsed_param)
	}
	fmt.Fprintf(os.Stderr, "%s\n", internal.BoldText.Render("----------"))

	if err := validateShaderParameters(manifest); err != nil {
		return err
	}
	if err := findNameCollisions(manifest); err != nil {
		return err
	}
	internal.Log("Validation succeeded!")
	return nil
}
