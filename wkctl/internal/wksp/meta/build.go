package meta

import "wkctl/internal/shader_manifest"

type VkShaderStage int

const (
	VK_SHADER_STAGE_FRAGMENT_BIT VkShaderStage = 16
	VK_SHADER_STAGE_COMPUTE_BIT  VkShaderStage = 32
)

type VkDescriptorType int

const (
	VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER VkDescriptorType = 1
	VK_DESCRIPTOR_TYPE_STORAGE_IMAGE          VkDescriptorType = 3
	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER         VkDescriptorType = 6
)

type WKSPParamType int

const (
	WKSP_PARAM_TYPE_INT WKSPParamType = iota
	WKSP_PARAM_TYPE_UINT
	WKSP_PARAM_TYPE_FLOAT
	WKSP_PARAM_TYPE_BOOL
)

func createVecFromShaderParam[T any](shader_param *shader_manifest.ShaderParameter, converter func(any) T) (WKSPParamVector[T],
	WKSPParamVector[T], WKSPParamVector[T]) {
	//
	default_vec := WKSPParamVector[T]{
		ComponentCount: shader_param.ComponentCount,
	}
	min_range_vec := WKSPParamVector[T]{
		ComponentCount: shader_param.ComponentCount,
	}
	max_range_vec := WKSPParamVector[T]{
		ComponentCount: shader_param.ComponentCount,
	}
	if shader_param.ComponentCount == 1 {
		default_vec.Components[0] = converter(shader_param.Default)
		min_range_vec.Components[0] = converter(shader_param.Range[0])
		max_range_vec.Components[0] = converter(shader_param.Range[1])
		return default_vec, min_range_vec, max_range_vec
	}
	for i := 0; i < shader_param.ComponentCount; i++ {
		default_vec.Components[i] = converter(shader_param.Default.([]any)[i])
		min_range_vec.Components[i] = converter(shader_param.Range[0].([]any)[i])
		max_range_vec.Components[i] = converter(shader_param.Range[1].([]any)[i])
	}
	return default_vec, min_range_vec, max_range_vec

}
func toFloat32(val any) float32 {
	switch v := val.(type) {
	case float32:
		return v
	case float64:
		return float32(v)
	case int64:
		return float32(v)
	}
	return 0.0
}

func toUint32(val any) uint32 {
	switch v := val.(type) {
	case float32:
		return uint32(v)
	case float64:
		return uint32(v)
	case int64:
		return uint32(v)
	}
	return 0.0
}

func toBool(val any) bool {
	switch v := val.(type) {
	case bool:
		return bool(v)
	}
	return false
}

func toInt32(val any) int32 {
	switch v := val.(type) {
	case float64:
		return int32(v)
	case int64:
		return int32(v)
	}
	return 0.0
}

func Build(manifest *shader_manifest.Manifest) *WKSPManifest {
	wksp_manifest := WKSPManifest{
		Meta: metadata{
			Name:    manifest.Meta.Name,
			Version: manifest.Meta.Version,
			License: manifest.Meta.License,
		},
		ParamCount: len(manifest.ShaderParams),
	}
	// One for WkFrameData, One for Shader Parameters
	wksp_manifest.Descriptors.Pool = append(wksp_manifest.Descriptors.Pool, Descriptor{
		Type:  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		Count: 2,
	})
	wksp_manifest.Descriptors.PoolMaxSets = 2

	for i := range wksp_manifest.ParamCount {
		param := &manifest.ShaderParams[i]

		switch shader_manifest.ShaderParameterTypes[param.Type].Kind {
		case "int":
			wksp_param := WKSPShaderParam[int32]{
				Type: WKSP_PARAM_TYPE_INT,
			}
			wksp_param.Default, wksp_param.Range[0], wksp_param.Range[1] = createVecFromShaderParam[int32](param, toInt32)
			wksp_param.Current = wksp_param.Default
			wksp_manifest.Params = append(wksp_manifest.Params, wksp_param)
		case "float":
			wksp_param := WKSPShaderParam[float32]{
				Type: WKSP_PARAM_TYPE_FLOAT,
			}
			val, is_int := param.Default.(int64)
			if is_int {
				param.Default = float32(val)
			}
			val, is_int = param.Range[0].(int64)
			if is_int {
				param.Range[0] = float32(val)
			}
			val, is_int = param.Range[1].(int64)
			if is_int {
				param.Range[1] = float32(val)
			}
			wksp_param.Default, wksp_param.Range[0], wksp_param.Range[1] = createVecFromShaderParam[float32](param, toFloat32)
			wksp_param.Current = wksp_param.Default
			wksp_manifest.Params = append(wksp_manifest.Params, wksp_param)
		case "uint":
			wksp_param := WKSPShaderParam[uint32]{
				Type: WKSP_PARAM_TYPE_UINT,
			}
			wksp_param.Default, wksp_param.Range[0], wksp_param.Range[1] = createVecFromShaderParam[uint32](param, toUint32)
			wksp_param.Current = wksp_param.Default
			wksp_manifest.Params = append(wksp_manifest.Params, wksp_param)
		case "bool":
			wksp_param := WKSPShaderParam[bool]{
				Type: WKSP_PARAM_TYPE_BOOL,
			}
			wksp_param.Default, wksp_param.Range[0], wksp_param.Range[1] = createVecFromShaderParam[bool](param, toBool)
			wksp_param.Current = wksp_param.Default
			wksp_manifest.Params = append(wksp_manifest.Params, wksp_param)
		}

	}
	storageImgDescIdx := len(wksp_manifest.Descriptors.Pool)
	wksp_manifest.Descriptors.Pool = append(wksp_manifest.Descriptors.Pool, Descriptor{
		Type:  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		Count: 0,
	})
	combinedImgSamplerDescIdx := len(wksp_manifest.Descriptors.Pool)
	wksp_manifest.Descriptors.Pool = append(wksp_manifest.Descriptors.Pool, Descriptor{
		Type:  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		Count: 0,
	})
	for i := range len(manifest.ShaderPasses) {
		tomlPass := &manifest.ShaderPasses[i]
		channelCount := len(tomlPass.Channels)
		wksp_pass := shaderPass{
			Name:         tomlPass.Name,
			Scale:        tomlPass.Scale,
			Workgroup:    tomlPass.Workgroup,
			ChannelCount: channelCount,
		}
		wksp_manifest.Descriptors.PoolMaxSets++
		if tomlPass.Type == "fragment" {
			wksp_pass.ShaderStage = VK_SHADER_STAGE_FRAGMENT_BIT
		} else {
			wksp_pass.ShaderStage = VK_SHADER_STAGE_COMPUTE_BIT
			wksp_manifest.Descriptors.PoolMaxSets++
			wksp_manifest.Descriptors.Pool[storageImgDescIdx].Count++
		}
		wksp_manifest.Descriptors.Pool[combinedImgSamplerDescIdx].Count++
		for j := range len(tomlPass.Formats) {
			wksp_pass.Formats = append(wksp_pass.Formats,
				shader_manifest.ImageFormatMap[tomlPass.Formats[j]])
		}
		for j := range channelCount {
			toml_channel := &tomlPass.Channels[j]
			wksp_channel := channel{
				Name:          toml_channel.As,
				Type:          toml_channel.Type,
				ProducerIndex: toml_channel.ProducerIndex,
			}
			wksp_pass.Channels = append(wksp_pass.Channels, wksp_channel)
		}
		wksp_manifest.Passes = append(wksp_manifest.Passes, wksp_pass)
	}
	return &wksp_manifest
}
