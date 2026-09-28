#!/usr/bin/env python3
"""Write a concise text summary from Grype's JSON vulnerability report."""

import json
import sys
from collections import Counter
from pathlib import Path


def main() -> None:
    if len(sys.argv) != 4:
        raise SystemExit(
            f"usage: {Path(sys.argv[0]).name} "
            "<grype-report.json> <sbom.cdx.json> <summary.txt>"
        )

    report_path = Path(sys.argv[1])
    sbom_path = Path(sys.argv[2])
    output_path = Path(sys.argv[3])
    report = json.loads(report_path.read_text(encoding="utf-8"))
    matches = report.get("matches")
    if not isinstance(matches, list):
        raise SystemExit(f"Grype report has no matches array: {report_path}")
    bom = json.loads(sbom_path.read_text(encoding="utf-8"))
    unresolved = [
        component
        for component in bom.get("components", [])
        if component.get("purl", "").startswith("pkg:deb/")
        and not component.get("version")
    ]

    counts = Counter(
        (match.get("vulnerability") or {}).get("severity") or "Unknown"
        for match in matches
    )
    lines = [f"Vulnerability matches: {len(matches)}"]
    root_properties = {
        prop.get("name"): prop.get("value")
        for prop in bom.get("metadata", {}).get("component", {}).get("properties", [])
    }
    if root_properties.get("basler:sbom:classification") == "build":
        lines.append(
            "Scope: direct build dependencies (linker and header inputs); "
            "this is not a complete runtime inventory."
        )
    if any(
        component.get("purl", "").startswith("pkg:conan/pylon-core@")
        for component in bom.get("components", [])
    ):
        lines.append(
            "pylon-core is identified by its Conan PURL; advisory coverage for "
            "this package depends on the vulnerability data sources."
        )
    if unresolved:
        lines.append(
            f"Debian dependency components without resolved versions: {len(unresolved)}"
        )
        lines.append(
            "These declared dependencies are inventoried but cannot be reliably "
            "matched to version-specific advisories:"
        )
        for component in unresolved:
            constraints = [
                property_value["value"]
                for property_value in component.get("properties", [])
                if property_value.get("name") == "basler:debian:declared-dependency"
            ]
            detail = f" ({'; '.join(constraints)})" if constraints else ""
            lines.append(f"- {component.get('name', 'unknown')}{detail}")
    if counts:
        lines.append(
            "Severity counts: "
            + ", ".join(f"{severity}: {count}" for severity, count in sorted(counts.items()))
        )
        lines.append("")
        for match in matches:
            vulnerability = match.get("vulnerability") or {}
            artifact = match.get("artifact") or {}
            fixed_versions = (vulnerability.get("fix") or {}).get("versions") or []
            lines.append(
                "{id} | {severity} | {name}@{version} | fixed: {fixed}".format(
                    id=vulnerability.get("id", "unknown"),
                    severity=vulnerability.get("severity") or "Unknown",
                    name=artifact.get("name", "unknown"),
                    version=artifact.get("version", "unknown"),
                    fixed=", ".join(fixed_versions) or "none listed",
                )
            )
    else:
        lines.append("No known vulnerabilities matched the scanned SBOM.")
    if unresolved:
        lines.append(
            "A zero-match result is not a clean bill of health for these "
            "versionless Debian dependency declarations."
        )

    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
