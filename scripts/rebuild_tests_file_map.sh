#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOCS_DIR="${SCRIPT_DIR}/../docs"

INDEX_FILE="${DOCS_DIR}/tests_file_map_index.md"
TARGET_FILE="${DOCS_DIR}/tests_file_map.md"

SHARDS=(
  "tests_file_map_shard_01_build_and_foundations.md"
  "tests_file_map_shard_02_runtime_config_and_backend_interfaces.md"
  "tests_file_map_shard_03_packet_parsing_and_reorder.md"
  "tests_file_map_shard_04_video_frame_assembly_and_pipeline.md"
  "tests_file_map_shard_05_video_signaling_and_runtime_projection.md"
  "tests_file_map_shard_06_video_receiver_timing_and_timestamp_mapping.md"
  "tests_file_map_shard_07_video_sdp_ingestion_and_transport_boundary.md"
  "tests_file_map_shard_08_audio_signaling_bootstrap_and_channel_order.md"
  "tests_file_map_shard_09_audio_sdp_ingestion.md"
  "tests_file_map_shard_10_audio_packet_pipeline_and_timestamping.md"
  "tests_file_map_shard_11_socket_runtime_and_concrete_backends.md"
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