#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version="$(<"$repo_root/VERSION.txt")"
openssl_tag="${OPENSSL_TAG:-openssl-3.5.9}"
work_dir="$(mktemp -d "${RUNNER_TEMP:-/tmp}/nx3d-win-release.XXXXXX")"
openssl_prefix="${OPENSSL_PREFIX:-$work_dir/openssl-prefix}"
jobs="${BUILD_JOBS:-4}"

if [[ -z "${OPENSSL_PREFIX:-}" ]]; then
    git clone --quiet --depth 1 --branch "$openssl_tag" \
        https://github.com/openssl/openssl.git "$work_dir/openssl-src"
    (
        cd "$work_dir/openssl-src"
        perl Configure mingw64 no-shared no-tests \
            --prefix="$openssl_prefix" \
            --cross-compile-prefix=x86_64-w64-mingw32-
        make -j"$jobs" build_libs > "$work_dir/openssl-build.log" 2>&1 || {
            tail -80 "$work_dir/openssl-build.log"
            exit 1
        }
        make -j"$jobs" install_sw > "$work_dir/openssl-install.log" 2>&1 || {
            tail -80 "$work_dir/openssl-install.log"
            exit 1
        }
    )
fi

# Copy only Boost headers. Including /usr/include directly would pull Linux
# C library headers into the MinGW build.
mkdir -p "$work_dir/boost-include" "$work_dir/cmake"
cp -a /usr/include/boost "$work_dir/boost-include/"
printf '#include <boost/json/src.hpp>\n' > "$work_dir/boost_json.cc"
x86_64-w64-mingw32-g++ -std=c++20 -O2 \
    -DBOOST_JSON_DYN_LINK=1 -DBOOST_JSON_SOURCE=1 \
    -I"$work_dir/boost-include" -shared "$work_dir/boost_json.cc" \
    -o "$work_dir/libboost_json_windows.dll" \
    -Wl,--out-implib,"$work_dir/libboost_json_windows.dll.a"

cat > "$work_dir/toolchain.cmake" <<'EOF'
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE NEVER)
EOF

cat > "$work_dir/cmake/FindBoost.cmake" <<EOF
set(Boost_FOUND TRUE)
set(Boost_VERSION 108300)
set(Boost_INCLUDE_DIRS "$work_dir/boost-include")
if(NOT TARGET Boost::json)
    add_library(Boost::json SHARED IMPORTED)
    set_target_properties(Boost::json PROPERTIES
        IMPORTED_LOCATION "$work_dir/libboost_json_windows.dll"
        IMPORTED_IMPLIB "$work_dir/libboost_json_windows.dll.a"
        INTERFACE_INCLUDE_DIRECTORIES "$work_dir/boost-include"
        INTERFACE_COMPILE_DEFINITIONS BOOST_JSON_DYN_LINK=1)
endif()
EOF

# Build Bullet for the MinGW target. Static libraries avoid adding Bullet DLLs
# to the server archive.
bullet_prefix="$work_dir/bullet-prefix"
git clone --quiet --depth 1 --branch 3.25 \
    https://github.com/bulletphysics/bullet3.git "$work_dir/bullet-src"
test "$(git -C "$work_dir/bullet-src" rev-parse HEAD)" = \
    2c204c49e56ed15ec5fcfa71d199ab6d6570b3f5
cmake -S "$work_dir/bullet-src" -B "$work_dir/bullet-build" \
    -DCMAKE_TOOLCHAIN_FILE="$work_dir/toolchain.cmake" \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$bullet_prefix" \
    -DBUILD_SHARED_LIBS=OFF -DBUILD_BULLET3=OFF \
    -DBUILD_BULLET2_DEMOS=OFF -DBUILD_CPU_DEMOS=OFF \
    -DBUILD_EXTRAS=OFF -DBUILD_UNIT_TESTS=OFF \
    -DUSE_GLUT=OFF -DUSE_GRAPHICAL_BENCHMARK=OFF \
    -DINSTALL_LIBS=ON -DINSTALL_CMAKE_FILES=OFF
cmake --build "$work_dir/bullet-build" --parallel "$jobs"
cmake --install "$work_dir/bullet-build"

CI=1 cmake -S "$repo_root" -B "$work_dir/build" \
    -DCMAKE_TOOLCHAIN_FILE="$work_dir/toolchain.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DNX3D_BUILD_CLIENT=OFF -DBUILD_TESTING=OFF \
    -DCMAKE_MODULE_PATH="$work_dir/cmake" \
    -DOPENSSL_ROOT_DIR="$openssl_prefix" \
    -DOPENSSL_USE_STATIC_LIBS=TRUE \
    -DBULLET_INCLUDE_DIR="$bullet_prefix/include/bullet" \
    -DBULLET_DYNAMICS_LIBRARY="$bullet_prefix/lib/libBulletDynamics.a" \
    -DBULLET_COLLISION_LIBRARY="$bullet_prefix/lib/libBulletCollision.a" \
    -DBULLET_MATH_LIBRARY="$bullet_prefix/lib/libLinearMath.a" \
    -DBULLET_SOFTBODY_LIBRARY="$bullet_prefix/lib/libBulletSoftBody.a"
cmake --build "$work_dir/build" --target nx-3D-server --parallel "$jobs"

package_dir="$work_dir/package"
mkdir -p "$package_dir" "$repo_root/dist"
cp "$work_dir/build/server/nx-3D-server.exe" \
    "$work_dir/build/nexilis/libnexilis.dll" \
    "$work_dir/libboost_json_windows.dll" "$package_dir/"
for dll in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll; do
    dll_path="$(find /usr -type f -path '*x86_64*' -name "$dll" -print -quit)"
    if [[ -z "$dll_path" ]]; then
        echo "Missing MinGW runtime: $dll" >&2
        exit 1
    fi
    cp "$dll_path" "$package_dir/"
done

file "$package_dir/nx-3D-server.exe" | grep -q 'PE32+ executable'
x86_64-w64-mingw32-objdump -p "$package_dir/nx-3D-server.exe" |
    grep -q 'DLL Name: libnexilis.dll'
archive="$repo_root/dist/nx-3D-server-v${version}-windows-x64.zip"
(cd "$package_dir" && zip -q "$archive" ./*)
unzip -t "$archive" | tail -1
echo "Created $archive"
