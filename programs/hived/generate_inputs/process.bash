#!/bin/bash

# To generate openapi.json, run:
# /path/to/hived --dump-options > hive/programs/hived/generate_inputs/openapi.json

# Input and output file names
INPUT_FILE="openapi.json"
OUTPUT_FILE_NAME="openapi"
OUTPUT_FILE="${OUTPUT_FILE_NAME}.preprocessed.json"

# Use jq to perform the transformations
jq '
# 1. Replace OpenAPI version from 3.1.0 to 3.0.0
.openapi = "3.0.0" |

# 2. Add empty paths object if it doesn'\''t exist or ensure it'\''s empty
.paths = {} |

# 3. Remove common from components.schemas
if .components.schemas then
  .components.schemas |= del(.common)
else
  .
end |

# 3.1. Remove specific keys from arguments.properties
if .components.schemas.arguments.properties then
  .components.schemas.arguments.properties |= del(.version, .help, ."dump-options", ."dump-config")
else
  .
end |

# 4. Convert default_value objects to simple default values
def convert_default_value:
  if has("default_value") and (.default_value | type) == "object" then
    .default = .default_value.value | del(.default_value)
  else
    .
  end;


# 5. Set default values based on type if not already set
def set_default_value:
  if has("type") and ((has("default") | not) or (.default == null)) and has("example") then
    if (.type | type) == "string" then
      if .type == "boolean" or .type == "bool" then
        .default = false
      elif .type == "array" then
        .default = []
      else
        .default = null
      end
    else
      .default = null
    end
  else
    .
  end;

# 6. Convert all type strings to arrays with null option, EXCEPT for "object" types and these without set default
def make_types_optional:
  if has("type") and (.type | type) == "string" and .type != "object" and ((has("default") | not) or .default == null) and has("example") then
    .type = [.type, "null"]
  else
    .
  end;

# 7. Make types strict when default is not null
def make_types_strict:
  if has("type") and has("default") and .default != null then
    if (.type | type) == "array" and (.type | contains(["null"])) then
      .type = (.type | map(select(. != "null")))
      | if length == 1 then .[0] else . end
    else
      .
    end
  else
    .
  end;

# 8. Convert all keys to snake_case
def to_snake_case:
  gsub("-"; "_") | ascii_downcase;

def convert_keys_to_snake_case:
  if type == "object" then
    with_entries(.key |= to_snake_case) | map_values(convert_keys_to_snake_case)
  elif type == "array" then
    map(convert_keys_to_snake_case)
  else
    .
  end;

# Apply transformations recursively to all objects
walk(
  if type == "object" then
    convert_default_value | set_default_value | make_types_optional | make_types_strict
  else
    .
  end
) | convert_keys_to_snake_case
' "$INPUT_FILE" | \
# Use sed to replace quoted large numbers with unquoted ones
sed 's/"18446744073709551615"/18446744073709551615/g' > "$OUTPUT_FILE"

echo "Transformation complete. Output saved to $OUTPUT_FILE"

# Split the transformed JSON into two files based on components.schemas
echo "Splitting into config and arguments files..."

# Create openapi-config.json with only config-related schemas
jq '
  if .components.schemas then
    .components.schemas = (.components.schemas | with_entries(select(.key | test("arguments"; "i") | not)))
  else
    .
  end
' "$OUTPUT_FILE" > "hived_inputs_description/${OUTPUT_FILE_NAME}-config.json"

# Create openapi-arguments.json with only arguments-related schemas
jq '
  if .components.schemas then
    .components.schemas = (.components.schemas | with_entries(select(.key | test("config"; "i") | not)))
  else
    .
  end
' "$OUTPUT_FILE" > "hived_inputs_description/${OUTPUT_FILE_NAME}-arguments.json"

echo "Split complete. Created openapi-config.json and openapi-arguments.json"
