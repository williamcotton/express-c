#!/usr/bin/env bash

# Check for correct usage
if [ "$#" -lt 2 ]; then
  echo "Usage: $0 <output_file> <dir1> [dir2] ... [dirN]"
  exit 1
fi

output_file=$1
shift  # Shift the arguments to process directories
directories=("$@")

# Clear or create the output file
> "$output_file"

# Function to process files in directories
process_directory() {
  local dir=$1
  if [ -d "$dir" ]; then
    for file in "$dir"/*; do
      if [ -f "$file" ]; then
        xxd -i "$file" >> "$output_file"
      fi
    done
  else
    echo "Warning: Directory '$dir' does not exist or is not a directory. Skipping."
  fi
}

# Process each directory
for dir in "${directories[@]}"; do
  process_directory "$dir"
done

# Generate metadata
embedded_files_data=$(grep 'unsigned char' "$output_file" | awk '{print $3}' | sed 's/[\[\]]*//g' | awk '{printf("%s, ", $1)}' | sed 's/, $//')
embedded_files_lengths=$(grep 'unsigned int' "$output_file" | awk '{print $5}' | sed 's/[;]*//g' | awk '{printf("%s, ", $1)}' | sed 's/, $//')
embedded_files_names=$(grep 'unsigned char' "$output_file" | awk '{print $3}' | sed 's/[\[\]]*//g' | awk '{printf("\"%s\", ", $1)}' | sed 's/, $//')
embedded_files_count=$(grep 'unsigned int' "$output_file" | wc -l)

# Append the metadata to the output file
cat <<EOF >> "$output_file"

unsigned char *embeddedFilesData[] = { $embedded_files_data };
int embeddedFilesLengths[] = { $embedded_files_lengths };
char *embeddedFilesNames[] = { $embedded_files_names };
const int embeddedFilesCount = $embedded_files_count;

embedded_files_data_t embeddedFiles = { embeddedFilesData, embeddedFilesLengths, embeddedFilesNames, embeddedFilesCount };
EOF
