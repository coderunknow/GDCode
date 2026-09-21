#!/usr/bin/env python3
"""Cross-check the object ids in core/src/Catalog.cpp against a GD object-id
dataset (CSV with lines `id,name`, e.g. objects.csv from the gd-info-explorer
project - not redistributed here, pass its path).

    python3 tools/check_gd_api.py /path/to/objects.csv

Exit code 0 when every catalog id exists in the dataset. The names are printed
so a reviewer can eyeball that e.g. `orb yellow` really is "Yellow Jump Orb".
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def load_dataset(path: Path) -> dict[int, str]:
    rows: dict[int, str] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line.strip():
            continue
        head, _, name = line.partition(",")
        try:
            rows[int(head)] = name.strip()
        except ValueError:
            continue
    return rows


def load_catalog() -> list[tuple[str, str, int]]:
    src = (ROOT / "core" / "src" / "Catalog.cpp").read_text(encoding="utf-8")
    entries: list[tuple[str, str, int]] = []
    current_type = None
    for line in src.splitlines():
        m_type = re.search(r'\{"([a-zA-Z]+)",\s*(-?\d+),', line)
        if m_type:
            current_type = m_type.group(1)
            default_id = int(m_type.group(2))
            if default_id > 0:
                entries.append((current_type, "", default_id))
            continue
        m_var = re.search(r'\{"([a-zA-Z0-9]+)",\s*(\d+)\}', line)
        if m_var and current_type:
            entries.append((current_type, m_var.group(1), int(m_var.group(2))))
    return entries


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    dataset = load_dataset(Path(sys.argv[1]))
    if not dataset:
        print("dataset is empty or unreadable")
        return 2
    catalog = load_catalog()
    if not catalog:
        print("could not parse any catalog entries")
        return 2

    missing = 0
    for type_, variant, gd_id in catalog:
        name = dataset.get(gd_id)
        label = f"{type_} {variant}".strip()
        if name is None:
            print(f"MISSING  {label:20s} id {gd_id}")
            missing += 1
        else:
            print(f"ok       {label:20s} id {gd_id:5d}  {name}")

    max_id = max(dataset)
    src = (ROOT / "core" / "include" / "gdcode" / "Catalog.hpp").read_text(encoding="utf-8")
    m = re.search(r"kMaxRawObjectId\s*=\s*(\d+)", src)
    if m and int(m.group(1)) != max_id:
        print(f"NOTE     kMaxRawObjectId is {m.group(1)} but the dataset's highest id is {max_id}")

    print(f"\n{len(catalog)} catalog entries checked, {missing} missing")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
