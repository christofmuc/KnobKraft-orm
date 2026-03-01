#!/usr/bin/env python3
"""Generate supported synth artifacts from docs/data/supported-synths.yml."""

from __future__ import annotations

import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

try:
    import yaml
except ImportError as exc:  # pragma: no cover
    raise SystemExit(
        "PyYAML is required. Install dependencies with `pip install -r requirements.txt`."
    ) from exc

ROOT = Path(__file__).resolve().parents[1]
DATA_FILE = ROOT / "docs" / "data" / "supported-synths.yml"
README_FILE = ROOT / "README.md"
DOCS_FILE = ROOT / "docs" / "supported-synths.md"
JSON_FILE = ROOT / "data" / "supported-synths.json"

README_BEGIN = "<!-- BEGIN:SUPPORTED_SYNTHS -->"
README_END = "<!-- END:SUPPORTED_SYNTHS -->"
DOCS_BEGIN = "<!-- BEGIN:SUPPORTED_SYNTHS_TABLE -->"
DOCS_END = "<!-- END:SUPPORTED_SYNTHS_TABLE -->"

STATUS_MAP = {
    "works": "works",
    "stable": "works",
    "beta": "beta",
    "alpha": "alpha",
    "in_progress": "in_progress",
    "in progress": "in_progress",
    "in progess": "in_progress",
}

STATUS_LABELS = {
    "works": "Works",
    "beta": "Beta",
    "alpha": "Alpha",
    "in_progress": "In Progress",
}

TYPE_MAP = {
    "native": "native",
    "adaptation": "adaptation",
}


def escape_md(value: str) -> str:
    return value.replace("|", "\\|").replace("\n", " ").strip()


def normalize_status(value: Any) -> str:
    status = str(value or "").strip().lower().replace("-", " ")
    status = " ".join(status.split())
    normalized = STATUS_MAP.get(status)
    if not normalized:
        raise ValueError(f"Unsupported status: {value!r}")
    return normalized


def normalize_type(value: Any) -> str:
    synth_type = str(value or "").strip().lower()
    normalized = TYPE_MAP.get(synth_type)
    if not normalized:
        raise ValueError(f"Unsupported type: {value!r}")
    return normalized


def load_synths() -> list[dict[str, str]]:
    raw = yaml.safe_load(DATA_FILE.read_text(encoding="utf-8"))
    if not isinstance(raw, dict) or not isinstance(raw.get("synths"), list):
        raise ValueError(f"Invalid schema in {DATA_FILE}: expected top-level 'synths' list")

    result: list[dict[str, str]] = []
    for index, row in enumerate(raw["synths"], start=1):
        if not isinstance(row, dict):
            raise ValueError(f"Invalid row #{index}: expected object")

        manufacturer = str(row.get("manufacturer", "")).strip()
        synth = str(row.get("synth", "")).strip()
        kudos = str(row.get("kudos", "")).strip()

        if not manufacturer:
            raise ValueError(f"Invalid row #{index}: missing manufacturer")
        if not synth:
            raise ValueError(f"Invalid row #{index}: missing synth")

        status = normalize_status(row.get("status", ""))
        synth_type = normalize_type(row.get("type", ""))

        result.append(
            {
                "manufacturer": manufacturer,
                "synth": synth,
                "status": status,
                "statusLabel": STATUS_LABELS[status],
                "type": synth_type,
                "kudos": kudos,
            }
        )

    return result


def build_markdown_table(synths: list[dict[str, str]]) -> str:
    lines = [
        "| Manufacturer | Synth | Status | Type | Kudos |",
        "| --- | --- | --- | --- | --- |",
    ]

    for row in synths:
        status = row["status"].replace("_", " ")
        lines.append(
            "| "
            f"{escape_md(row['manufacturer'])} | "
            f"{escape_md(row['synth'])} | "
            f"{escape_md(status)} | "
            f"{escape_md(row['type'])} | "
            f"{escape_md(row['kudos'])} |"
        )

    return "\n".join(lines)


def build_docs_grouped_content(synths: list[dict[str, str]]) -> str:
    grouped: dict[str, list[dict[str, str]]] = {}
    order: list[str] = []

    for row in synths:
        manufacturer = row["manufacturer"]
        if manufacturer not in grouped:
            grouped[manufacturer] = []
            order.append(manufacturer)
        grouped[manufacturer].append(row)

    def slugify(value: str) -> str:
        slug = value.lower()
        slug = re.sub(r"[^a-z0-9]+", "-", slug)
        slug = slug.strip("-")
        return slug or "unknown"

    lines: list[str] = []
    lines.append("## Manufacturers")
    lines.append("")
    lines.extend([f"- [{m}](#manufacturer-{slugify(m)})" for m in order])
    lines.append("")

    for manufacturer in order:
        lines.append(f'<a id="manufacturer-{slugify(manufacturer)}"></a>')
        lines.append(f"## {manufacturer}")
        lines.append("")
        lines.append("| Synth | Status | Type | Kudos |")
        lines.append("| --- | --- | --- | --- |")
        for row in grouped[manufacturer]:
            status = row["status"].replace("_", " ")
            lines.append(
                "| "
                f"{escape_md(row['synth'])} | "
                f"{escape_md(status)} | "
                f"{escape_md(row['type'])} | "
                f"{escape_md(row['kudos'])} |"
            )
        lines.append("")

    return "\n".join(lines).strip()


def replace_marked_block(text: str, begin_marker: str, end_marker: str, replacement: str) -> str:
    pattern = re.compile(
        rf"(?ms)^{re.escape(begin_marker)}\n.*?^{re.escape(end_marker)}$"
    )
    new_block = f"{begin_marker}\n{replacement}\n{end_marker}"
    if not pattern.search(text):
        raise ValueError(f"Could not find marker block: {begin_marker} ... {end_marker}")
    return pattern.sub(new_block, text, count=1)


def update_markdown_files(readme_table: str, docs_content: str) -> None:
    readme = README_FILE.read_text(encoding="utf-8")
    readme = replace_marked_block(readme, README_BEGIN, README_END, readme_table)
    README_FILE.write_text(readme, encoding="utf-8")

    docs = DOCS_FILE.read_text(encoding="utf-8")
    docs = replace_marked_block(docs, DOCS_BEGIN, DOCS_END, docs_content)
    DOCS_FILE.write_text(docs, encoding="utf-8")


def write_json(synths: list[dict[str, str]]) -> None:
    JSON_FILE.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "count": len(synths),
        "synths": synths,
    }
    JSON_FILE.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    synths = load_synths()
    readme_table = build_markdown_table(synths)
    docs_content = build_docs_grouped_content(synths)
    update_markdown_files(readme_table, docs_content)
    write_json(synths)
    print(f"Generated {len(synths)} synth rows -> README/docs/data JSON")
    return 0


if __name__ == "__main__":
    sys.exit(main())
