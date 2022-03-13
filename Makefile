# NOTE(dkorolev): Yes, it is ugly to have a `Makefile` for a `cmake`-built project,
#                 but why not keep both "standard" solutions working out of the box?

.PHONY: debug release indent clean

DEBUG_BUILD_DIR=Debug
RELEASE_BUILD_DIR=Release

CLANG_FORMAT=clang-format-10

debug: ${DEBUG_BUILD_DIR}
	MAKEFLAGS=--no-print-directory cmake --build "${DEBUG_BUILD_DIR}"

${DEBUG_BUILD_DIR}:
	cmake -B "${DEBUG_BUILD_DIR}" .

release: ${RELEASE_BUILD_DIR}
	MAKEFLAGS=--no-print-directory cmake --build "${RELEASE_BUILD_DIR}"

${RELEASE_BUILD_DIR}:
	cmake -DCMAKE_BUILD_TYPE=Release -B "${RELEASE_BUILD_DIR}" .

indent:
	clang-format-10 -i *.cc

clean:
	rm -rf "${DEBUG_BUILD_DIR}" "${RELEASE_BUILD_DIR}"
