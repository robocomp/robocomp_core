import toml
import os
import sys
from typing import Dict, Any, Union
import re

class ConfigLoader:
    @staticmethod
    def _convert_value(value: str) -> Any:
        """Converts a string to its appropriate type (int, float, bool or str)."""
        value = value.strip()
        
        if value.lower() == 'true':
            return True
        if value.lower() == 'false':
            return False
        if value.lower() == 'none':
            return None
            
        # Int
        if re.match(r'^-?\d+$', value):
            return int(value)
            
        # Float
        if re.match(r'^-?\d+\.\d+$', value):
            return float(value)
            
        # Erase strings "" 
        if (value.startswith('"') and value.endswith('"')) or \
           (value.startswith("'") and value.endswith("'")):
            return value[1:-1]
            
        return value

    @staticmethod
    def _nest_config(flat_config: Dict[str, Any]) -> Dict[str, Any]:
        """Convert a flat dictionary with dotted keys to a nested one"""
        nested_config = {}
        
        for key, value in flat_config.items():
            parts = key.split('.')
            current_level = nested_config
            
            for part in parts[:-1]:
                if part not in current_level:
                    current_level[part] = {}
                current_level = current_level[part]
            
            current_level[parts[-1]] = value
        
        return nested_config


    @staticmethod
    def load_from_txt(file_path: str) -> Dict[str, Any]:
        """
        Load configuration from a text file with key=value pairs.
        
        Args:
            file_path: Path to the configuration file
            
        Returns:
            Dictionary with the configuration
            
        Raises:
            FileNotFoundError: If file doesn't exist
            ValueError: If file has invalid format
        """
        config = {}
        if not os.path.exists(file_path):
            raise FileNotFoundError(f"The file {file_path} does not exist.")
        
        with open(file_path, 'r') as file:
            for line_number, line in enumerate(file, 1):
                line = line.strip()
                # Skip empty lines and comments
                if not line or line.startswith('#'):
                    continue
                line = line.split("#")[0]
                    
                try:
                    key, value = map(str.strip, line.split('=', 1))
                    config[key] = ConfigLoader._convert_value(value)
                except ValueError:
                    raise ValueError(
                        f"Invalid format in {file_path}, line {line_number}. "
                        f"Expected 'key=value', got: '{line}'"
                    )
        return ConfigLoader._nest_config(config)

    @staticmethod
    def load_from_toml(file_path: str) -> Dict[str, Any]:
        """
        Load configuration from a TOML file.
        
        Args:
            file_path: Path to the TOML configuration file
            
        Returns:
            Dictionary with the configuration
            
        Raises:
            FileNotFoundError: If file doesn't exist
            toml.TomlDecodeError: If file has invalid TOML syntax
        """
        if not os.path.exists(file_path):
            raise FileNotFoundError(f"The file {file_path} does not exist.")
        
        with open(file_path, 'r') as file:
            try:
                return toml.load(file)
            except toml.TomlDecodeError as e:
                raise toml.TomlDecodeError(
                    f"Invalid TOML syntax in {file_path}: {str(e)}"
                ) from e

    @staticmethod
    def load_config(config_file_path: str) -> Dict[str, Any]:
        """
        Load configuration from either TOML or text file based on extension.
        
        Args:
            config_file_path: Path to the configuration file
            
        Returns:
            Dictionary with the configuration
            
        Raises:
            FileNotFoundError: If file doesn't exist
            ValueError: If text file has invalid format
            toml.TomlDecodeError: If TOML file has invalid syntax
        """
        try:
            if config_file_path.endswith('.toml'):
                return ConfigLoader.load_from_toml(config_file_path)
            else:
                return ConfigLoader.load_from_txt(config_file_path)
        except FileNotFoundError as e:
            print(f"Error: {e}", file=sys.stderr)
            sys.exit(1)
        except (ValueError, toml.TomlDecodeError) as e:
            print(f"Configuration error: {e}", file=sys.stderr)
            sys.exit(1)
        except Exception as e:
            print(f"Unexpected error: {e}", file=sys.stderr)
            sys.exit(1)

    @staticmethod
    def print(configData, indent=0):
        for key, value in configData.items():
            if isinstance(value, dict):
                print(' ' * indent + f"{key}: {{")
                ConfigLoader.print(value, indent + 4)
                print(' ' * indent + "}")
            else:
                print(' ' * indent + f"{key}: {value}")
    
    @staticmethod
    def get_sur_names(config_data: Dict[str, Any], key: str) -> list[str]:
        """
        Get the sub-keys (field names) of a specific section if it is a nested dictionary.
        
        Args:
            config_data: The nested configuration dictionary.
            key: The primary key whose sub-keys are to be retrieved.
            
        Returns:
            A sorted list of sub-keys found under the provided key.
            Returns an empty list if the key is not found or is not a dictionary.
        """
        section = config_data.get(key)
        if isinstance(section, dict):
            return sorted([
                k for k, v in section.items() 
                if isinstance(v, dict)
            ])
        return []

def main():
    if len(sys.argv) < 2:
        print("Usage: python script.py <config_file_path>", file=sys.stderr)
        sys.exit(1)
    
    config_file_path = sys.argv[1]
    try:
        config = ConfigLoader.load_config(config_file_path)
        print("Successfully loaded configuration:")
        ConfigLoader.print(config)
    except Exception as e:
        print(f"Failed to load configuration: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()