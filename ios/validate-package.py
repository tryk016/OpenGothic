#!/usr/bin/env python3

import argparse
import json
import plistlib
import sys
import zipfile
from pathlib import Path
from urllib.parse import urlparse


class ValidationError(RuntimeError):
    pass


def read_info(package: Path) -> dict:
    if package.is_dir() and package.suffix == ".app":
        info_path = package / "Info.plist"
        info = plistlib.loads(info_path.read_bytes())
        executable = package / info.get("CFBundleExecutable", "")
        if not executable.is_file():
            raise ValidationError(f"missing application executable: {executable.name}")
        return info

    if package.suffix.lower() != ".ipa":
        raise ValidationError("package must be an .ipa file or an .app directory")
    with zipfile.ZipFile(package) as archive:
        infos = [name for name in archive.namelist()
                 if name.startswith("Payload/") and name.count("/") == 2
                 and name.endswith(".app/Info.plist")]
        if len(infos) != 1:
            raise ValidationError(f"expected one application in Payload, found {len(infos)}")
        info = plistlib.loads(archive.read(infos[0]))
        executable = infos[0].removesuffix("Info.plist") + info.get("CFBundleExecutable", "")
        if executable not in archive.namelist():
            raise ValidationError("application executable is missing from the IPA")
        return info


def require_equal(actual: object, expected: str | None, label: str) -> None:
    if expected is not None and str(actual) != expected:
        raise ValidationError(f"{label}: expected {expected!r}, found {actual!r}")


def validate_manifest(path: Path, package: Path, info: dict) -> None:
    source = json.loads(path.read_text(encoding="utf-8"))
    bundle_id = info["CFBundleIdentifier"]
    version = info["CFBundleShortVersionString"]
    apps = [app for app in source.get("apps", [])
            if app.get("bundleIdentifier") == bundle_id]
    if len(apps) != 1:
        raise ValidationError(f"manifest must contain one app with bundle ID {bundle_id}")

    app = apps[0]
    require_equal(app.get("version"), version, "manifest app version")
    versions = [item for item in app.get("versions", []) if item.get("version") == version]
    if len(versions) != 1:
        raise ValidationError(f"manifest must contain one release entry for version {version}")
    release = versions[0]
    require_equal(release.get("minOSVersion"), str(info["MinimumOSVersion"]),
                  "manifest minimum iOS version")

    if package.suffix.lower() == ".ipa":
        package_size = package.stat().st_size
        for entry in (app, release):
            if entry.get("size") != package_size:
                raise ValidationError("manifest package size does not match the IPA")
            filename = Path(urlparse(entry.get("downloadURL", "")).path).name
            if filename != package.name:
                raise ValidationError("manifest download URL does not match the IPA filename")


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate an OpenGothic iOS package")
    parser.add_argument("package", type=Path)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--expected-version")
    parser.add_argument("--expected-bundle-id")
    parser.add_argument("--expected-min-ios")
    args = parser.parse_args()

    try:
        info = read_info(args.package)
        require_equal(info.get("CFBundlePackageType"), "APPL", "bundle type")
        require_equal(info.get("CFBundleShortVersionString"), args.expected_version,
                      "application version")
        require_equal(info.get("CFBundleIdentifier"), args.expected_bundle_id,
                      "bundle identifier")
        require_equal(info.get("MinimumOSVersion"), args.expected_min_ios,
                      "minimum iOS version")
        if args.manifest is not None:
            validate_manifest(args.manifest, args.package, info)
    except (KeyError, OSError, ValueError, zipfile.BadZipFile, ValidationError) as error:
        print(f"package validation failed: {error}", file=sys.stderr)
        return 1

    print(f"validated {args.package}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
