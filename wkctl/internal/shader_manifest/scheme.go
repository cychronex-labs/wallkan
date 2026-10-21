package shader_manifest

const MAX_CHANNELS int = 16

type ChannelType uint32

const (
	WK_CHANNEL_TYPE_SHADER_PASS ChannelType = iota
	WK_CHANNEL_TYPE_SHADER_ASSET
)

type SamplerOptions string

const (
	SamplerLinear        SamplerOptions = "linear"
	SamplerLinearClamp   SamplerOptions = "linear_clamp"
	SamplerNearest       SamplerOptions = "nearest"
	SamplerNearestClamp  SamplerOptions = "nearest_clamp"
	SamplerLinearRepeat  SamplerOptions = "linear_repeat"
	SamplerNearestRepeat SamplerOptions = "nearest_repeat"
	SamplerLinearMirror  SamplerOptions = "linear_mirror"
	SamplerMipmap        SamplerOptions = "mipmap"
	SamplerMipmapRepeat  SamplerOptions = "mipmap_repeat"
)
const DefaultSamplerOption = SamplerLinear

type Meta struct {
	Name     string   `toml:"name"`
	Version  string   `toml:"version"`
	Author   string   `toml:"author"`
	License  string   `toml:"license"`
	Includes []string `toml:"includes"`
}

type FpsSettings struct {
	Min       int
	Max       int
	Preferred any
}

type EngineConfig struct {
	Fps FpsSettings
}

type tomlShaderParameter struct {
	Type    string `toml:"type"`
	Default any    `toml:"default"`
	Range   [2]any `toml:"range"`
}

// Parsed from the map of TomlShaderParameter
type ShaderParameter struct {
	Name           string
	Type           string
	ComponentCount int
	Default        any
	Range          [2]any
}

type Channel struct {
	From          string      `toml:"from"`
	Type          ChannelType `toml:"-"`
	ProducerIndex int         `toml:"-"`
	As            string      `toml:"as"`
	Sampler       string      `toml:"sampler"`
}

type ShaderPass struct {
	Name       string    `toml:"name"`
	Type       string    `toml:"type"`
	Source     string    `toml:"source"`
	Scale      float32   `toml:"scale"`
	Formats    []string  `toml:"formats,omitempty"`
	Workgroup  [2]int    `toml:"workgroup,omitempty"`
	Channels   []Channel `toml:"channels"`
	Iterations int       `toml:"iterations"`
}

type Manifest struct {
	Meta             Meta                           `toml:"meta"`
	Engine           *EngineConfig                  `toml:"engine,omitempty"`
	TomlShaderParams map[string]tomlShaderParameter `toml:"params,omitempty"`
	ShaderParams     []ShaderParameter              `toml:"-"`
	ShaderPasses     []ShaderPass                   `toml:"pass"`
	ShaderPath       string                         `toml:"-"`
}
