"""Package an installed Windows runtime with provenance, notices and checksums."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import zipfile


ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def notice_files(directory):
    for folder, dirs, files in os.walk(directory):
        dirs[:] = [d for d in dirs
                   if d not in {".git", "node_modules", "CMakeFiles", "__pycache__", "build", "bin.v2", "DL_CACHE"}
                   and not (Path(folder) / d).is_junction()]
        for name in files:
            upper = name.upper()
            if (upper.startswith(("LICENSE", "LICENCE", "COPYING", "NOTICE", "COPYRIGHT", "GPL", "LGPL"))
                    or upper in {"FTL.TXT", "OCCT_LGPL_EXCEPTION.TXT"}):
                path = Path(folder) / name
                if path.stat().st_size < 2_000_000 and path.suffix.lower() not in {".cpp", ".hpp", ".py", ".pl"}:
                    yield path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--extra-notices", type=Path)
    parser.add_argument("--binary-source-commit", help="Original commit when repackaging previously compiled binaries")
    args = parser.parse_args()
    runtime = args.runtime.resolve()
    output = args.output.resolve()
    if runtime == output or runtime in output.parents:
        parser.error("Output must be outside the runtime directory")
    for name in ("snapmaker-orca.exe", "Snapmaker_Orca.dll", "resources", "mesa/opengl32.dll"):
        if not (runtime / name).exists():
            parser.error(f"Required runtime component missing: {name}")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    if subprocess.check_output(["git", "status", "--porcelain", "--untracked-files=no"], cwd=ROOT, text=True).strip():
        parser.error("Commit tracked source changes before packaging")
    output.mkdir(parents=True, exist_ok=True)
    archive = output / f"Per-Wall-Loop-Color-Index-{args.tag}-Windows-x64.zip"
    if archive.exists():
        parser.error(f"Archive already exists: {archive}")
    files = {}
    for path in sorted(runtime.rglob("*")):
        if path.is_file():
            if path.suffix.lower() in {".pdb", ".obj", ".lib", ".log"}:
                continue
            files[path.relative_to(runtime).as_posix()] = path
    files["LICENSE.txt"] = ROOT / "LICENSE.txt"
    files["PerWallLoopColorIndex.md"] = ROOT / "doc/PerWallLoopColorIndex.md"
    files["WindowsWallLoopBuild.md"] = ROOT / "doc/WindowsWallLoopBuild.md"
    for base in (ROOT / "deps", ROOT / "deps_src", ROOT / "src"):
        # ExternalProject source trees retain the upstream dependency licenses.
        roots = [base] if base.name != "deps" else [p for p in base.iterdir() if p.is_dir() and p.name != "build"]
        if base.name == "deps":
            roots += sorted((base / "build").glob("dep_*-prefix/src/dep_*"))
        for source in roots:
            if source.is_dir():
                print(f"Collecting notices: {source.relative_to(ROOT)}", flush=True)
                for path in notice_files(source):
                    files["third-party-notices/" + path.relative_to(ROOT).as_posix()] = path
    if args.extra_notices:
        for path in args.extra_notices.rglob("*"):
            if path.is_file():
                files["third-party-notices/extra/" + path.relative_to(args.extra_notices).as_posix()] = path
    info = {
        "release": args.tag,
        "source_commit": commit,
        "binary_source_commit": args.binary_source_commit or commit,
        "source_url": f"https://github.com/setnev/per-wall-loop-color-index/tree/{commit}",
        "upstream_version": "Snapmaker Orca 2.4.1",
        "platform": "Windows x64",
        "unsigned": True,
        "build": "Release slicing engine; GUI optimization disabled in the first preview",
    }
    readme = f"""Per Wall Loop Color Index - {args.tag}

Extract this entire folder, then run snapmaker-orca.exe.
Advanced > Multimaterial > Filament for Features > Per Wall Loop Color Index.
Use plain filament numbers outside-in: 2,1,3 gives 2,1,3,3 for four walls.

Community preview, based on Snapmaker Orca 2.4.1. Unsigned Windows x64 build.
Source: {info['source_url']}
Downloads: https://github.com/setnev/per-wall-loop-color-index/releases
License: LICENSE.txt. Dependency notices: third-party-notices/.
See PerWallLoopColorIndex.md for behavior and WindowsWallLoopBuild.md for builds.
"""
    generated = {
        "RELEASE-INFO.json": json.dumps(info, indent=2) + "\n",
        "README-FIRST.txt": readme,
    }
    manifest = {name: digest(path) for name, path in sorted(files.items())}
    manifest.update({name: hashlib.sha256(value.encode()).hexdigest() for name, value in generated.items()})
    generated["FILE-SHA256SUMS.txt"] = "".join(f"{value}  {name}\n" for name, value in sorted(manifest.items()))
    with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as bundle:
        print(f"Writing {len(files)} files to {archive.name}", flush=True)
        for name, path in sorted(files.items()):
            bundle.write(path, "Per-Wall-Loop-Color-Index/" + name)
        for name, value in generated.items():
            bundle.writestr("Per-Wall-Loop-Color-Index/" + name, value)
    (output / "SHA256SUMS.txt").write_text(f"{digest(archive)}  {archive.name}\n", encoding="utf-8")
    print(json.dumps({"archive": str(archive), "files": len(manifest), "commit": commit}))


if __name__ == "__main__":
    main()
