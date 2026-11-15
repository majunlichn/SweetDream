#!/usr/bin/env python3
"""Download ShaderToy resources into the Resources directory beside this script."""

from pathlib import Path
import hashlib
import shutil
import sys
import tempfile
from urllib.error import URLError
from urllib.request import Request, urlopen


RESOURCES = {
    "RustyMetal.jpg": (
        "https://www.shadertoy.com/media/a/"
        "95b90082f799f48677b4f206d856ad572f1d178c676269eac6347631d4447258.jpg",
        "ef063b8f751db77d2ca4ef8a7f28ba60096e58cc8250b30f32923f99b067c9aa",
    ),
}


def verify_checksum(path: Path, expected: str) -> None:
    digest = hashlib.sha256()
    with path.open("rb") as resource:
        for chunk in iter(lambda: resource.read(65536), b""):
            digest.update(chunk)
    if digest.hexdigest() != expected:
        raise RuntimeError(f"Resource checksum mismatch: {path}; remove it and rerun")


def download(url: str, destination: Path, checksum: str) -> None:
    if destination.is_file() and destination.stat().st_size > 0:
        verify_checksum(destination, checksum)
        print(f"Already downloaded: {destination}")
        return

    temporary = None
    try:
        request = Request(url, headers={"User-Agent": "SweetDream-ShaderToy/1.0"})
        with urlopen(request, timeout=60) as response:
            if response.headers.get_content_type() == "text/html":
                raise RuntimeError(f"Received HTML instead of a resource: {url}")
            with tempfile.NamedTemporaryFile(
                dir=destination.parent, suffix=".tmp", delete=False
            ) as output:
                temporary = Path(output.name)
                shutil.copyfileobj(response, output)
            size = temporary.stat().st_size
            expected = response.headers.get("Content-Length")
            if size == 0 or (expected is not None and size != int(expected)):
                raise RuntimeError(f"Incomplete resource download: {url}")
        verify_checksum(temporary, checksum)
        temporary.replace(destination)
        print(f"Downloaded: {destination} ({size} bytes)")
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def main() -> int:
    directory = Path(__file__).resolve().parent / "Resources"
    try:
        directory.mkdir(exist_ok=True)
        for filename, (url, checksum) in RESOURCES.items():
            download(url, directory / filename, checksum)
    except (OSError, URLError, RuntimeError, ValueError) as error:
        print(f"Resource download failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
