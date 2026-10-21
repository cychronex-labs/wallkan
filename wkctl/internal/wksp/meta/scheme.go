package meta

import (
	"strings"
	"wkctl/internal"
	"wkctl/internal/shader_manifest"
)

const (
	VkFilterNearest uint32 = 0
	VkFilterLinear  uint32 = 1

	VkSamplerMipmapModeNearest uint32 = 0
	VkSamplerMipmapModeLinear  uint32 = 1

	VkSamplerAddressModeRepeat         uint32 = 0
	VkSamplerAddressModeMirroredRepeat uint32 = 1
	VkSamplerAddressModeClampToEdge    uint32 = 2

	VkLodClampNone float32 = 1000.0
)

type SamplerConfig struct {
	MagFilter    uint32  `json:"magfilter"`
	MinFilter    uint32  `json:"minfilter"`
	MipmapMode   uint32  `json:"mipmapmode"`
	AddressModeU uint32  `json:"addressmodeu"`
	AddressModeV uint32  `json:"addressmodev"`
	AddressModeW uint32  `json:"addressmodew"`
	MinLod       float32 `json:"minlod"`
	MaxLod       float32 `json:"maxlod"`
}

func ParseSamplerConfig(preset string) (SamplerConfig, error) {
	switch strings.TrimSpace(strings.ToLower(preset)) {
	case "", "linear", "linear_clamp":
		return SamplerConfig{
			MagFilter:    VkFilterLinear,
			MinFilter:    VkFilterLinear,
			MipmapMode:   VkSamplerMipmapModeNearest,
			AddressModeU: VkSamplerAddressModeClampToEdge,
			AddressModeV: VkSamplerAddressModeClampToEdge,
			AddressModeW: VkSamplerAddressModeClampToEdge,
			MinLod:       0.0,
			MaxLod:       0.25,
		}, nil

	case "nearest", "nearest_clamp":
		return SamplerConfig{
			MagFilter:    VkFilterNearest,
			MinFilter:    VkFilterNearest,
			MipmapMode:   VkSamplerMipmapModeNearest,
			AddressModeU: VkSamplerAddressModeClampToEdge,
			AddressModeV: VkSamplerAddressModeClampToEdge,
			AddressModeW: VkSamplerAddressModeClampToEdge,
			MinLod:       0.0,
			MaxLod:       0.25,
		}, nil

	case "linear_repeat":
		return SamplerConfig{
			MagFilter:    VkFilterLinear,
			MinFilter:    VkFilterLinear,
			MipmapMode:   VkSamplerMipmapModeNearest,
			AddressModeU: VkSamplerAddressModeRepeat,
			AddressModeV: VkSamplerAddressModeRepeat,
			AddressModeW: VkSamplerAddressModeRepeat,
			MinLod:       0.0,
			MaxLod:       0.25,
		}, nil

	case "nearest_repeat":
		return SamplerConfig{
			MagFilter:    VkFilterNearest,
			MinFilter:    VkFilterNearest,
			MipmapMode:   VkSamplerMipmapModeNearest,
			AddressModeU: VkSamplerAddressModeRepeat,
			AddressModeV: VkSamplerAddressModeRepeat,
			AddressModeW: VkSamplerAddressModeRepeat,
			MinLod:       0.0,
			MaxLod:       0.25,
		}, nil

	case "linear_mirror":
		return SamplerConfig{
			MagFilter:    VkFilterLinear,
			MinFilter:    VkFilterLinear,
			MipmapMode:   VkSamplerMipmapModeNearest,
			AddressModeU: VkSamplerAddressModeMirroredRepeat,
			AddressModeV: VkSamplerAddressModeMirroredRepeat,
			AddressModeW: VkSamplerAddressModeMirroredRepeat,
			MinLod:       0.0,
			MaxLod:       0.25,
		}, nil

	case "mipmap":
		return SamplerConfig{
			MagFilter:    VkFilterLinear,
			MinFilter:    VkFilterLinear,
			MipmapMode:   VkSamplerMipmapModeLinear,
			AddressModeU: VkSamplerAddressModeClampToEdge,
			AddressModeV: VkSamplerAddressModeClampToEdge,
			AddressModeW: VkSamplerAddressModeClampToEdge,
			MinLod:       0.0,
			MaxLod:       VkLodClampNone,
		}, nil

	case "mipmap_repeat":
		return SamplerConfig{
			MagFilter:    VkFilterLinear,
			MinFilter:    VkFilterLinear,
			MipmapMode:   VkSamplerMipmapModeLinear,
			AddressModeU: VkSamplerAddressModeRepeat,
			AddressModeV: VkSamplerAddressModeRepeat,
			AddressModeW: VkSamplerAddressModeRepeat,
			MinLod:       0.0,
			MaxLod:       VkLodClampNone,
		}, nil

	default:
		return SamplerConfig{}, internal.Error("unknown sampler preset: %q", preset)
	}
}

type metadata struct {
	Name    string `json:"name"`
	Version string `json:"version"`
	Author  string `json:"author"`
	License string `json:"license"`
}

type channel struct {
	Name string                      `json:"name"`
	Type shader_manifest.ChannelType `json:"type"`
	// Either asset index or a shader pass index
	ProducerIndex int              `json:"producer"`
	Sampler       SamplerConfig    `json:"sampler"`
	DescType      VkDescriptorType `json:"desc_type"`
}

type ShaderPassDescriptorSet struct {
	SetIdx        DescriptorSetIndex `json:"set_idx"`
	Type          VkDescriptorType   `json:"type"`
	TotalBindings int                `json:"total_bindings"`
}

type shaderPass struct {
	Name         string        `json:"name"`
	ShaderStage  VkShaderStage `json:"type"`
	Scale        float32       `json:"scale"`
	Formats      []int         `json:"formats,omitempty"`
	Workgroup    [2]int        `json:"workgroup,omitempty"`
	ChannelCount int           `json:"channel_count"`
	SelfFeedback bool          `json:"self_feedback"`
	Iterations   int           `json:"iterations"`
	Channels     []channel     `json:"channels"`
}

type WKSPParamVector[T any] struct {
	Components     [4]T `json:"components"`
	ComponentCount int  `json:"components_count"`
}

type WKSPShaderParam[T any] struct {
	Type    WKSPParamType         `json:"type"`
	Default WKSPParamVector[T]    `json:"default"`
	Current WKSPParamVector[T]    `json:"current"`
	Range   [2]WKSPParamVector[T] `json:"range"`
}

type Descriptor struct {
	Type  VkDescriptorType `json:"type"`
	Count int              `json:"count"`
}

type Descriptors struct {
	Pool        []Descriptor `json:"pool"`
	PoolMaxSets int          `json:"pool_maxsets"`
}

type WKSPManifest struct {
	Meta        metadata     `json:"meta"`
	Descriptors Descriptors  `json:"descriptors"`
	ParamCount  int          `json:"param_count"`
	Params      []any        `json:"params"`
	Passes      []shaderPass `json:"passes"`
}
