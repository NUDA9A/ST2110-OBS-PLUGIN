#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOCS_DIR="${SCRIPT_DIR}/../docs"

INDEX_FILE="${DOCS_DIR}/code_map_index.md"
TARGET_FILE="${DOCS_DIR}/code_map.md"

SHARDS=(
  "code_map_shard_01_build_and_entrypoints.md"
  "code_map_shard_02_core_interfaces_and_common_types.md"
  "code_map_shard_03_packet_parsing_and_reorder.md"
  "code_map_shard_04_video_pipeline_and_frame_assembly.md"
  "code_map_shard_05_video_signaling_bootstrap_and_timing.md"
  "code_map_shard_06_video_sdp_ingestion.md"
  "code_map_shard_07_audio_signaling_bootstrap_and_channel_order.md"
  "code_map_shard_08_audio_pipeline_and_storage.md"
  "code_map_shard_09_audio_sdp_ingestion.md"
  "code_map_shard_10_socket_runtime_and_media_backends.md"
)

if [[ ! -f "${INDEX_FILE}" ]]; then
  echo "Missing index file: ${INDEX_FILE}" >&2
  exit 1
fi

for shard in "${SHARDS[@]}"; do
  if [[ ! -f "${DOCS_DIR}/${shard}" ]]; then
    echo "Missing shard file: ${DOCS_DIR}/${shard}" >&2
    exit 1
  fi
done

cat "${INDEX_FILE}" > "${TARGET_FILE}"

for shard in "${SHARDS[@]}"; do
  printf '\n\n' >> "${TARGET_FILE}"
  cat "${DOCS_DIR}/${shard}" >> "${TARGET_FILE}"
done

echo "Rebuilt ${TARGET_FILE}"