"""Apply the PS5 Vulkan integration to bgfx.cmake v1.167.9590-598.

The renderer adaptation follows Waddamburo's native PS5 bridge (MIT).
Refuse unexpected source edits; do not silently rewrite a different bgfx release.
"""
from pathlib import Path
import hashlib
import os
import subprocess
import sys

EXPECTED = {'bx/src/allocator.cpp': ['aed1d47866a219fb5a6048bcc66742d2999677d18b5e2cadf97c97fbb7db7e04', '60737bcd11e06a839be3a247070a5415d8a3c723171a889f5150045408e82e9c'], 'bgfx/src/renderer_vk.cpp': ['1c7a7f737ef61d814cd3db86022385740c391bcef087e86d7c6d9c8ee362cf05', 'b1f9530a4be31aa1aca5933154db6b45198ed34c42c060f7d41c6ff757ca1c2c'], 'cmake/bgfx/bgfx.cmake': ['91898ae3664b34b11b12f2b60da341665c8649559300810b0b05f46fd14b8bfe', 'a6d506ddd4d6e5e204fe9696e938a6b1d5cc57181750a0d585d65d069ef6d21a']}

def main():
    root = Path(sys.argv[1]).resolve()
    actual = {name: hashlib.sha256((root / name).read_bytes()).hexdigest() for name in EXPECTED}
    if all(actual[name] == checksums[1] for name, checksums in EXPECTED.items()):
        return
    if not all(actual[name] == checksums[0] for name, checksums in EXPECTED.items()):
        raise SystemExit("Unexpected bgfx sources; use the pinned release or preserve your edits in another directory")
    patch = Path(__file__).resolve().parents[1] / "patches/bgfx-ps5.patch"
    # A release archive is often unpacked inside the application's repository.
    # Keep apply rooted at the dependency, independently of that parent worktree.
    environment = dict(os.environ, GIT_CEILING_DIRECTORIES=str(root.parent))
    args = ["git", "apply", "--no-index"]
    subprocess.run(args + ["--check", str(patch)], cwd=root, env=environment, check=True)
    subprocess.run(args + [str(patch)], cwd=root, env=environment, check=True)
    for name, checksums in EXPECTED.items():
        if hashlib.sha256((root / name).read_bytes()).hexdigest() != checksums[1]:
            raise SystemExit(f"Patched bgfx checksum mismatch: {name}")

if __name__ == "__main__":
    main()
