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

# called compare_trustlet
STOCKTRUSTLETDIR=""

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


save_file_list_simple_source() {
  local input_dir=$1  # Base directory for the search
  local output_file="${OUT}/filelist_mysource.txt"  # Output file path
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

save_file_list_simple_vendor() {
  local input_dir=$1  # Base directory for the search
  local output_file="${OUT}/filelist_myvendor.txt"  # Output file path
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


compare_binarys() {
  local trustlet_dir_mysource=$1  # Source directory
  local trustlet_dir_myvendor="${BLOB_ROOT}"  # Vendor directory
  local file_list_myvendor="${OUT}/filelist_myvendor.txt"  # Vendor file list
  local file_list_mysource="${OUT}/filelist_mysource.txt"  # Source file list
  local allowed_file_types=(".tlbin" ".drbin")  # Allowed file types

  # Generate file list from source directory
  > "$file_list_mysource"  # Clearing the file list
  for type in "${allowed_file_types[@]}"; do
    find "$trustlet_dir_mysource" -type f -name "*${type}" -exec basename {} \; >> "$file_list_mysource"
  done

  echo "Starting file comparison..."

  while IFS= read -r file; do
    if grep -Fxq "$file" "$file_list_myvendor"; then
      # If the file exists in both directories, proceed with comparison
      if cmp -s "${trustlet_dir_mysource}/${file}" "${trustlet_dir_myvendor}/${file}"; then
        echo "Identical file: $file"
      else
        echo "Different file: $file"
      fi
    else
      echo "File missing in vendor: $file"
    fi
  done < "$file_list_mysource"

  # Optionally, check for files in the vendor directory that are not in the source (reverse check)
  while IFS= read -r file; do
    if ! grep -Fxq "$file" "$file_list_mysource"; then
      echo "File missing in source: $file"
    fi
  done < "$file_list_myvendor"
}

save_file_list_simple_source "${BLOB_ROOT}"
save_file_list_simple_vendor ""

compare_binarys ""
