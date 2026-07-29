cmake_minimum_required(VERSION 3.16)
project(meshcore_linux_udp_bridge CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

set(MESHCORE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(VARIANT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED libsodium)

add_executable(meshcore_repeater
  # --- MeshCore core (src/*.cpp - verified: no unconditional Arduino.h dependency) ---
  ${MESHCORE_ROOT}/src/Dispatcher.cpp
  ${MESHCORE_ROOT}/src/Identity.cpp
  ${MESHCORE_ROOT}/src/Mesh.cpp
  ${MESHCORE_ROOT}/src/Packet.cpp
  ${MESHCORE_ROOT}/src/Utils.cpp

  # --- MeshCore helpers actually referenced by MyMesh.h/.cpp (NOT the full
  #     src/helpers/*.cpp set - several of those, e.g. AutoDiscoverRTCClock.cpp,
  #     RTC_RX8130CE.cpp, are NOT self-guarded by #ifdef and would drag in real
  #     I2C RTC chip libraries we don't have and don't need) ---
  ${MESHCORE_ROOT}/src/helpers/AdvertDataHelpers.cpp
  ${MESHCORE_ROOT}/src/helpers/ClientACL.cpp
  ${MESHCORE_ROOT}/src/helpers/CommonCLI.cpp
  ${MESHCORE_ROOT}/src/helpers/IdentityStore.cpp
  ${MESHCORE_ROOT}/src/helpers/RegionMap.cpp
  ${MESHCORE_ROOT}/src/helpers/StaticPoolPacketManager.cpp
  ${MESHCORE_ROOT}/src/helpers/TransportKeyStore.cpp
  ${MESHCORE_ROOT}/src/helpers/TxtDataHelpers.cpp

  # --- vendored Ed25519 C library (already portable, no Arduino dependency) ---
  ${MESHCORE_ROOT}/lib/ed25519/add_scalar.c
  ${MESHCORE_ROOT}/lib/ed25519/fe.c
  ${MESHCORE_ROOT}/lib/ed25519/ge.c
  ${MESHCORE_ROOT}/lib/ed25519/key_exchange.c
  ${MESHCORE_ROOT}/lib/ed25519/keypair.c
  ${MESHCORE_ROOT}/lib/ed25519/sc.c
  ${MESHCORE_ROOT}/lib/ed25519/seed.c
  ${MESHCORE_ROOT}/lib/ed25519/sha512.c
  ${MESHCORE_ROOT}/lib/ed25519/sign.c
  ${MESHCORE_ROOT}/lib/ed25519/verify.c

  # --- the repeater application itself (upstream, 3 small LINUX_PLATFORM edits applied) ---
  ${MESHCORE_ROOT}/examples/simple_repeater/main.cpp
  ${MESHCORE_ROOT}/examples/simple_repeater/MyMesh.cpp

  # --- this variant ---
  ${VARIANT_ROOT}/target.cpp
  ${VARIANT_ROOT}/linux_main.cpp
  ${VARIANT_ROOT}/PktFwdRadio.cpp

  # --- Linux/Arduino compatibility shim ---
  ${VARIANT_ROOT}/compat/LinuxCompat.cpp
  ${VARIANT_ROOT}/compat/lib/tiny-aes-c/aes.c
)

target_include_directories(meshcore_repeater PRIVATE
  ${VARIANT_ROOT}/compat        # Arduino.h/Stream.h/Print.h/FS.h/SHA256.h/AES.h/Ed25519.h/RTClib.h/CayenneLPP.h
  ${VARIANT_ROOT}                # target.h
  ${MESHCORE_ROOT}/src
  ${MESHCORE_ROOT}/src/helpers
  ${MESHCORE_ROOT}/lib/ed25519   # ed_25519.h
  ${MESHCORE_ROOT}/examples/simple_repeater  # RateLimiter.h (included by MyMesh.h)
  ${SODIUM_INCLUDE_DIRS}
)

target_compile_definitions(meshcore_repeater PRIVATE
  LINUX_PLATFORM=1
  NDEBUG
  ENABLE_ADVERT_ON_BOOT=1
  ENABLE_PRIVATE_KEY_IMPORT=1
  ENABLE_PRIVATE_KEY_EXPORT=1
  # Brisbane/SEQ MeshCore default preset (MBUG wiki, wiki.mbug.com.au/en/Meshcore/Settings) -
  # see Research_beginning_notes.md for provenance. Must match global_conf_1302.json's
  # radio_0/chan_Lora_std retune, and whatever the local mesh is actually using -
  # these two configs are not independently adjustable.
  LORA_FREQ=923.125
  LORA_BW=62.5
  LORA_SF=8
  LORA_CR=5
)

target_link_directories(meshcore_repeater PRIVATE ${SODIUM_LIBRARY_DIRS})
target_link_libraries(meshcore_repeater PRIVATE ${SODIUM_LIBRARIES})
target_compile_options(meshcore_repeater PRIVATE -w ${SODIUM_CFLAGS_OTHER})
