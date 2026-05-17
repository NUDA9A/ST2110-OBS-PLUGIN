#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_NAME="$(basename "$0")"

REPO_DIR="$(pwd)"
BUILD_DIR=""
BUILD_TYPE="RelWithDebInfo"
JOBS="$(nproc 2>/dev/null || echo 4)"

DEPS_DIR=""
RUN_APT=1
INSTALL_MTL=1
FORCE_REBUILD_DPDK=0
FORCE_REBUILD_MTL=0

BUILD_PLUGIN=1
BUILD_MTL_RX_WORKER=1
BUILD_SEND_APP=1
INSTALL_PLUGIN=1
INSTALL_MTL_RX_WORKER=1
INSTALL_SEND_APP=1
MTL_DEV_KERNEL_SOCKET=0

REQUIRE_NDI=1
START_AVAHI=1

CONFIGURE_HUGEPAGES=1
PERSIST_HUGEPAGES=1
HUGEPAGES="2048"

CONFIGURE_VFIO_PERMS=1
CONFIGURE_INTEL_IOMMU_GRUB=0

OBS_PLUGIN_DIR="${HOME}/.config/obs-studio/plugins/st2110_obs"
APP_INSTALL_DIR="${HOME}/.local/bin"

NDI_INCLUDE_DIR_ARG="${NDI_INCLUDE_DIR:-}"
NDI_RUNTIME_DIR_ARG="${NDI_RUNTIME_DIR_V6:-}"

MTL_REPO_URL="https://github.com/OpenVisualCloud/Media-Transport-Library.git"
MTL_REF="v26.01"
MTL_SOURCE_DIR=""

DPDK_REPO_URL="https://github.com/DPDK/dpdk.git"
DPDK_REF="v25.11"
DPDK_PATCH_SERIES="25.11"
DPDK_SOURCE_DIR=""

PKG_CONFIG_PATH_EXTRA=""
CMAKE_PREFIX_PATH_EXTRA=""

BIND_PMD_BDFS=()
CREATE_E800_VFS_BDFS=()

usage() {
    cat <<EOF
Usage:
  ${SCRIPT_NAME} [options]

General:
  --repo-dir PATH                 Repository root. Default: current directory.
  --deps-dir PATH                 Dependency source root. Default: <repo>/.deps
  --build-dir PATH                Project build directory. Default: <repo>/build/st2110-linux
  --build-type TYPE               CMake build type. Default: RelWithDebInfo
  --jobs N                        Build parallelism. Default: nproc

NDI:
  --ndi-include-dir PATH          Directory containing Processing.NDI.Lib.h
  --ndi-runtime-dir PATH          Directory containing libndi.so.6/libndi.so
  --allow-no-ndi                  Do not require NDI. Sender app build is disabled if NDI header is missing.

MTL / DPDK:
  --mtl-source-dir PATH           Media Transport Library source dir. Default: <deps>/Media-Transport-Library
  --mtl-ref REF                   Optional MTL git ref to checkout.
  --dpdk-source-dir PATH          DPDK source dir. Default: <deps>/dpdk
  --dpdk-ref REF                  DPDK git ref. Default: v25.11
  --dpdk-patch-series NAME        MTL DPDK patch series dir. Default: 25.11
  --mtl-pkg-config-dir PATH       Extra pkg-config dir; prepended to PKG_CONFIG_PATH
  --no-install-mtl                Do not clone/build/install DPDK/MTL; require existing pkg-config mtl.
  --force-rebuild-dpdk            Rebuild/reinstall DPDK even if libdpdk.pc is visible.
  --force-rebuild-mtl             Rebuild/reinstall MTL even if mtl.pc is visible.
  --mtl-dev-kernel-socket      Build project MTL RX backend with MTL_PMD_KERNEL_SOCKET
                               and kernel:<ifname> port projection for local VM/dev tests.
                               MTL/DPDK libraries are still required.

System runtime:
  --hugepages N                   Set vm.nr_hugepages. Default: 2048
  --no-hugepages                  Do not configure hugepages.
  --no-persist-hugepages          Do not write /etc/sysctl.d/99-st2110-mtl-hugepages.conf
  --no-vfio-permissions           Do not create vfio group/udev rule.
  --configure-intel-iommu-grub    Append intel_iommu=on iommu=pt to GRUB_CMDLINE_LINUX_DEFAULT
                                  and run update-grub. Reboot is required.

NIC setup:
  --create-e800-vfs-bdf BDF       Run MTL script/nicctl.sh create_vf <BDF>. Can be repeated.
  --bind-pmd-bdf BDF              Run MTL script/nicctl.sh bind_pmd <BDF>. Can be repeated.

Build outputs:
  --cmake-prefix-path PATH        Extra CMAKE_PREFIX_PATH, useful for custom OBS/libobs install.
  --obs-plugin-dir PATH           OBS plugin install dir.
                                  Default: ~/.config/obs-studio/plugins/st2110_obs
  --app-install-dir PATH          Sender app install dir. Default: ~/.local/bin

Build selection:
  --no-apt                        Do not install apt packages.
  --no-avahi-start                Do not enable/start avahi-daemon.
  --no-plugin                     Do not build/install OBS plugin.
  --no-send-app                   Do not build/install st2110_mtl_send_test.
  --no-install-plugin             Build plugin but do not install it.
  --no-install-send-app           Build sender app but do not install it.

Examples:
  ${SCRIPT_NAME} \\
    --ndi-include-dir /opt/ndi/include \\
    --ndi-runtime-dir /opt/ndi/lib/x86_64-linux-gnu

  ${SCRIPT_NAME} \\
    --ndi-include-dir /opt/ndi/include \\
    --ndi-runtime-dir /opt/ndi/lib/x86_64-linux-gnu \\
    --create-e800-vfs-bdf 0000:af:00.0

  ${SCRIPT_NAME} \\
    --ndi-include-dir /opt/ndi/include \\
    --ndi-runtime-dir /opt/ndi/lib/x86_64-linux-gnu \\
    --bind-pmd-bdf 0000:af:01.0
EOF
}

log() {
    printf '\033[1;34m[st2110-install]\033[0m %s\n' "$*"
}

warn() {
    printf '\033[1;33m[st2110-install][warn]\033[0m %s\n' "$*" >&2
}

die() {
    printf '\033[1;31m[st2110-install][error]\033[0m %s\n' "$*" >&2
    exit 1
}

have_cmd() {
    command -v "$1" >/dev/null 2>&1
}

run() {
    log "+ $*"
    "$@"
}

sudo_cmd() {
    if [[ "${EUID}" -eq 0 ]]; then
        "$@"
    else
        sudo "$@"
    fi
}

sudo_env_cmd() {
    if [[ "${EUID}" -eq 0 ]]; then
        env "$@"
    else
        sudo -E env "$@"
    fi
}

append_unique_colon_path() {
    local var_name="$1"
    local value="$2"
    local current="${!var_name:-}"

    if [[ -z "$value" ]]; then
        return
    fi

    case ":${current}:" in
        *":${value}:"*) ;;
        *)
            if [[ -z "$current" ]]; then
                export "${var_name}=${value}"
            else
                export "${var_name}=${value}:${current}"
            fi
            ;;
    esac
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --repo-dir)
            REPO_DIR="$2"
            shift 2
            ;;
        --deps-dir)
            DEPS_DIR="$2"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --build-type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        --ndi-include-dir)
            NDI_INCLUDE_DIR_ARG="$2"
            shift 2
            ;;
        --ndi-runtime-dir)
            NDI_RUNTIME_DIR_ARG="$2"
            shift 2
            ;;
        --mtl-source-dir)
            MTL_SOURCE_DIR="$2"
            shift 2
            ;;
        --mtl-ref)
            MTL_REF="$2"
            shift 2
            ;;
        --dpdk-source-dir)
            DPDK_SOURCE_DIR="$2"
            shift 2
            ;;
        --dpdk-ref)
            DPDK_REF="$2"
            shift 2
            ;;
        --dpdk-patch-series)
            DPDK_PATCH_SERIES="$2"
            shift 2
            ;;
        --mtl-pkg-config-dir)
            PKG_CONFIG_PATH_EXTRA="$2"
            shift 2
            ;;
        --cmake-prefix-path)
            CMAKE_PREFIX_PATH_EXTRA="$2"
            shift 2
            ;;
        --obs-plugin-dir)
            OBS_PLUGIN_DIR="$2"
            shift 2
            ;;
        --app-install-dir)
            APP_INSTALL_DIR="$2"
            shift 2
            ;;
        --hugepages)
            HUGEPAGES="$2"
            shift 2
            ;;
        --no-hugepages)
            CONFIGURE_HUGEPAGES=0
            shift
            ;;
        --no-persist-hugepages)
            PERSIST_HUGEPAGES=0
            shift
            ;;
        --no-vfio-permissions)
            CONFIGURE_VFIO_PERMS=0
            shift
            ;;
        --configure-intel-iommu-grub)
            CONFIGURE_INTEL_IOMMU_GRUB=1
            shift
            ;;
        --create-e800-vfs-bdf)
            CREATE_E800_VFS_BDFS+=("$2")
            shift 2
            ;;
        --bind-pmd-bdf)
            BIND_PMD_BDFS+=("$2")
            shift 2
            ;;
        --no-install-mtl)
            INSTALL_MTL=0
            shift
            ;;
        --force-rebuild-dpdk)
            FORCE_REBUILD_DPDK=1
            shift
            ;;
        --force-rebuild-mtl)
            FORCE_REBUILD_MTL=1
            shift
            ;;
        --no-apt)
            RUN_APT=0
            shift
            ;;
        --no-avahi-start)
            START_AVAHI=0
            shift
            ;;
        --no-plugin)
            BUILD_PLUGIN=0
            BUILD_MTL_RX_WORKER=0
            INSTALL_PLUGIN=0
            INSTALL_MTL_RX_WORKER=0
            shift
            ;;
        --no-send-app)
            BUILD_SEND_APP=0
            INSTALL_SEND_APP=0
            shift
            ;;
        --no-install-plugin)
            INSTALL_PLUGIN=0
            shift
            ;;
        --no-install-send-app)
            INSTALL_SEND_APP=0
            shift
            ;;
        --allow-no-ndi)
            REQUIRE_NDI=0
            shift
            ;;
        --mtl-dev-kernel-socket)
            MTL_DEV_KERNEL_SOCKET=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            die "Unknown argument: $1"
            ;;
    esac
done

REPO_DIR="$(cd "$REPO_DIR" && pwd)"

if [[ -z "$DEPS_DIR" ]]; then
    DEPS_DIR="${REPO_DIR}/.deps"
fi

mkdir -p "$DEPS_DIR"
DEPS_DIR="$(cd "$DEPS_DIR" && pwd)"

if [[ -z "$BUILD_DIR" ]]; then
    BUILD_DIR="${REPO_DIR}/build/st2110-linux"
fi
mkdir -p "$BUILD_DIR"
BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"

if [[ -z "$MTL_SOURCE_DIR" ]]; then
    MTL_SOURCE_DIR="${DEPS_DIR}/Media-Transport-Library"
fi

if [[ -z "$DPDK_SOURCE_DIR" ]]; then
    DPDK_SOURCE_DIR="${DEPS_DIR}/dpdk"
fi

if [[ ! -f "${REPO_DIR}/CMakeLists.txt" ]]; then
    die "Repository root is invalid: ${REPO_DIR}"
fi

if grep -q "add_subdirectory(tests)" "${REPO_DIR}/CMakeLists.txt" && [[ ! -f "${REPO_DIR}/tests/CMakeLists.txt" ]]; then
    die "Root CMakeLists.txt calls add_subdirectory(tests), but tests/CMakeLists.txt is missing.

Patch root CMakeLists.txt:

if(EXISTS \"\${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt\")
    enable_testing()
    add_subdirectory(tests)
endif()"
fi

install_apt_packages() {
    if [[ "$RUN_APT" -eq 0 ]]; then
        log "Skipping apt dependency installation"
        return
    fi

    if ! have_cmd apt-get; then
        warn "apt-get is not available; skipping system package installation"
        return
    fi

    log "Installing system packages through apt"

    sudo_cmd apt-get update

    local requested=(
        build-essential
        ca-certificates
        cmake
        file
        git
        ninja-build
        pkg-config
        python3
        python3-pip
        python3-pyelftools
        gcc
        g++
        meson
        libnuma-dev
        libjson-c-dev
        libpcap-dev
        libgtest-dev
        libssl-dev
        systemtap-sdt-dev
        llvm
        clang
        flex
        byacc
        linux-headers-$(uname -r)
        avahi-daemon
        libavahi-client3
        libavahi-common3
        libavahi-client-dev
        obs-studio
        libobs-dev
    )

    local installable=()
    local missing=()

    for pkg in "${requested[@]}"; do
        if apt-cache show "$pkg" >/dev/null 2>&1; then
            installable+=("$pkg")
        else
            missing+=("$pkg")
        fi
    done

    if [[ "${#installable[@]}" -gt 0 ]]; then
        sudo_cmd apt-get install -y "${installable[@]}"
    fi

    if [[ "${#missing[@]}" -gt 0 ]]; then
        warn "These apt packages were not found in configured repositories: ${missing[*]}"
        warn "If CMake cannot find libobs, install OBS development files manually or pass --cmake-prefix-path."
    fi

    if [[ "$START_AVAHI" -eq 1 ]] && have_cmd systemctl; then
        if systemctl list-unit-files avahi-daemon.service >/dev/null 2>&1; then
            log "Enabling and starting avahi-daemon"
            sudo_cmd systemctl enable --now avahi-daemon || warn "Could not start avahi-daemon"
        else
            warn "avahi-daemon.service is not available"
        fi
    fi
}

configure_pkg_config_paths() {
    append_unique_colon_path PKG_CONFIG_PATH "/usr/local/lib64/pkgconfig"
    append_unique_colon_path PKG_CONFIG_PATH "/usr/local/lib/pkgconfig"
    append_unique_colon_path PKG_CONFIG_PATH "/usr/local/lib/x86_64-linux-gnu/pkgconfig"

    if [[ -n "$PKG_CONFIG_PATH_EXTRA" ]]; then
        append_unique_colon_path PKG_CONFIG_PATH "$PKG_CONFIG_PATH_EXTRA"
    fi

    if [[ -n "$CMAKE_PREFIX_PATH_EXTRA" ]]; then
        append_unique_colon_path CMAKE_PREFIX_PATH "$CMAKE_PREFIX_PATH_EXTRA"
    fi
}

configure_hugepages() {
    if [[ "$CONFIGURE_HUGEPAGES" -eq 0 ]]; then
        log "Skipping hugepages configuration"
        return
    fi

    if ! [[ "$HUGEPAGES" =~ ^[0-9]+$ ]] || [[ "$HUGEPAGES" -eq 0 ]]; then
        die "--hugepages must be a positive integer"
    fi

    log "Configuring hugepages: vm.nr_hugepages=${HUGEPAGES}"

    sudo_cmd sysctl -w "vm.nr_hugepages=${HUGEPAGES}"

    if [[ "$PERSIST_HUGEPAGES" -eq 1 ]]; then
        printf 'vm.nr_hugepages=%s\n' "$HUGEPAGES" | sudo_cmd tee /etc/sysctl.d/99-st2110-mtl-hugepages.conf >/dev/null
        log "Persisted hugepages config: /etc/sysctl.d/99-st2110-mtl-hugepages.conf"
    fi

    sudo_cmd mkdir -p /dev/hugepages

    if ! mountpoint -q /dev/hugepages; then
        sudo_cmd mount -t hugetlbfs nodev /dev/hugepages || warn "Could not mount hugetlbfs at /dev/hugepages"
    fi

    if [[ -f /proc/meminfo ]]; then
        grep -E 'HugePages_Total|HugePages_Free|Hugepagesize' /proc/meminfo || true
    fi
}

configure_vfio_permissions() {
    if [[ "$CONFIGURE_VFIO_PERMS" -eq 0 ]]; then
        log "Skipping VFIO permission setup"
        return
    fi

    log "Configuring VFIO permissions"

    if ! getent group vfio >/dev/null 2>&1; then
        if getent group 2110 >/dev/null 2>&1; then
            sudo_cmd groupadd vfio
        else
            sudo_cmd groupadd -g 2110 vfio
        fi
    fi

    sudo_cmd usermod -aG vfio "$USER" || warn "Could not add ${USER} to vfio group"

    printf 'SUBSYSTEM=="vfio", GROUP="vfio", MODE="0660"\n' | sudo_cmd tee /etc/udev/rules.d/10-vfio.rules >/dev/null

    sudo_cmd modprobe vfio || true
    sudo_cmd modprobe vfio-pci || true
    sudo_cmd udevadm control --reload-rules || true
    sudo_cmd udevadm trigger || true

    warn "If this user was just added to the vfio group, log out/in before non-root MTL runs."
}

configure_intel_iommu_grub() {
    if [[ "$CONFIGURE_INTEL_IOMMU_GRUB" -eq 0 ]]; then
        if [[ ! -d /sys/kernel/iommu_groups ]] || [[ -z "$(find /sys/kernel/iommu_groups -mindepth 1 -maxdepth 1 2>/dev/null | head -n 1)" ]]; then
            warn "No IOMMU groups detected. DPDK/VFIO may not work until IOMMU is enabled in BIOS and kernel."
            warn "To let this script append Intel kernel args, rerun with --configure-intel-iommu-grub and reboot."
        fi
        return
    fi

    if [[ ! -f /etc/default/grub ]]; then
        warn "/etc/default/grub not found; cannot configure Intel IOMMU kernel args"
        return
    fi

    log "Configuring Intel IOMMU GRUB kernel args"

    sudo_cmd cp /etc/default/grub /etc/default/grub.st2110-backup.$(date +%Y%m%d%H%M%S)

    sudo_cmd python3 - <<'PY'
from pathlib import Path
path = Path("/etc/default/grub")
text = path.read_text()
needle = "GRUB_CMDLINE_LINUX_DEFAULT="
args_to_add = ["intel_iommu=on", "iommu=pt"]

lines = []
changed = False

for line in text.splitlines():
    if line.startswith(needle):
        prefix, value = line.split("=", 1)
        quote = '"'
        value = value.strip()
        if value.startswith("'") and value.endswith("'"):
            quote = "'"
            content = value[1:-1]
        elif value.startswith('"') and value.endswith('"'):
            content = value[1:-1]
        else:
            content = value

        parts = content.split()
        for arg in args_to_add:
            if arg not in parts:
                parts.append(arg)
                changed = True

        line = f'{prefix}={quote}{" ".join(parts)}{quote}'
    lines.append(line)

if changed:
    path.write_text("\n".join(lines) + "\n")
PY

    if have_cmd update-grub; then
        sudo_cmd update-grub
        warn "IOMMU kernel args updated. Reboot is required."
    else
        warn "update-grub not found. Update bootloader manually and reboot."
    fi
}

clone_or_update_repo() {
    local url="$1"
    local dir="$2"
    local ref="$3"
    local name="$4"

    if [[ ! -d "$dir/.git" ]]; then
        log "Cloning ${name}: ${url} -> ${dir}"
        run git clone "$url" "$dir"
    else
        log "${name} source already exists: ${dir}"
    fi

    if [[ -n "$ref" ]]; then
        log "Checking out ${name} ref: ${ref}"
        run git -C "$dir" fetch --all --tags
        run git -C "$dir" checkout "$ref"
    fi
}

build_and_install_dpdk() {
    if pkg-config --exists libdpdk && [[ "$FORCE_REBUILD_DPDK" -eq 0 ]]; then
        log "DPDK already visible through pkg-config: $(pkg-config --modversion libdpdk 2>/dev/null || echo found)"
        return
    fi

    clone_or_update_repo "$DPDK_REPO_URL" "$DPDK_SOURCE_DIR" "$DPDK_REF" "DPDK"

    local marker="${DPDK_SOURCE_DIR}/.st2110_mtl_patches_applied_${DPDK_PATCH_SERIES}"

    if [[ ! -f "$marker" ]]; then
        local patch_dir="${MTL_SOURCE_DIR}/patches/dpdk/${DPDK_PATCH_SERIES}"

        if [[ ! -d "$patch_dir" ]]; then
            die "MTL DPDK patch directory not found: ${patch_dir}"
        fi

        local patches=()
        while IFS= read -r -d '' patch; do
            patches+=("$patch")
        done < <(find "$patch_dir" -maxdepth 1 -type f -name '*.patch' -print0 | sort -z)

        if [[ "${#patches[@]}" -eq 0 ]]; then
            die "No DPDK patches found in ${patch_dir}"
        fi

        log "Applying MTL DPDK patches from ${patch_dir}"
        (
            cd "$DPDK_SOURCE_DIR"
            git am "${patches[@]}"
        ) || {
            warn "git am failed. Attempting git am --abort."
            git -C "$DPDK_SOURCE_DIR" am --abort || true
            die "Failed to apply MTL DPDK patches"
        }

        touch "$marker"
    else
        log "DPDK patch marker exists: ${marker}"
    fi

    local dpdk_build_dir="${DPDK_SOURCE_DIR}/build"

    if [[ "$FORCE_REBUILD_DPDK" -eq 1 ]]; then
        rm -rf "$dpdk_build_dir"
    fi

    if [[ ! -f "${dpdk_build_dir}/build.ninja" ]]; then
        log "Configuring DPDK"
        run meson setup "$dpdk_build_dir" "$DPDK_SOURCE_DIR"
    fi

    log "Building DPDK"
    run ninja -C "$dpdk_build_dir" -j "$JOBS"

    log "Installing DPDK"
    sudo_env_cmd "PATH=${PATH}" ninja install -C "$dpdk_build_dir"
    sudo_cmd ldconfig

    configure_pkg_config_paths

    if ! pkg-config --exists libdpdk; then
        die "DPDK installed, but pkg-config cannot find libdpdk.pc. Current PKG_CONFIG_PATH=${PKG_CONFIG_PATH:-}"
    fi

    log "DPDK pkg-config: $(pkg-config --modversion libdpdk 2>/dev/null || echo found)"
}

build_and_install_mtl() {
    if pkg-config --exists mtl && [[ "$FORCE_REBUILD_MTL" -eq 0 ]]; then
        log "MTL already visible through pkg-config: $(pkg-config --modversion mtl 2>/dev/null || echo found)"
        return
    fi

    clone_or_update_repo "$MTL_REPO_URL" "$MTL_SOURCE_DIR" "$MTL_REF" "Media Transport Library"

    build_and_install_dpdk

    log "Building and installing Media Transport Library"

    rm -rf "${MTL_SOURCE_DIR}/build" "${MTL_SOURCE_DIR}/tests/tools/RxTxApp/build"

    (
        cd "$MTL_SOURCE_DIR"
        ./build.sh release
    )

    sudo_cmd ldconfig
    configure_pkg_config_paths

    if ! pkg-config --exists mtl; then
        die "MTL build finished, but pkg-config cannot find mtl.pc. Current PKG_CONFIG_PATH=${PKG_CONFIG_PATH:-}"
    fi

    log "MTL pkg-config: $(pkg-config --modversion mtl 2>/dev/null || echo found)"
}

setup_nics_if_requested() {
    if [[ "${#CREATE_E800_VFS_BDFS[@]}" -eq 0 && "${#BIND_PMD_BDFS[@]}" -eq 0 ]]; then
        log "No NIC binding requested"
        return
    fi

    if [[ ! -x "${MTL_SOURCE_DIR}/script/nicctl.sh" ]]; then
        die "MTL nicctl.sh not found or not executable: ${MTL_SOURCE_DIR}/script/nicctl.sh"
    fi

    for bdf in "${CREATE_E800_VFS_BDFS[@]}"; do
        log "Creating E800 VFs and binding to DPDK PMD for PF: ${bdf}"
        sudo_env_cmd "PATH=${PATH}" "${MTL_SOURCE_DIR}/script/nicctl.sh" create_vf "$bdf"
    done

    for bdf in "${BIND_PMD_BDFS[@]}"; do
        log "Binding BDF to DPDK PMD: ${bdf}"
        sudo_env_cmd "PATH=${PATH}" "${MTL_SOURCE_DIR}/script/nicctl.sh" bind_pmd "$bdf"
    done
}

find_ndi_include_dir() {
    local candidates=()

    if [[ -n "$NDI_INCLUDE_DIR_ARG" ]]; then
        candidates+=("$NDI_INCLUDE_DIR_ARG")
    fi

    if [[ -n "${NDI_INCLUDE_DIR:-}" ]]; then
        candidates+=("${NDI_INCLUDE_DIR}")
    fi

    if [[ -n "${NDILIB_INCLUDE_DIR:-}" ]]; then
        candidates+=("${NDILIB_INCLUDE_DIR}")
    fi

    if [[ -n "${NDI_SDK_DIR:-}" ]]; then
        candidates+=("${NDI_SDK_DIR}/include")
    fi

    candidates+=(
        "/opt/ndi/include"
        "/usr/local/include"
        "/usr/include"
    )

    for dir in "${candidates[@]}"; do
        if [[ -f "${dir}/Processing.NDI.Lib.h" ]]; then
            printf '%s\n' "$dir"
            return 0
        fi
    done

    return 1
}

find_ndi_runtime_dir() {
    local candidates=()

    local inferred_sdk_roots=()

    if [[ -n "${NDI_INCLUDE_DIR:-}" ]]; then
        inferred_sdk_roots+=("$(cd "${NDI_INCLUDE_DIR}/.." 2>/dev/null && pwd || true)")
    fi

    if [[ -n "$NDI_INCLUDE_DIR_ARG" ]]; then
        inferred_sdk_roots+=("$(cd "${NDI_INCLUDE_DIR_ARG}/.." 2>/dev/null && pwd || true)")
    fi

    for root in "${inferred_sdk_roots[@]}"; do
        if [[ -z "$root" ]]; then
            continue
        fi

        candidates+=(
            "${root}/lib"
            "${root}/lib/x86_64-linux-gnu"
            "${root}/bin/x86_64-linux-gnu"
            "${root}/Lib/x86_64-linux-gnu"
            "${root}/Bin/x86_64-linux-gnu"
        )
    done

    if [[ -n "$NDI_RUNTIME_DIR_ARG" ]]; then
        candidates+=("$NDI_RUNTIME_DIR_ARG")
    fi

    if [[ -n "${NDI_RUNTIME_DIR_V6:-}" ]]; then
        candidates+=("${NDI_RUNTIME_DIR_V6}")
    fi

    if [[ -n "${NDILIB_REDIST_FOLDER:-}" ]]; then
        candidates+=("${NDILIB_REDIST_FOLDER}")
    fi

    if [[ -n "${NDI_SDK_DIR:-}" ]]; then
        candidates+=(
            "${NDI_SDK_DIR}/lib"
            "${NDI_SDK_DIR}/lib/x86_64-linux-gnu"
            "${NDI_SDK_DIR}/bin/x86_64-linux-gnu"
        )
    fi

    candidates+=(
        "/opt/ndi/lib"
        "/opt/ndi/lib/x86_64-linux-gnu"
        "/usr/local/lib"
        "/usr/lib"
        "/usr/lib64"
    )

    for dir in "${candidates[@]}"; do
        if [[ -f "${dir}/libndi.so.6" || -f "${dir}/libndi.so" ]]; then
            printf '%s\n' "$dir"
            return 0
        fi
    done

    if have_cmd ldconfig && ldconfig -p 2>/dev/null | grep -qE 'libndi\.so(\.6)?'; then
        printf '%s\n' ""
        return 0
    fi

    return 1
}

check_ndi() {
    local ndi_inc=""
    local ndi_rt=""

    if ndi_inc="$(find_ndi_include_dir)"; then
        export NDI_INCLUDE_DIR="$ndi_inc"
        log "NDI include dir: ${NDI_INCLUDE_DIR}"
    else
        if [[ "$REQUIRE_NDI" -eq 1 ]]; then
            die "NDI SDK header not found. Set --ndi-include-dir, NDI_INCLUDE_DIR, or NDI_SDK_DIR."
        fi

        warn "NDI SDK header not found. Disabling send app build."
        BUILD_SEND_APP=0
        INSTALL_SEND_APP=0
        return
    fi

    if ndi_rt="$(find_ndi_runtime_dir)"; then
        if [[ -n "$ndi_rt" ]]; then
            export NDI_RUNTIME_DIR_V6="$ndi_rt"
            append_unique_colon_path LD_LIBRARY_PATH "$ndi_rt"
            log "NDI runtime dir: ${NDI_RUNTIME_DIR_V6}"
        else
            log "NDI runtime found through system dynamic linker cache"
        fi
    else
        if [[ "$REQUIRE_NDI" -eq 1 ]]; then
            die "NDI runtime not found. Set --ndi-runtime-dir, NDI_RUNTIME_DIR_V6, or install libndi.so.6."
        fi

        warn "NDI runtime not found. Build may pass, runtime discovery/sender will fail."
    fi
}

check_mtl_available() {
    configure_pkg_config_paths

    if pkg-config --exists mtl && [[ "$FORCE_REBUILD_MTL" -eq 0 ]]; then
        log "MTL pkg-config: $(pkg-config --modversion mtl 2>/dev/null || echo found)"
        return
    fi

    if [[ "$INSTALL_MTL" -eq 0 ]]; then
        die "MTL pkg-config package 'mtl' was not found, or MTL rebuild was requested, but --no-install-mtl is set.
Install MTL manually, remove --no-install-mtl, or provide --mtl-pkg-config-dir."
    fi

    if pkg-config --exists mtl && [[ "$FORCE_REBUILD_MTL" -eq 1 ]]; then
        log "Forcing MTL rebuild despite existing pkg-config package: $(pkg-config --modversion mtl 2>/dev/null || echo found)"
    fi

    build_and_install_mtl
}

configure_and_build_project() {
    local build_plugin_flag="OFF"
    local build_worker_flag="OFF"
    local worker_install_dir="${OBS_PLUGIN_DIR}/bin/64bit"
    local build_send_app_flag="OFF"
    local mtl_dev_kernel_socket_flag="OFF"

    if [[ "$BUILD_PLUGIN" -eq 1 ]]; then
        build_plugin_flag="ON"
        build_worker_flag="ON"
        BUILD_MTL_RX_WORKER=1
        INSTALL_MTL_RX_WORKER=1
    fi

    if [[ "$BUILD_SEND_APP" -eq 1 ]]; then
        build_send_app_flag="ON"
    fi

    if [[ "$MTL_DEV_KERNEL_SOCKET" -eq 1 ]]; then
        mtl_dev_kernel_socket_flag="ON"
    fi

    log "Configuring project CMake"
    run cmake -S "$REPO_DIR" -B "$BUILD_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DST2110_BUILD_OBS_PLUGIN="$build_plugin_flag" \
        -DST2110_BUILD_MTL_RX_WORKER="$build_worker_flag" \
        -DST2110_BUILD_MTL_SEND_TEST_APP="$build_send_app_flag" \
        -DST2110_MTL_DEV_KERNEL_SOCKET="$mtl_dev_kernel_socket_flag" \
        -DST2110_OBS_PLUGIN_INSTALL_DIR="$OBS_PLUGIN_DIR" \
        -DST2110_MTL_RX_WORKER_INSTALL_DIR="$worker_install_dir"

    local targets=()

    if [[ "$BUILD_PLUGIN" -eq 1 ]]; then
        targets+=("st2110_obs")
    fi

    if [[ "$BUILD_MTL_RX_WORKER" -eq 1 ]]; then
        targets+=("st2110_mtl_rx_worker")
    fi

    if [[ "$BUILD_SEND_APP" -eq 1 ]]; then
        targets+=("st2110_mtl_send_test")
    fi

    if [[ "${#targets[@]}" -eq 0 ]]; then
        die "Nothing to build"
    fi

    log "Building project targets: ${targets[*]}"
    run cmake --build "$BUILD_DIR" --target "${targets[@]}" -j "$JOBS"
}

check_runtime_deps() {
    local binary="$1"
    local label="$2"

    if ! have_cmd ldd; then
        warn "ldd is not available; cannot verify runtime dependencies for ${label}"
        return
    fi

    local missing=""
    missing="$(ldd "$binary" 2>/dev/null | awk '/not found/ {print}' || true)"

    if [[ -n "$missing" ]]; then
        die "${label} has unresolved runtime libraries:
${missing}

Run sudo ldconfig after installing MTL/DPDK, or fix the dynamic linker path before starting OBS."
    fi
}

install_project_outputs() {
    if [[ "$INSTALL_PLUGIN" -eq 1 ]]; then
        local plugin_so=""

        plugin_so="$(find "$BUILD_DIR" -type f -name 'st2110_obs.so' | head -n 1 || true)"
        if [[ -z "$plugin_so" ]]; then
            die "Built plugin st2110_obs.so was not found under ${BUILD_DIR}"
        fi

        local plugin_bin_dir="${OBS_PLUGIN_DIR}/bin/64bit"
        run mkdir -p "$plugin_bin_dir"
        run install -m 755 "$plugin_so" "${plugin_bin_dir}/st2110_obs.so"

        log "Installed OBS plugin:"
        log "  ${plugin_bin_dir}/st2110_obs.so"

        if [[ "$INSTALL_MTL_RX_WORKER" -eq 1 ]]; then
            local worker_bin=""

            worker_bin="$(find "$BUILD_DIR" -type f -name 'st2110_mtl_rx_worker' -perm -111 | head -n 1 || true)"
            if [[ -z "$worker_bin" ]]; then
                die "Built worker st2110_mtl_rx_worker was not found under ${BUILD_DIR}"
            fi

            run install -m 755 "$worker_bin" "${plugin_bin_dir}/st2110_mtl_rx_worker"
            check_runtime_deps "${plugin_bin_dir}/st2110_mtl_rx_worker" "Installed MTL RX worker"

            log "Installed MTL RX worker:"
            log "  ${plugin_bin_dir}/st2110_mtl_rx_worker"
        fi
    fi

    if [[ "$INSTALL_SEND_APP" -eq 1 ]]; then
        local app_bin=""

        app_bin="$(find "$BUILD_DIR" -type f -name 'st2110_mtl_send_test' -perm -111 | head -n 1 || true)"
        if [[ -z "$app_bin" ]]; then
            die "Built sender app st2110_mtl_send_test was not found under ${BUILD_DIR}"
        fi

        run mkdir -p "$APP_INSTALL_DIR"
        run install -m 755 "$app_bin" "${APP_INSTALL_DIR}/st2110_mtl_send_test"

        log "Installed sender app:"
        log "  ${APP_INSTALL_DIR}/st2110_mtl_send_test"
    fi
}

print_summary() {
    cat <<EOF

Done.

Environment used:
  REPO_DIR=${REPO_DIR}
  DEPS_DIR=${DEPS_DIR}
  BUILD_DIR=${BUILD_DIR}

  MTL_SOURCE_DIR=${MTL_SOURCE_DIR}
  DPDK_SOURCE_DIR=${DPDK_SOURCE_DIR}
  DPDK_REF=${DPDK_REF}
  DPDK_PATCH_SERIES=${DPDK_PATCH_SERIES}

  NDI_INCLUDE_DIR=${NDI_INCLUDE_DIR:-}
  NDI_RUNTIME_DIR_V6=${NDI_RUNTIME_DIR_V6:-}
  PKG_CONFIG_PATH=${PKG_CONFIG_PATH:-}
  CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH:-}
  LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-}
  ST2110_MTL_DEV_KERNEL_SOCKET=${MTL_DEV_KERNEL_SOCKET}

Installed:
  OBS plugin dir: ${OBS_PLUGIN_DIR}
  OBS plugin binary dir: ${OBS_PLUGIN_DIR}/bin/64bit
  MTL RX worker: ${OBS_PLUGIN_DIR}/bin/64bit/st2110_mtl_rx_worker
  sender app dir: ${APP_INSTALL_DIR}

Important:
  If vfio group membership changed, log out/in before running MTL without root.
  If GRUB/IOMMU was changed, reboot before DPDK/VFIO usage.
  Hugepages after reboot may need checking:
    grep -E 'HugePages_Total|HugePages_Free|Hugepagesize' /proc/meminfo

Smoke tests:
  Metadata-only:
    ${APP_INSTALL_DIR}/st2110_mtl_send_test \\
      --metadata-only \\
      --media av \\
      --name "ST2110 Metadata Test" \\
      --local-ip <sender-ip>

  Video-only 720p30:
    ${APP_INSTALL_DIR}/st2110_mtl_send_test \\
      --media video \\
      --name "ST2110 Video 720p30" \\
      --video-mode 720p30 \\
      --port-name <mtl-port-bdf-or-kernel-name> \\
      --local-ip <sender-ip> \\
      --video-dst-ip 239.211.0.20 \\
      --video-udp-port 5004

  Audio-only:
    ${APP_INSTALL_DIR}/st2110_mtl_send_test \\
      --media audio \\
      --name "ST2110 Audio Tone" \\
      --port-name <mtl-port-bdf-or-kernel-name> \\
      --local-ip <sender-ip> \\
      --audio-dst-ip 239.211.0.22 \\
      --audio-udp-port 5006
EOF
}

install_apt_packages
configure_pkg_config_paths
configure_hugepages
configure_vfio_permissions
configure_intel_iommu_grub
check_ndi
check_mtl_available
setup_nics_if_requested
configure_and_build_project
install_project_outputs
print_summary