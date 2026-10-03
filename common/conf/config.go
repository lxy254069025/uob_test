package conf

import (
	"os"

	"gopkg.in/yaml.v2"
)

type RpcClientConf struct {
	Address string `yaml:"Address"`
	Name    string `yaml:"Name"`
	Type    string `yaml:"Type"`
}

type GoZeroClientConf struct {
	Endpoints []string `yaml:"Endpoints"`
	NonBlock  bool     `yaml:"NonBlock"`
	Timeout   int64    `yaml:"Timeout"`
}

func LoadConfig(file string, v interface{}) error {
	content, err := os.ReadFile(file)
	if err != nil {
		return err
	}

	err = yaml.Unmarshal(content, v)

	if err != nil {
		return err
	}

	return nil
}
