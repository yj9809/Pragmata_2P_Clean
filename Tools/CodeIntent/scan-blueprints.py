import json
import os
from datetime import datetime, timezone

import unreal


def _text(value):
    try:
        return str(value)
    except Exception:
        return ""


def _class_name(asset_data):
    try:
        value = getattr(asset_data, "asset_class_path")
        package_name = _text(getattr(value, "package_name", ""))
        asset_name = _text(getattr(value, "asset_name", ""))
        if package_name or asset_name:
            return f"{package_name}.{asset_name}".strip(".")
    except Exception:
        pass

    for attr in ("asset_class",):
        try:
            value = getattr(asset_data, attr)
            text = _text(value)
            if text:
                return text
        except Exception:
            pass
    return ""


def _asset_path(asset_data):
    try:
        return asset_data.get_full_name().split(" ", 1)[-1]
    except Exception:
        pass
    try:
        return _text(asset_data.object_path)
    except Exception:
        return ""


def _package_name(asset_data):
    try:
        return _text(asset_data.package_name)
    except Exception:
        return ""


def _dependency_options():
    try:
        return unreal.AssetRegistryDependencyOptions(
            include_soft_package_references=True,
            include_hard_package_references=True,
            include_searchable_names=True,
            include_soft_management_references=True,
            include_hard_management_references=True,
        )
    except Exception:
        try:
            return unreal.AssetRegistryDependencyOptions(True, True, True, True, True)
        except Exception:
            return None


def _get_dependencies(registry, package_name):
    if not package_name:
        return []
    options = _dependency_options()
    try:
        values = registry.get_dependencies(package_name, options) if options else registry.get_dependencies(package_name)
        return sorted({_text(v) for v in values if _text(v)})
    except Exception as exc:
        return [f"<dependency scan failed: {exc}>"]


def _get_referencers(registry, package_name):
    if not package_name:
        return []
    options = _dependency_options()
    try:
        values = registry.get_referencers(package_name, options) if options else registry.get_referencers(package_name)
        return sorted({_text(v) for v in values if _text(v)})
    except Exception as exc:
        return [f"<referencer scan failed: {exc}>"]


def _blueprint_parent(asset_path):
    try:
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    except Exception:
        return ""

    for attr in ("parent_class", "generated_class"):
        try:
            value = getattr(asset, attr)
            if attr == "parent_class" and value:
                return value.get_path_name()
        except Exception:
            pass
    return ""


def _interesting_edges(asset_path, dependencies):
    needles = (
        "Pragmata",
        "Hugh",
        "Diana",
        "Hack",
        "Damage",
        "Weapon",
        "HUD",
        "Widget",
        "Puzzle",
        "Monster",
        "Enemy",
    )
    edges = []
    for dep in dependencies:
        if dep.startswith("/Script/Pragmata_2P") or any(n in dep for n in needles):
            edges.append({"from": asset_path, "to": dep})
    return edges


def main():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = registry.get_assets_by_path("/Game", recursive=True)

    blueprints = []
    edges = []
    class_counts = {}

    for asset_data in assets:
        cls = _class_name(asset_data)
        if "Blueprint" not in cls:
            continue

        asset_path = _asset_path(asset_data)
        package_name = _package_name(asset_data)
        deps = _get_dependencies(registry, package_name)
        refs = _get_referencers(registry, package_name)
        parent = _blueprint_parent(asset_path)

        class_counts[cls] = class_counts.get(cls, 0) + 1
        edges.extend(_interesting_edges(asset_path, deps))

        blueprints.append(
            {
                "asset": asset_path,
                "package": package_name,
                "class": cls,
                "parent_class": parent,
                "dependencies": deps,
                "referencers": refs,
                "dependency_count": len(deps),
                "referencer_count": len(refs),
            }
        )

    blueprints.sort(key=lambda item: item["asset"])

    project_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    out_dir = os.path.join(project_dir, "Saved", "CodeIntent", "blueprints")
    os.makedirs(out_dir, exist_ok=True)

    report = {
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "source": "UnreelMCP python.execute_script + Unreal AssetRegistry",
        "blueprint_count": len(blueprints),
        "class_counts": dict(sorted(class_counts.items())),
        "blueprints": blueprints,
        "interesting_edges": edges,
    }

    out_path = os.path.join(out_dir, "blueprint-asset-scan.json")
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)

    summary_path = os.path.join(out_dir, "blueprint-asset-scan-summary.md")
    with open(summary_path, "w", encoding="utf-8") as f:
        f.write("# Blueprint asset scan\n\n")
        f.write(f"- generated_at_utc: {report['generated_at_utc']}\n")
        f.write(f"- blueprint_count: {len(blueprints)}\n")
        f.write("- output: `Saved/CodeIntent/blueprints/blueprint-asset-scan.json`\n\n")
        f.write("## class counts\n\n")
        for cls, count in sorted(class_counts.items()):
            f.write(f"- `{cls}`: {count}\n")
        f.write("\n## interesting edges\n\n")
        for edge in edges[:200]:
            f.write(f"- `{edge['from']}` -> `{edge['to']}`\n")

    print(json.dumps({"blueprint_count": len(blueprints), "edges": len(edges), "output": out_path}, ensure_ascii=False))


main()
