#!/usr/bin/env python3
"""Clone, build, and install the pinned SDL3 release."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys


SDL_VERSION = "3.4.14"
SDL_TAG = f"release-{SDL_VERSION}"
SDL_REPOSITORY = "https://github.com/libsdl-org/SDL.git"

SDL_ROOT = Path(__file__).resolve().parent
SOURCE_DIR = SDL_ROOT / "source"
BUILD_DIR = SDL_ROOT / "build"
INSTALL_DIR = SDL_ROOT / "installed"


def format_command(command: list[str]) -> str:
    if os.name == "nt":
        return subprocess.list2cmdline(command)
    return shlex.join(command)


def run(command: list[str]) -> None:
    print(f"+ {format_command(command)}", flush=True)
    subprocess.run(command, check=True)


def require_tool(name: str) -> str:
    executable = shutil.which(name)
    if executable is None:
        raise RuntimeError(f"Required tool '{name}' was not found on PATH.")
    return executable


def git_in_source(git: str, *git_args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [git, "-C", str(SOURCE_DIR), *git_args],
        capture_output=True,
        text=True,
    )


def source_is_pinned(git: str) -> bool:
    describe = git_in_source(git, "describe", "--tags", "--exact-match", "HEAD")
    return describe.returncode == 0 and describe.stdout.strip() == SDL_TAG


def clone_or_update_source(git: str) -> None:
    if not SOURCE_DIR.exists():
        run(
            [
                git,
                "clone",
                "--branch",
                SDL_TAG,
                "--depth",
                "1",
                SDL_REPOSITORY,
                str(SOURCE_DIR),
            ]
        )
        return

    if not (SOURCE_DIR / ".git").is_dir():
        raise RuntimeError(
            f"{SOURCE_DIR} exists but is not a Git checkout. "
            "Move or remove it before retrying."
        )

    status = git_in_source(git, "status", "--porcelain")
    if status.returncode != 0:
        raise RuntimeError(status.stderr.strip() or "git status failed.")
    if status.stdout.strip():
        raise RuntimeError(
            f"{SOURCE_DIR} contains local changes. Commit, stash, or remove them "
            "before updating SDL."
        )

    if source_is_pinned(git):
        print(f"SDL source already at {SDL_TAG}", flush=True)
        return

    run([git, "-C", str(SOURCE_DIR), "fetch", "--depth", "1", "origin", "tag", SDL_TAG])
    run([git, "-C", str(SOURCE_DIR), "checkout", "--detach", SDL_TAG])


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            f"Build SDL {SDL_VERSION} into ThirdParty/SDL/installed. "
            "The source and intermediate files remain under ThirdParty/SDL."
        )
    )
    parser.add_argument(
        "--config",
        default="Release",
        help="CMake build configuration (default: Release).",
    )
    parser.add_argument(
        "--linkage",
        choices=("shared", "static", "both"),
        default="shared",
        help="SDL libraries to build (default: shared).",
    )
    parser.add_argument(
        "--generator",
        help='CMake generator, for example "Ninja" or "Visual Studio 18 2026".',
    )
    parser.add_argument(
        "--jobs",
        type=int,
        help="Maximum number of parallel build jobs.",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Delete the SDL build and install directories before building.",
    )
    parser.add_argument(
        "--cmake-arg",
        action="append",
        default=[],
        metavar="ARG",
        help="Additional CMake configure argument; may be specified more than once.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.jobs is not None and args.jobs < 1:
        raise RuntimeError("--jobs must be at least 1.")

    git = require_tool("git")
    cmake = require_tool("cmake")

    if args.clean:
        shutil.rmtree(BUILD_DIR, ignore_errors=True)
        shutil.rmtree(INSTALL_DIR, ignore_errors=True)

    SDL_ROOT.mkdir(parents=True, exist_ok=True)
    clone_or_update_source(git)

    build_shared = args.linkage in ("shared", "both")
    build_static = args.linkage in ("static", "both")
    configure_command = [
        cmake,
        "-S",
        str(SOURCE_DIR),
        "-B",
        str(BUILD_DIR),
        f"-DCMAKE_INSTALL_PREFIX={INSTALL_DIR}",
        f"-DCMAKE_BUILD_TYPE={args.config}",
        f"-DSDL_SHARED={'ON' if build_shared else 'OFF'}",
        f"-DSDL_STATIC={'ON' if build_static else 'OFF'}",
        "-DSDL_INSTALL=ON",
        "-DSDL_INSTALL_CPACK=OFF",
        "-DSDL_INSTALL_DOCS=OFF",
        "-DSDL_TEST_LIBRARY=OFF",
        "-DSDL_TESTS=OFF",
        "-DSDL_EXAMPLES=OFF",
    ]
    if args.generator:
        configure_command.extend(["-G", args.generator])
    configure_command.extend(args.cmake_arg)
    run(configure_command)

    build_command = [
        cmake,
        "--build",
        str(BUILD_DIR),
        "--config",
        args.config,
        "--parallel" if args.jobs is None else f"--parallel={args.jobs}",
    ]
    run(build_command)
    run(
        [
            cmake,
            "--install",
            str(BUILD_DIR),
            "--config",
            args.config,
        ]
    )

    print(f"\nSDL {SDL_VERSION} installed to {INSTALL_DIR}")
    print("Configure SweetDream with -DSD_BUILD_GUI=ON")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        sys.exit(1)
