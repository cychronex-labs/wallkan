package meta

import "wkctl/internal/shader_manifest"

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
	ProducerIndex int `json:"producer"`
}

type shaderPass struct {
	Name         string        `json:"name"`
	ShaderStage  VkShaderStage `json:"type"`
	Scale        float32       `json:"scale"`
	Formats      []int         `json:"formats,omitempty"`
	Workgroup    [2]int        `json:"workgroup,omitempty"`
	Channels     []channel     `json:"channels"`
	ChannelCount int           `json:"channel_count"`
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
