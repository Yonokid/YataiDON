"""Fetch the additional pinned, public sources for the PS5 game build."""
import hashlib
import tarfile
from pathlib import Path
import shutil
import subprocess
import urllib.request
import zipfile

VENDOR = Path(__file__).resolve().parent / "vendor"
DEPS = {
    "libebur128": ("jiixyj/libebur128", "67b33abe1558160ed76ada1322329b0e9e058b02"),
    "rapidjson": ("Tencent/rapidjson", "24b5e7a8b27f42fa16b96fc70aade9106cf7102f"),
    "tomlplusplus": ("marzer/tomlplusplus", "30172438cee64926dc41fdd9c11fb3ba5b2ba9de"),
    "spdlog": ("gabime/spdlog", "f355b3d58f7067eee1706ff3c801c2361011f3d5"),
    "lua": ("marovira/lua", "4737fb98d04a3e2fd365e55d8cb95e4c961db3a5"),
    "sol2": ("ThePhD/sol2", "9190880c593dfb018ccf5cc9729ab87739709862"),
    "miniz": ("richgel999/miniz", "39f65158c7e5a830e390f3aece08de654c763981"),
    "libsamplerate": ("libsndfile/libsamplerate", "c96f5e3de9c4488f4e6c97f59f5245f22fda22f7"),
    "libsndfile": ("libsndfile/libsndfile", "72f6af15e8f85157bd622ed45b979025828b7001"),
}


def main():
    VENDOR.mkdir(parents=True, exist_ok=True)
    for name, (repo, revision) in DEPS.items():
        path = VENDOR / name
        if not path.exists():
            subprocess.run(["git", "clone", "--no-checkout", "--depth", "1", f"https://github.com/{repo}.git", str(path)], check=True)
            subprocess.run(["git", "-C", str(path), "fetch", "--depth", "1", "origin", revision], check=True)
            subprocess.run(["git", "-C", str(path), "checkout", "--detach", revision], check=True)
        actual = subprocess.check_output(["git", "-C", str(path), "rev-parse", "HEAD"], text=True).strip()
        if actual != revision:
            raise RuntimeError(f"Unexpected revision in {path}; existing sources were preserved")
    bgfx = VENDOR / "bgfx.cmake"
    if not bgfx.exists():
        archive = VENDOR / "bgfx.cmake.v1.167.9590-598.tar.gz"
        urllib.request.urlretrieve(
            "https://github.com/bkaradzic/bgfx.cmake/releases/download/v1.167.9590-598/bgfx.cmake.v1.167.9590-598.tar.gz",
            archive)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != "a0cab59dc46776e0155ce8ad415e595fc6cc7d5b17fe66fd5c96f330540174df":
            raise RuntimeError("bgfx release checksum mismatch")
        with tarfile.open(archive) as source:
            source.extractall(VENDOR, filter="data")
    subprocess.run(["python3", str(VENDOR.parent / "tools/patch-bgfx.py"), str(bgfx)], check=True)
    sqlite = VENDOR / "sqlite"
    if not (sqlite / "sqlite3.c").exists():
        archive = VENDOR / "sqlite-amalgamation-3500400.zip"
        urllib.request.urlretrieve("https://www.sqlite.org/2025/sqlite-amalgamation-3500400.zip", archive)
        with archive.open("rb") as stream:
            checksum = hashlib.file_digest(stream, "sha256").hexdigest()
        if checksum != "1d3049dd0f830a025a53105fc79fd2ab9431aea99e137809d064d8ee8356b032":
            raise RuntimeError("SQLite archive checksum mismatch")
        sqlite.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(archive) as source:
            for name in ("sqlite3.c", "sqlite3.h"):
                with source.open(f"sqlite-amalgamation-3500400/{name}") as data, (sqlite / name).open("wb") as output:
                    shutil.copyfileobj(data, output)


if __name__ == "__main__":
    main()
