#!/usr/bin/env bash

set -e

if [[ $# != 2 ]] ; then
  printf "Usage:\n\tcreate_icns.sh path/to/source/image.png output_path\n"
  exit 1
fi

source_image_path="$1"
if [[ ! -f "${source_image_path}" ]] ; then
  printf "The following image was not found: ${source_image_path}\n"
  exit 1
fi

dest_icns_path="$2"

temp_folder="/tmp/Multiplier.iconset"
if [[ -d "${temp_folder}" ]] ; then
  rm -rf "${temp_folder}"
fi

mkdir "${temp_folder}"

format_list=(16 32 64 128 256 512)

for format in "${format_list[@]}" ; do
  sips -z ${format} ${format} "${source_image_path}" --out "${temp_folder}/icon_${format}x${format}.png"

  doubled=$((format * 2))

  sips -z ${doubled} ${doubled} "${source_image_path}" --out "${temp_folder}/icon_${format}x${format}@2x.png"
done

iconutil -c icns "${temp_folder}"
mv "${temp_folder}/../Multiplier.icns" "${dest_icns_path}"
rm -rf "${temp_folder}"
