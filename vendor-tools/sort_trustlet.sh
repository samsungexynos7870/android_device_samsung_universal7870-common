#!/bin/bash

set -e

DEVICE_COMMON=universal7870-common
VENDOR=samsung
# TOOLS_DIR=vendor-tools

# Load extract_utils and do some sanity checks
MY_DIR="${BASH_SOURCE%/*}"
if [[ ! -d "${MY_DIR}" ]]; then MY_DIR="${PWD}"; fi

ANDROID_ROOT="${MY_DIR}/../../../.."
# HELPER="${ANDROID_ROOT}/tools/extract-utils/extract_utils.sh"

BLOB_ROOT="${ANDROID_ROOT}/vendor/${VENDOR}/${DEVICE_COMMON}/proprietary"

TRUSTLETDIR="${BLOB_ROOT}/vendor/app/mcRegistry"

cleanup_out_dir() {
  local dir_path=$1

  # Check if the directory exists
  if [ -d "$dir_path" ]; then
    # Delete all files in the directory
    echo "Deleting all files in: $dir_path"
    rm -rf "$dir_path"/*
  else
    echo "Directory does not exist: $dir_path"
  fi
}

cleanup_out_dir "out"

OUT="out"

mkdir -p "${OUT}"

# Output file path
OUTPUT_FILE="${OUT}/out.txt"


# myinputdir find some/files
# some/filespath without myinputdir 

save_file_list_simple() {
  local input_dir=$1  # Base directory for the search
  local output_file="${OUT}/filelist.txt"  # Output file path
  local allowed_file_types=(".tlbin" ".drbin")  # Array of allowed file types

  > "$output_file"  # Ensure the file is empty before starting

  if [[ -d "${input_dir}" ]]; then
    echo "Saving file names with paths relative to ${input_dir} to ${output_file}."
    # Find files of allowed types and process each one
    while IFS= read -r -d '' file; do
      # Make path relative by removing the input_dir part
      local relative_path="${file#${input_dir}/}"
      echo "${relative_path}" >> "${output_file}"
    done < <(find "${input_dir}" -type f \( -name "*${allowed_file_types[0]}" -o -name "*${allowed_file_types[1]}" \) -print0)
  else
    echo "Directory does not exist: ${input_dir}"
  fi
}

generate_descriptions_based_on_content() {
  local trustlet_dir="${BLOB_ROOT}"
  local file_list="${OUT}/filelist.txt"
  local output_file="${OUT}/descriptions.txt"

  # Define an associative array with keywords and their descriptions
  # high oucourence shoud be first
  declare -A KEYWORDS=(
    ["MCLF"]="MCLF Registy file"
    ["TIMA"]="Part of SAMSUNG TrustZone-based Integrity Measurement Architecture (TIMA) | KNOX"
    ["CCMid"]="Contains CCMid Keystore and Client Certificate Manager (CCM) | KNOX"
    ["TlCm"]="TlCm"
    ["TRUSTLET ASSERT"]="contains TRUSTLET ASSERT"
    ["TEE Keymaster"]="TEE Keymaster"
    ["egisFpSnsrTest"]="egisFpSnsrTest | Samsung Biometrics"
    ["TEE Gatekeeper"]="TEE Gatekeeper"
    ["TIMA_INIT"]="SAMSUNG TIMA"
    ["KEYMAN"]="KEYMAN driver"
    ["Trustlet tima_lkmauth"]="Trustlet tima_lkmauth | lkm_sec_info rsa signature verification"
    ["TIMA ATN"]="TIMA ATN"
    ["SecFastDrv"]="SecFastDrv Fastcall handler"
    ["sec_face"]="sec_face | face unlock feature"
    ["crypto/rsa/rsa.c"]="crypto rsa cert handler"
    ["TUI Secure Driver"]="TUI Secure Driver | teegris"
  )

  # Check if the directory and file list exist
  if [ ! -d "$trustlet_dir" ] || [ ! -f "$file_list" ]; then
    echo "The trustlet directory or file list does not exist."
    return 1
  fi

  # Clear or create the output file
  > "$output_file"

  # Read each filename from the file list
  while IFS= read -r filename; do
    local full_path="${trustlet_dir}/${filename}"
    if [ ! -f "$full_path" ]; then
      echo "File does not exist: $full_path" >> "$output_file"
      continue
    fi

    local description=""

    # Use the KEYWORDS array to search for keywords and add descriptions
    for key in "${!KEYWORDS[@]}"; do
      if grep -qi "$key" "$full_path"; then
        description+="# ${KEYWORDS[$key]} "
      fi
    done

    # If no keywords were found, add a default message
    if [ -z "$description" ]; then
      description="# No keywords found."
    fi

    # Write the description and filename to the output file
    echo -e "$description \n$filename\n" >> "$output_file"
  done < "$file_list"

  echo "Descriptions based on content generated in $output_file."
}

# Example usage of the function with the TRUSTLETDIR variable
save_file_list_simple "${BLOB_ROOT}"

generate_descriptions_based_on_content
