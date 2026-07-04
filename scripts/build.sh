#!/usr/bin/env bash
# Bash equivalent of scripts/package.ps1 for Linux/macOS build machines
# (including this repo's GitHub Actions workflow). See scripts/package.ps1
# for the canonical, more thoroughly commented implementation; keep both in
# sync if the CMake target names or cache variables change.
set -euo pipefail

ABI="${1:-arm64-v8a}"
BUILD_ROOT="${BUILD_ROOT:-build}"
ANDROID_PLATFORM="${ANDROID_PLATFORM:-android-28}"
GENERATOR="${GENERATOR:-Ninja}"
PRELOADER_ROOT="${LEVI_PRELOADER_ROOT:-}"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

resolve_ndk_home() {
	if [[ -n "${NDK_HOME:-}" && -d "${NDK_HOME}" ]]; then
		echo "${NDK_HOME}"
		return
	fi
	if [[ -n "${ANDROID_HOME:-}" ]]; then
		if [[ -d "${ANDROID_HOME}/ndk/28.2.13676358" ]]; then
			echo "${ANDROID_HOME}/ndk/28.2.13676358"
			return
		fi
		if [[ -d "${ANDROID_HOME}/ndk" ]]; then
			local candidate
			candidate="$(ls -1 "${ANDROID_HOME}/ndk" | sort -r | head -n1)"
			if [[ -n "${candidate}" ]]; then
				echo "${ANDROID_HOME}/ndk/${candidate}"
				return
			fi
		fi
	fi
	if [[ -n "${ANDROID_NDK_HOME:-}" && -d "${ANDROID_NDK_HOME}" ]]; then
		echo "${ANDROID_NDK_HOME}"
		return
	fi
	echo "Android NDK not found. Set ANDROID_NDK_HOME or ANDROID_HOME." >&2
	exit 1
}

generate_config() {
	local config_build_dir="${repo_root}/${BUILD_ROOT}-config"
	local preloader_args=()
	[[ -n "${PRELOADER_ROOT}" ]] && preloader_args+=("-DLEVI_PRELOADER_ROOT=${PRELOADER_ROOT}")

	# Redirect to stderr: this function's stdout is captured via $(...) by
	# the caller, and only the final path below should end up in that
	# capture -- otherwise cmake's own status output gets appended to the
	# path string (this broke CI: LEVI_PACKAGE_CONFIG_DIR ended up
	# containing embedded newlines and compiler-detection log lines).
	cmake -S "${repo_root}" -B "${config_build_dir}" -G "${GENERATOR}" "${preloader_args[@]}" >&2
	cmake --build "${config_build_dir}" --target levi_generate_config >&2
	echo "${config_build_dir}/generated-config"
}

build_abi() {
	local target_abi="$1"
	local ndk_home
	ndk_home="$(resolve_ndk_home)"
	local toolchain="${ndk_home}/build/cmake/android.toolchain.cmake"
	local build_dir="${repo_root}/${BUILD_ROOT}-${target_abi}"
	local generated_config_dir
	generated_config_dir="$(generate_config)"
	local preloader_args=()
	[[ -n "${PRELOADER_ROOT}" ]] && preloader_args+=("-DLEVI_PRELOADER_ROOT=${PRELOADER_ROOT}")

	cmake -S "${repo_root}" -B "${build_dir}" -G "${GENERATOR}" \
		-DCMAKE_TOOLCHAIN_FILE="${toolchain}" \
		-DANDROID_ABI="${target_abi}" \
		-DANDROID_PLATFORM="${ANDROID_PLATFORM}" \
		-DANDROID_STL="c++_shared" \
		"${preloader_args[@]}" \
		-DLEVI_PACKAGE_CONFIG_DIR="${generated_config_dir}"

	cmake --build "${build_dir}" --target levi_package
}

if [[ "${ABI}" == "all" ]]; then
	build_abi "arm64-v8a"
	build_abi "armeabi-v7a"
else
	build_abi "${ABI}"
fi
