#!/usr/bin/env bash

set -euo pipefail

readonly REMOTE_INI="Documents/Gothic.ini"

device_id=""
bundle_id=""
save_slot=""
profile_path=""
duration_seconds=120
command_timeout_seconds=30
output_root=""
dry_run=0

run_dir=""
command_log=""
backup_ini=""
profile_applied=0
remote_pid=""
collector_pid=""
core_device_id=""
process_check_index=0

usage() {
  cat <<'EOF'
Run one bounded OpenGothic iOS performance-lab sample.

Usage:
  run-device-sample.sh \
    --device DEVICE_ID \
    --bundle BUNDLE_ID \
    --slot 1|4 \
    --profile PATH/TO/Gothic.ini \
    --duration-seconds SECONDS \
    --output-dir DIRECTORY

Required:
  --device DEVICE_ID        CoreDevice identifier or physical-device UDID.
  --bundle BUNDLE_ID        Installed performance-lab app bundle identifier.
  --slot 1|4                Existing save slot to load with -nomenu -save.
  --profile FILE            The single Gothic.ini profile for this sample.
  --output-dir DIRECTORY    Parent directory for the unique run evidence.

Optional:
  --duration-seconds N      Collection time after launch; 30-7200 (default: 120).
  --command-timeout-seconds N
                            Timeout for one-shot device operations; 5-300
                            (default: 30).
  --dry-run                 Validate arguments and print the plan without
                            contacting or changing the device.
  -h, --help                Show this help.

Safety and results:
  * The app must not already be running. The script never uses uninstall or
    --terminate-existing.
  * Documents/Gothic.ini is backed up before replacement and restored after
    the launched process stops. Save directories are never copied or changed.
  * stdout is captured to a file by one bounded devicectl collector; nothing is
    streamed continuously to the terminal.
  * A sample is accepted only when at least two world PERF v=2 records exist
    and the first and last both report thermal=nominal.
  * Exit 0: accepted sample. Exit 2: completed but rejected sample.
    Other non-zero values indicate an operational or restoration failure.
EOF
}

fail() {
  printf 'ERROR: %s\n' "$*" >&2
  exit 1
}

require_value() {
  if [[ $# -lt 2 || -z ${2:-} ]]; then
    fail "$1 requires a value"
  fi
}

set_once() {
  local option=$1
  local current=$2
  if [[ -n $current ]]; then
    fail "$option was provided more than once"
  fi
}

record_command() {
  [[ -n $command_log ]] || return 0
  {
    printf '$'
    printf ' %q' "$@"
    printf '\n'
  } >>"$command_log"
}

run_recorded() {
  record_command "$@"
  "$@"
}

remote_pid_is_running() {
  [[ -n $remote_pid && -n $core_device_id ]] || return 1

  process_check_index=$((process_check_index + 1))
  local check_json="$run_dir/process-check-${process_check_index}.json"
  local check_stdout="$run_dir/process-check-${process_check_index}.stdout"
  local check_stderr="$run_dir/process-check-${process_check_index}.stderr"

  if ! run_recorded xcrun devicectl device info processes \
      --device "$core_device_id" \
      --timeout "$command_timeout_seconds" \
      --json-output "$check_json" \
      >"$check_stdout" 2>"$check_stderr"; then
    return 2
  fi

  local jq_status=0
  if jq -e --argjson pid "$remote_pid" --arg prefix "$app_url" \
      'any(.result.runningProcesses[]?;
           .processIdentifier==$pid and ((.executable // "") | startswith($prefix)))' \
      "$check_json" >/dev/null; then
    return 0
  else
    jq_status=$?
  fi

  [[ $jq_status -eq 1 ]] && return 1
  return 2
}

terminate_remote_process() {
  [[ -n $remote_pid && -n $core_device_id ]] || return 0

  local terminate_json="$run_dir/terminate.json"
  if run_recorded xcrun devicectl device process terminate \
      --device "$core_device_id" \
      --pid "$remote_pid" \
      --timeout "$command_timeout_seconds" \
      --json-output "$terminate_json" \
      >"$run_dir/terminate.stdout" 2>"$run_dir/terminate.stderr"; then
    sleep 1
    if remote_pid_is_running; then
      : # Fall through to the exact-PID SIGKILL below.
    else
      local probe_status=$?
      if [[ $probe_status -eq 1 ]]; then
        remote_pid=""
        return 0
      fi
    fi
  else
    # A failed terminate commonly means that the app exited by itself. Confirm
    # absence before deciding whether a forceful exact-PID stop is needed.
    if remote_pid_is_running; then
      : # Still present; use the exact-PID fallback below.
    else
      local probe_status=$?
      if [[ $probe_status -eq 1 ]]; then
        remote_pid=""
        return 0
      fi
    fi
  fi

  local force_json="$run_dir/terminate-force.json"
  if run_recorded xcrun devicectl device process terminate \
      --device "$core_device_id" \
      --pid "$remote_pid" \
      --kill \
      --timeout "$command_timeout_seconds" \
      --json-output "$force_json" \
      >"$run_dir/terminate-force.stdout" 2>"$run_dir/terminate-force.stderr"; then
    remote_pid=""
    return 0
  fi

  if remote_pid_is_running; then
    return 1
  else
    local probe_status=$?
    if [[ $probe_status -eq 1 ]]; then
      remote_pid=""
      return 0
    fi
  fi
  return 1
}

stop_collector() {
  [[ -n $collector_pid ]] || return 0

  local attempt=0
  while kill -0 "$collector_pid" 2>/dev/null && [[ $attempt -lt 20 ]]; do
    sleep 1
    attempt=$((attempt + 1))
  done

  if kill -0 "$collector_pid" 2>/dev/null; then
    kill -TERM "$collector_pid" 2>/dev/null || true
  fi
  wait "$collector_pid" 2>/dev/null || true
  collector_pid=""
}

restore_original_ini() {
  [[ $profile_applied -eq 1 ]] || return 0
  [[ -f $backup_ini ]] || return 1

  local restore_json="$run_dir/restore-ini.json"
  local verify_json="$run_dir/verify-restored-ini.json"
  local restored_copy="$run_dir/restored-device-Gothic.ini"

  if ! run_recorded xcrun devicectl device copy to \
      --device "$core_device_id" \
      --source "$backup_ini" \
      --destination "$REMOTE_INI" \
      --domain-type appDataContainer \
      --domain-identifier "$bundle_id" \
      --timeout "$command_timeout_seconds" \
      --json-output "$restore_json" \
      >"$run_dir/restore-ini.stdout" 2>"$run_dir/restore-ini.stderr"; then
    return 1
  fi

  if ! run_recorded xcrun devicectl device copy from \
      --device "$core_device_id" \
      --source "$REMOTE_INI" \
      --destination "$restored_copy" \
      --domain-type appDataContainer \
      --domain-identifier "$bundle_id" \
      --timeout "$command_timeout_seconds" \
      --json-output "$verify_json" \
      >"$run_dir/verify-restored-ini.stdout" 2>"$run_dir/verify-restored-ini.stderr"; then
    return 1
  fi

  if ! cmp -s "$backup_ini" "$restored_copy"; then
    return 1
  fi

  profile_applied=0
  return 0
}

# Invoked indirectly by the EXIT trap.
# shellcheck disable=SC2329
cleanup() {
  local status=$?
  local process_stopped=1
  trap - EXIT
  set +e

  if [[ -n $remote_pid ]]; then
    if ! terminate_remote_process; then
      process_stopped=0
      status=1
    fi
  fi
  stop_collector

  if [[ $profile_applied -eq 1 ]]; then
    if [[ $process_stopped -eq 0 ]]; then
      printf '%s\n' \
        "RESTORATION DEFERRED: PID $remote_pid could still be running. Stop that exact process, then restore $backup_ini to $REMOTE_INI for $bundle_id." \
        >&2
    elif ! restore_original_ini; then
      printf '%s\n' \
        "RESTORATION FAILED: restore $backup_ini to $REMOTE_INI for $bundle_id before another run." \
        >&2
      status=1
    fi
  fi

  exit "$status"
}

trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'exit 129' HUP

while [[ $# -gt 0 ]]; do
  case "$1" in
    --device)
      require_value "$@"
      set_once "$1" "$device_id"
      device_id=$2
      shift 2
      ;;
    --bundle)
      require_value "$@"
      set_once "$1" "$bundle_id"
      bundle_id=$2
      shift 2
      ;;
    --slot)
      require_value "$@"
      set_once "$1" "$save_slot"
      save_slot=$2
      shift 2
      ;;
    --profile)
      require_value "$@"
      set_once "$1" "$profile_path"
      profile_path=$2
      shift 2
      ;;
    --duration-seconds)
      require_value "$@"
      duration_seconds=$2
      shift 2
      ;;
    --command-timeout-seconds)
      require_value "$@"
      command_timeout_seconds=$2
      shift 2
      ;;
    --output-dir)
      require_value "$@"
      set_once "$1" "$output_root"
      output_root=$2
      shift 2
      ;;
    --dry-run)
      dry_run=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      [[ $# -eq 0 ]] || fail "unexpected positional arguments: $*"
      ;;
    *)
      fail "unknown argument: $1"
      ;;
  esac
done

[[ -n $device_id ]] || fail "--device is required"
[[ -n $bundle_id ]] || fail "--bundle is required"
[[ -n $save_slot ]] || fail "--slot is required"
[[ -n $profile_path ]] || fail "--profile is required"
[[ -n $output_root ]] || fail "--output-dir is required"

[[ $device_id =~ ^[[:alnum:]-]+$ ]] || fail "--device must be an identifier, UDID or serial number"
[[ $bundle_id =~ ^[[:alnum:].-]+$ ]] || fail "invalid --bundle value"
[[ $save_slot == "1" || $save_slot == "4" ]] || fail "--slot must be 1 or 4"
[[ $duration_seconds =~ ^[1-9][0-9]*$ ]] || fail "--duration-seconds must be a positive decimal integer without leading zeroes"
[[ $command_timeout_seconds =~ ^[1-9][0-9]*$ ]] || fail "--command-timeout-seconds must be a positive decimal integer without leading zeroes"
((duration_seconds >= 30 && duration_seconds <= 7200)) || fail "--duration-seconds must be between 30 and 7200"
((command_timeout_seconds >= 5 && command_timeout_seconds <= 300)) || fail "--command-timeout-seconds must be between 5 and 300"
[[ -f $profile_path && -r $profile_path ]] || fail "profile is not a readable regular file: $profile_path"
grep -Eq '^[[:space:]]*\[IOS_PERF_LAB\][[:space:]]*$' "$profile_path" || \
  fail "profile does not contain an [IOS_PERF_LAB] section"

for required_command in xcrun jq shasum cmp mktemp; do
  command -v "$required_command" >/dev/null 2>&1 || fail "required command not found: $required_command"
done

if [[ $dry_run -eq 1 ]]; then
  printf 'DRY RUN: no device command will be executed.\n'
  printf 'Device: %s\n' "$device_id"
  printf 'Bundle: %s\n' "$bundle_id"
  printf 'Save slot: %s\n' "$save_slot"
  printf 'Profile: %s\n' "$profile_path"
  printf 'Duration: %s seconds\n' "$duration_seconds"
  printf 'Output root: %s\n' "$output_root"
  printf '%s\n' \
    'Plan: resolve physical device; require an installed, stopped app; back up Gothic.ini;' \
    'apply and verify the one profile; launch -nomenu -save; collect a bounded raw log;' \
    'terminate only the launched PID; restore and verify Gothic.ini; apply the thermal gate.'
  exit 0
fi

mkdir -p "$output_root"
profile_name=$(basename "$profile_path")
profile_tag=$(printf '%s' "${profile_name%.*}" | tr -c 'A-Za-z0-9._-' '_')
timestamp=$(date -u '+%Y%m%dT%H%M%SZ')
run_dir=$(mktemp -d "${output_root%/}/ios-perf-${profile_tag}-slot${save_slot}-${timestamp}.XXXXXX")
command_log="$run_dir/commands.log"
backup_ini="$run_dir/original-device-Gothic.ini"
profile_snapshot="$run_dir/input-Gothic.ini"
raw_log="$run_dir/raw-console.log"
collector_stderr="$run_dir/collector.stderr"
launch_json="$run_dir/launch.json"

cp -p "$profile_path" "$profile_snapshot"
profile_sha=$(shasum -a 256 "$profile_snapshot" | awk '{print $1}')
printf 'profile_sha256=%s\n' "$profile_sha" >"$run_dir/profile.sha256"
: >"$command_log"

devices_json="$run_dir/devices.json"
run_recorded xcrun devicectl list devices \
  --timeout "$command_timeout_seconds" \
  --json-output "$devices_json" \
  >"$run_dir/devices.stdout" 2>"$run_dir/devices.stderr"

device_matches=$(jq --arg id "$device_id" \
  '[.result.devices[]? | select(.identifier==$id or .properties.hardware.udid==$id or (.properties.hardware.serialNumber // "")==$id)] | length' \
  "$devices_json")
[[ $device_matches -eq 1 ]] || fail "--device matched $device_matches devices; expected exactly one"

core_device_id=$(jq -r --arg id "$device_id" \
  '.result.devices[] | select(.identifier==$id or .properties.hardware.udid==$id or (.properties.hardware.serialNumber // "")==$id) | .identifier' \
  "$devices_json")
hardware_udid=$(jq -r --arg id "$device_id" \
  '.result.devices[] | select(.identifier==$id or .properties.hardware.udid==$id or (.properties.hardware.serialNumber // "")==$id) | .properties.hardware.udid' \
  "$devices_json")
device_reality=$(jq -r --arg id "$device_id" \
  '.result.devices[] | select(.identifier==$id or .properties.hardware.udid==$id or (.properties.hardware.serialNumber // "")==$id) | .properties.hardware.reality' \
  "$devices_json")
device_state=$(jq -r --arg id "$device_id" \
  '.result.devices[] | select(.identifier==$id or .properties.hardware.udid==$id or (.properties.hardware.serialNumber // "")==$id) | .properties.connection.state' \
  "$devices_json")
[[ $device_reality == "physical" ]] || fail "performance samples require a physical device, got: $device_reality"
[[ $device_state == "connected" ]] || fail "device is not connected, state: $device_state"

apps_json="$run_dir/apps.json"
run_recorded xcrun devicectl device info apps \
  --device "$core_device_id" \
  --bundle-id "$bundle_id" \
  --require-container-access \
  --timeout "$command_timeout_seconds" \
  --json-output "$apps_json" \
  >"$run_dir/apps.stdout" 2>"$run_dir/apps.stderr"

app_count=$(jq '.result.apps | length' "$apps_json")
[[ $app_count -eq 1 ]] || fail "bundle is not installed with an accessible data container: $bundle_id"
app_url=$(jq -r '.result.apps[0].url' "$apps_json")
[[ $app_url == file://* ]] || fail "devicectl did not report a valid app URL"
[[ $(jq -r '.result.apps[0].containerAccessible' "$apps_json") == "true" ]] || \
  fail "the app data container is not accessible"

processes_before_json="$run_dir/processes-before.json"
run_recorded xcrun devicectl device info processes \
  --device "$core_device_id" \
  --timeout "$command_timeout_seconds" \
  --json-output "$processes_before_json" \
  >"$run_dir/processes-before.stdout" 2>"$run_dir/processes-before.stderr"
existing_pid=$(jq -r --arg prefix "$app_url" \
  '[.result.runningProcesses[]? | select((.executable // "") | startswith($prefix)) | .processIdentifier] | first // empty' \
  "$processes_before_json")
[[ -z $existing_pid ]] || fail "the target app is already running as PID $existing_pid; stop it without this script first"

backup_json="$run_dir/backup-ini.json"
run_recorded xcrun devicectl device copy from \
  --device "$core_device_id" \
  --source "$REMOTE_INI" \
  --destination "$backup_ini" \
  --domain-type appDataContainer \
  --domain-identifier "$bundle_id" \
  --timeout "$command_timeout_seconds" \
  --json-output "$backup_json" \
  >"$run_dir/backup-ini.stdout" 2>"$run_dir/backup-ini.stderr"
[[ -f $backup_ini ]] || fail "device Gothic.ini backup was not created; the profile was not applied"
original_sha=$(shasum -a 256 "$backup_ini" | awk '{print $1}')
printf 'original_sha256=%s\n' "$original_sha" >"$run_dir/original.sha256"

apply_json="$run_dir/apply-ini.json"
# Arm the restoration trap before the first operation that can alter Gothic.ini.
profile_applied=1
run_recorded xcrun devicectl device copy to \
  --device "$core_device_id" \
  --source "$profile_snapshot" \
  --destination "$REMOTE_INI" \
  --domain-type appDataContainer \
  --domain-identifier "$bundle_id" \
  --timeout "$command_timeout_seconds" \
  --json-output "$apply_json" \
  >"$run_dir/apply-ini.stdout" 2>"$run_dir/apply-ini.stderr"

applied_copy="$run_dir/applied-device-Gothic.ini"
verify_applied_json="$run_dir/verify-applied-ini.json"
run_recorded xcrun devicectl device copy from \
  --device "$core_device_id" \
  --source "$REMOTE_INI" \
  --destination "$applied_copy" \
  --domain-type appDataContainer \
  --domain-identifier "$bundle_id" \
  --timeout "$command_timeout_seconds" \
  --json-output "$verify_applied_json" \
  >"$run_dir/verify-applied-ini.stdout" 2>"$run_dir/verify-applied-ini.stderr"
cmp -s "$profile_snapshot" "$applied_copy" || fail "device Gothic.ini does not match the selected profile"

started_utc=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
collector_timeout=$((duration_seconds + command_timeout_seconds + 90))
record_command xcrun devicectl device process launch \
  --device "$core_device_id" \
  --console \
  --timeout "$collector_timeout" \
  --json-output "$launch_json" \
  -- "$bundle_id" -nomenu -save "$save_slot"
xcrun devicectl device process launch \
  --device "$core_device_id" \
  --console \
  --timeout "$collector_timeout" \
  --json-output "$launch_json" \
  -- "$bundle_id" -nomenu -save "$save_slot" \
  >"$raw_log" 2>"$collector_stderr" &
collector_pid=$!

launch_probe=0
while [[ $launch_probe -lt 20 ]]; do
  if ! kill -0 "$collector_pid" 2>/dev/null; then
    break
  fi
  sleep 1
  processes_live_json="$run_dir/processes-live-${launch_probe}.json"
  record_command xcrun devicectl device info processes \
    --device "$core_device_id" \
    --timeout "$command_timeout_seconds" \
    --json-output "$processes_live_json"
  if xcrun devicectl device info processes \
      --device "$core_device_id" \
      --timeout "$command_timeout_seconds" \
      --json-output "$processes_live_json" \
      >"$run_dir/processes-live-${launch_probe}.stdout" \
      2>"$run_dir/processes-live-${launch_probe}.stderr"; then
    remote_pid=$(jq -r --arg prefix "$app_url" \
      '[.result.runningProcesses[]? | select((.executable // "") | startswith($prefix)) | .processIdentifier] | first // empty' \
      "$processes_live_json")
    [[ -z $remote_pid ]] || break
  fi
  launch_probe=$((launch_probe + 1))
done
[[ -n $remote_pid ]] || fail "the launched app PID was not observed; inspect raw-console.log and collector.stderr"
printf '%s\n' "$remote_pid" >"$run_dir/launched.pid"

ended_early=0
elapsed=0
while [[ $elapsed -lt $duration_seconds ]]; do
  if ! kill -0 "$collector_pid" 2>/dev/null; then
    ended_early=1
    break
  fi
  step=5
  remaining=$((duration_seconds - elapsed))
  if [[ $remaining -lt $step ]]; then
    step=$remaining
  fi
  sleep "$step"
  elapsed=$((elapsed + step))
done

if ! kill -0 "$collector_pid" 2>/dev/null; then
  ended_early=1
fi

if ! terminate_remote_process; then
  fail "could not confirm termination of the exact launched PID $remote_pid"
fi
stop_collector
ended_utc=$(date -u '+%Y-%m-%dT%H:%M:%SZ')

if ! restore_original_ini; then
  fail "could not restore and verify the original Gothic.ini; use $backup_ini for manual recovery"
fi

all_telemetry="$run_dir/perf-telemetry.log"
world_telemetry="$run_dir/world-telemetry.log"
# devicectl's console bridge uses a PTY and can hard-wrap a PERF record in the
# middle of a field. Preserve raw-console.log byte-for-byte, but reconstruct a
# separate analysis copy through the final entitlement_present field.
tr -d '\r' <"$raw_log" | awk '
  /^PERF v=2 / {
    if (collecting && length(record)>0)
      print record
    record=$0
    collecting=1
    if ($0 ~ /entitlement_present=[^[:space:]]+$/) {
      print record
      record=""
      collecting=0
    }
    next
  }
  collecting {
    record=record $0
    if ($0 ~ /entitlement_present=[^[:space:]]+$/) {
      print record
      record=""
      collecting=0
    }
  }
  END {
    if (collecting && length(record)>0)
      print record
  }
' >"$all_telemetry"
grep '^PERF v=2 scene=world ' "$all_telemetry" >"$world_telemetry" || true
sample_count=$(awk 'END { print NR+0 }' "$world_telemetry")
thermal_start="missing"
thermal_end="missing"
if [[ $sample_count -gt 0 ]]; then
  first_perf=$(sed -n '1p' "$world_telemetry")
  last_perf=$(tail -n 1 "$world_telemetry")
  thermal_start=$(printf '%s\n' "$first_perf" | awk '{for(i=1;i<=NF;i++) if($i ~ /^thermal=/) {sub(/^thermal=/,"",$i); print $i; exit}}')
  thermal_end=$(printf '%s\n' "$last_perf" | awk '{for(i=1;i<=NF;i++) if($i ~ /^thermal=/) {sub(/^thermal=/,"",$i); print $i; exit}}')
  thermal_start=${thermal_start:-missing}
  thermal_end=${thermal_end:-missing}
fi
printf '%s\n' "$thermal_start" >"$run_dir/thermal-start.txt"
printf '%s\n' "$thermal_end" >"$run_dir/thermal-end.txt"

accepted=true
reason="accepted"
if [[ $ended_early -eq 1 ]]; then
  accepted=false
  reason="process_ended_before_requested_duration"
elif [[ $sample_count -lt 2 ]]; then
  accepted=false
  reason="fewer_than_two_world_perf_samples"
elif [[ $thermal_start != "nominal" ]]; then
  accepted=false
  reason="start_thermal_not_nominal"
elif [[ $thermal_end != "nominal" ]]; then
  accepted=false
  reason="end_thermal_not_nominal"
elif ! grep -q '^PERF_LAB v=1 ' "$raw_log"; then
  accepted=false
  reason="performance_lab_marker_missing"
fi

git_head="unknown"
repo_root=$(cd "$(dirname "$0")/../../.." && pwd)
if git -C "$repo_root" rev-parse --verify HEAD >/dev/null 2>&1; then
  git_head=$(git -C "$repo_root" rev-parse HEAD)
fi
devicectl_version=$(xcrun devicectl --version 2>&1 | tr '\n' ' ')

jq -n \
  --argjson accepted "$accepted" \
  --arg reason "$reason" \
  --arg startedUtc "$started_utc" \
  --arg endedUtc "$ended_utc" \
  --arg deviceIdentifier "$core_device_id" \
  --arg deviceUdid "$hardware_udid" \
  --arg bundleIdentifier "$bundle_id" \
  --arg saveSlot "$save_slot" \
  --arg profile "$profile_name" \
  --arg profileSha256 "$profile_sha" \
  --arg originalIniSha256 "$original_sha" \
  --arg thermalStart "$thermal_start" \
  --arg thermalEnd "$thermal_end" \
  --arg gitHead "$git_head" \
  --arg devicectlVersion "$devicectl_version" \
  --argjson durationSeconds "$duration_seconds" \
  --argjson worldPerfSamples "$sample_count" \
  '{
    schemaVersion: 1,
    accepted: $accepted,
    reason: $reason,
    startedUtc: $startedUtc,
    endedUtc: $endedUtc,
    deviceIdentifier: $deviceIdentifier,
    deviceUdid: $deviceUdid,
    bundleIdentifier: $bundleIdentifier,
    saveSlot: $saveSlot,
    durationSeconds: $durationSeconds,
    profile: $profile,
    profileSha256: $profileSha256,
    originalIniSha256: $originalIniSha256,
    thermalStart: $thermalStart,
    thermalEnd: $thermalEnd,
    worldPerfSamples: $worldPerfSamples,
    gitHead: $gitHead,
    devicectlVersion: $devicectlVersion
  }' >"$run_dir/result.json"

if [[ $accepted == "true" ]]; then
  printf 'ACCEPTED: %s\n' "$run_dir"
  exit 0
fi

printf 'REJECTED (%s): %s\n' "$reason" "$run_dir" >&2
exit 2
