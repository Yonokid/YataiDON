"""Fetch the sources matching devkitPro switch-ffmpeg 7.1-5 and libiconv 1.18."""

import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request

# Python < 3.11.4 (devkitPro image) lacks extraction filters.
SAFE_EXTRACT = {"filter": "data"} if hasattr(tarfile, "data_filter") else {}
DEFAULT_VENDOR = Path(__file__).resolve().parent / "vendor"
VENDOR = Path(os.environ.get("YATAIDON_MEDIA_SOURCE_DIR", DEFAULT_VENDOR))
FILES = {
    "ffmpeg-7.1.tar.xz": (
        "https://ffmpeg.org/releases/ffmpeg-7.1.tar.xz",
        "40973d44970dbc83ef302b0609f2e74982be2d85916dd2ee7472d30678a7abe6",
    ),
    "ffmpeg-7.1.patch": (
        "https://raw.githubusercontent.com/devkitPro/pacman-packages/master/switch/ffmpeg/ffmpeg-7.1.patch",
        "1792380b992e3554a4abcddf0d7b395bfd8c118ac7c6e38c8f2fb0d39753a390",
    ),
    "ffmpeg-7.1-tls.patch": (
        "https://raw.githubusercontent.com/devkitPro/pacman-packages/master/switch/ffmpeg/tls.patch",
        "57ea7ec8ed26d13d3172d0fd589b7883d22f0c180d50b7434fcc73fb2b3ab7d7",
    ),
    "libiconv-1.18.tar.gz": (
        "https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.18.tar.gz",
        "3b08f5f4f9b4eb82f151a7040bfd6fe6c6fb922efe4b1659c66ea933276965e8",
    ),
}


def main():
    VENDOR.mkdir(parents=True, exist_ok=True)
    for name, (url, expected) in FILES.items():
        path = VENDOR / name
        if not path.exists():
            cached = DEFAULT_VENDOR / name
            if cached != path and cached.exists():
                shutil.copy2(cached, path)
            else:
                print(f"Downloading {name}", flush=True)
                urllib.request.urlretrieve(url, path)
        with path.open("rb") as stream:
            actual = hashlib.file_digest(stream, "sha256").hexdigest()
        if actual != expected:
            raise RuntimeError(f"Checksum mismatch for {path}; existing file was preserved")

    for name, archive in (("ffmpeg-7.1", "ffmpeg-7.1.tar.xz"), ("libiconv-1.18", "libiconv-1.18.tar.gz")):
        if not (VENDOR / name).exists():
            with tarfile.open(VENDOR / archive) as source:
                source.extractall(VENDOR, **SAFE_EXTRACT)

    ffmpeg = VENDOR / "ffmpeg-7.1"
    marker = ffmpeg / ".yataidon-portlibs-patches"
    patch_names = ("ffmpeg-7.1.patch", "ffmpeg-7.1-tls.patch")
    expected = "\n".join(FILES[name][1] for name in patch_names) + "\n"
    if marker.exists():
        if marker.read_text() != expected:
            raise RuntimeError("Unexpected FFmpeg patch marker; existing sources were preserved")
    else:
        for name in patch_names:
            subprocess.run(["patch", "-p1", "-i", str(VENDOR / name)], cwd=ffmpeg, check=True)
        marker.write_text(expected)


if __name__ == "__main__":
    main()
