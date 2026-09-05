#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
verify_i18n_alignment.py

Industrial-Grade Document AST Structural Tree & Multi-Language Alignment Suite:
1. Bidirectional 1:1 Directory Tree Symmetry Check (docs/zh/ <-> docs/en/, or arbitrary --source-dir <-> --target-dir)
2. True Hierarchical Document AST Structural Tree Parsing & Sequential Diff:
   - Full Heading Tree Hierarchy (Root -> H1 -> H2 -> H3 -> H4)
   - Intra-Section Ordered Block Sequence ([Alert, Diagram, Table(cols), Code(lang)])
   - Section Numbering Pattern & Sequence Topology (1., 1.1, §x, ADR-x)
   - Table Schema (Column count matching)
   - Code Fences (nested variable depth ```, ````, `````)
   - Visual Assets (![caption](url) & Mermaid)
3. Structural Path Discrepancy Reporting (e.g., "Under Section ## 2. Architecture -> ### 2.1: Missing Table(4)")
4. CI/CD Quality Gates & Structured Reporting (Markdown / JSON)

Usage:
  python docs/scripts/verify_i18n_alignment.py                         # Tree alignment check
  python docs/scripts/verify_i18n_alignment.py --check-content        # Full AST tree structural hierarchy check
  python docs/scripts/verify_i18n_alignment.py --source-dir docs/design --target-dir docs/zh/design --check-content
  python docs/scripts/verify_i18n_alignment.py --strict --strict-content --min-score 85.0
"""

import os
import re
import sys
import json
import argparse
from typing import Dict, List, Tuple, Any, Optional

if hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DOCS_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
DEFAULT_ZH_DIR = os.path.join(DOCS_DIR, "zh")
DEFAULT_EN_DIR = os.path.join(DOCS_DIR, "en")

SSOT_DOMAINS = ["design", "tech-designs", "product"]
META_PATTERN = re.compile(r'<!--\s*i18n-meta\s*(.*?)\s*-->', re.DOTALL | re.IGNORECASE)

class BlockElement:
    """Represents an ordered leaf structural block inside a document section."""
    def __init__(self, kind: str, meta: Optional[Dict[str, Any]] = None):
        self.kind = kind  # "code", "mermaid", "image", "table", "alert", "list", "paragraph"
        self.meta = meta or {}

    def signature(self) -> str:
        if self.kind == "code":
            return f"Code({self.meta.get('lang', 'text')})"
        elif self.kind == "table":
            return f"Table({self.meta.get('cols', 0)}cols)"
        elif self.kind == "alert":
            return f"Alert({self.meta.get('type', 'NOTE')})"
        elif self.kind == "mermaid":
            return "MermaidDiagram"
        elif self.kind == "image":
            return "ImageIllustration"
        elif self.kind == "list":
            return "ListBlock"
        elif self.kind == "paragraph":
            return "Paragraph"
        return self.kind

    def __repr__(self):
        return self.signature()


class SectionNode:
    """Represents a hierarchical node in the document AST heading tree."""
    def __init__(self, level: int, title: str, num_prefix: str = "", path: str = ""):
        self.level = level                  # 0 for ROOT, 1 for #, 2 for ##, 3 for ###, 4 for ####
        self.title = title                  # Clean heading title
        self.num_prefix = num_prefix        # "1.", "1.1", "ADR-0004", "Phase 1"
        self.path = path                    # Breadcrumb path: "ROOT > 1. Overview > 1.1 Architecture"
        self.blocks: List[BlockElement] = [] # Ordered sequence of leaf blocks in this section
        self.children: List['SectionNode'] = [] # Nested sub-sections

    def add_block(self, kind: str, meta: Optional[Dict[str, Any]] = None):
        # Collapse contiguous paragraphs / lists to avoid micro-fragmentation noise
        if kind in ("paragraph", "list") and self.blocks and self.blocks[-1].kind == kind:
            return
        self.blocks.append(BlockElement(kind, meta))

    def get_block_signature_sequence(self) -> List[str]:
        """Returns the ordered signature list of significant blocks (excluding plain text noise)."""
        significant = ["code", "mermaid", "image", "table", "alert"]
        return [b.signature() for b in self.blocks if b.kind in significant]

    def total_nodes_count(self) -> int:
        count = 1
        for child in self.children:
            count += child.total_nodes_count()
        return count

    def total_blocks_count(self) -> int:
        count = len(self.blocks)
        for child in self.children:
            count += child.total_blocks_count()
        return count


class DocumentAST:
    """Full Document Abstract Syntax Tree with sequential block ordering."""
    def __init__(self, filepath: str):
        self.filepath = filepath
        self.exists = os.path.exists(filepath)
        self.raw_content = ""
        self.clean_content = ""
        self.root = SectionNode(level=0, title="DOCUMENT_ROOT", path="ROOT")
        self.metadata: Dict[str, str] = {}
        
        # Summary metrics
        self.h_counts: Dict[int, int] = {1: 0, 2: 0, 3: 0, 4: 0}
        self.total_tables = 0
        self.total_code_blocks = 0
        self.total_mermaid = 0
        self.total_images = 0
        self.total_alerts = 0
        self.total_links = 0
        
        if self.exists:
            self._parse_ast()

    def _parse_ast(self):
        try:
            with open(self.filepath, "r", encoding="utf-8-sig", errors="ignore") as f:
                self.raw_content = f.read()
        except Exception:
            return

        # 1. Parse frontmatter i18n-meta
        match = META_PATTERN.search(self.raw_content)
        if match:
            meta_str = match.group(1)
            for line in meta_str.splitlines():
                if ":" in line:
                    k, v = line.split(":", 1)
                    self.metadata[k.strip().lower()] = v.strip()

        content = META_PATTERN.sub("", self.raw_content)
        self.clean_content = content
        lines = content.splitlines()

        # AST stack for hierarchy building
        stack: List[SectionNode] = [self.root]

        in_code_fence = False
        fence_char = ""
        fence_len = 0
        current_code_lang = ""
        in_table = False

        for line in lines:
            stripped = line.strip()

            # --- Code Block Fences (Supports variable length ```, ````, ~~~) ---
            fence_match = re.match(r'^(`{3,}|~{3,})(.*)$', stripped)
            if fence_match:
                marker = fence_match.group(1)
                info = fence_match.group(2).strip().lower()
                
                if not in_code_fence:
                    in_code_fence = True
                    fence_char = marker[0]
                    fence_len = len(marker)
                    current_code_lang = info if info else "text"
                    
                    if current_code_lang == "mermaid":
                        self.total_mermaid += 1
                        stack[-1].add_block("mermaid", {"lang": "mermaid"})
                    else:
                        self.total_code_blocks += 1
                        stack[-1].add_block("code", {"lang": current_code_lang})
                    continue
                else:
                    if marker[0] == fence_char and len(marker) >= fence_len:
                        in_code_fence = False
                        fence_char = ""
                        fence_len = 0
                        current_code_lang = ""
                        continue

            if in_code_fence:
                continue

            # --- Headings (#, ##, ###, ####) ---
            heading_match = re.match(r'^(#{1,6})\s+(.+)$', stripped)
            if heading_match:
                level = len(heading_match.group(1))
                full_title = heading_match.group(2).strip()
                
                if level in self.h_counts:
                    self.h_counts[level] += 1

                # Extract numbering prefix (e.g., "1.", "1.2", "§1", "ADR-0004", "Phase 1")
                num_prefix_match = re.match(r'^((?:§|\b(?:Phase|ADR|Step|Task|Track)\b\s*[-0-9A-Za-z]+|\d+(?:\.\d+)*)\.?)\s+(.+)$', full_title, re.IGNORECASE)
                if num_prefix_match:
                    num_prefix = num_prefix_match.group(1).strip()
                    clean_title = num_prefix_match.group(2).strip()
                else:
                    num_prefix = ""
                    clean_title = full_title

                # Pop stack until top node has lower level
                while len(stack) > 1 and stack[-1].level >= level:
                    stack.pop()

                parent = stack[-1]
                path = f"{parent.path} > {clean_title}" if parent.level > 0 else clean_title
                new_node = SectionNode(level=level, title=clean_title, num_prefix=num_prefix, path=path)
                parent.children.append(new_node)
                stack.append(new_node)
                continue

            # --- Images: ![caption](url) ---
            img_matches = re.findall(r'!\[.*?\]\(.*?\)', line)
            if img_matches:
                self.total_images += len(img_matches)
                for _ in img_matches:
                    stack[-1].add_block("image")

            # --- Links: [text](url) ---
            line_no_imgs = re.sub(r'!\[.*?\]\(.*?\)', '', line)
            link_matches = re.findall(r'\[.*?\]\(.*?\)', line_no_imgs)
            if link_matches:
                self.total_links += len(link_matches)

            # --- Tables ---
            if stripped.startswith("|") and stripped.endswith("|") and len(stripped) > 2:
                if re.match(r'^\|[\s\-:|]+\|$', stripped):
                    pass
                else:
                    cols = len([c for c in stripped.split("|")[1:-1]])
                    if not in_table:
                        in_table = True
                        self.total_tables += 1
                        stack[-1].add_block("table", {"cols": cols})
                continue
            else:
                in_table = False

            # --- Alerts (> [!NOTE], > [!WARNING], etc.) ---
            alert_match = re.match(r'^>\s*\[!(NOTE|TIP|IMPORTANT|WARNING|CAUTION)\]', stripped, re.IGNORECASE)
            if alert_match:
                alert_type = alert_match.group(1).upper()
                self.total_alerts += 1
                stack[-1].add_block("alert", {"type": alert_type})
                continue

            # --- Lists & Paragraphs ---
            if re.match(r'^(\*|-|\+|\d+\.)\s+', stripped):
                stack[-1].add_block("list")
                continue

            if stripped and not stripped.startswith(">") and not stripped.startswith("---") and not stripped.startswith("***"):
                stack[-1].add_block("paragraph")


def diff_section_subtrees(src_node: SectionNode, tgt_node: SectionNode, diffs: List[str], path_prefix: str = "") -> Tuple[int, int]:
    """Recursively compares two section subtrees, checking ordered block sequences and nested child sections."""
    matched_score_points = 0
    total_score_points = 0

    cur_path = f"{path_prefix} > {src_node.title}" if path_prefix else src_node.title

    # 1. Compare Section Level & Numbering Prefix
    total_score_points += 2
    if src_node.level == tgt_node.level:
        matched_score_points += 2
    else:
        diffs.append(f"[{cur_path}] Level mismatch: Source=H{src_node.level} vs Target=H{tgt_node.level}")

    if src_node.num_prefix or tgt_node.num_prefix:
        total_score_points += 2
        if src_node.num_prefix.lower() == tgt_node.num_prefix.lower():
            matched_score_points += 2
        else:
            diffs.append(f"[{cur_path}] Section numbering mismatch: Source '{src_node.num_prefix}' vs Target '{tgt_node.num_prefix}'")

    # 2. Compare Ordered Block Sequence in this Section
    src_blocks = src_node.get_block_signature_sequence()
    tgt_blocks = tgt_node.get_block_signature_sequence()

    block_points = max(len(src_blocks), len(tgt_blocks)) * 2
    total_score_points += block_points

    if src_blocks == tgt_blocks:
        matched_score_points += block_points
    else:
        # Check order or content discrepancy
        diffs.append(f"[{cur_path}] Block sequence mismatch:\n"
                     f"          ↳ Source: {src_blocks if src_blocks else '[]'}\n"
                     f"          ↳ Target: {tgt_blocks if tgt_blocks else '[]'}")
        # Partial credit for common elements in same order (LCS)
        lcs_count = 0
        i, j = 0, 0
        while i < len(src_blocks) and j < len(tgt_blocks):
            if src_blocks[i] == tgt_blocks[j]:
                lcs_count += 1
                i += 1
                j += 1
            elif src_blocks[i] in tgt_blocks[j:]:
                j += 1
            else:
                i += 1
        matched_score_points += lcs_count * 2

    # 3. Compare Child Sub-Sections
    src_children = src_node.children
    tgt_children = tgt_node.children

    child_points = max(len(src_children), len(tgt_children)) * 4
    total_score_points += child_points

    if len(src_children) != len(tgt_children):
        diffs.append(f"[{cur_path}] Child sections count mismatch: Source has {len(src_children)} sub-sections vs Target has {len(tgt_children)}")
        matched_score_points += min(len(src_children), len(tgt_children)) * 4
    else:
        matched_score_points += child_points

    min_children = min(len(src_children), len(tgt_children))
    for idx in range(min_children):
        child_m, child_t = diff_section_subtrees(src_children[idx], tgt_children[idx], diffs, cur_path)
        matched_score_points += child_m
        total_score_points += child_t

    return matched_score_points, total_score_points


def compare_document_ast_trees(src_doc: DocumentAST, tgt_doc: DocumentAST) -> Dict[str, Any]:
    """Performs full recursive hierarchical AST tree diff between two documents."""
    diffs = []
    
    # 1. Global Metric Summary Diffs
    for lvl in [1, 2, 3]:
        if src_doc.h_counts[lvl] != tgt_doc.h_counts[lvl]:
            diffs.append(f"Global H{lvl} Heading count mismatch: Source={src_doc.h_counts[lvl]} vs Target={tgt_doc.h_counts[lvl]}")

    if src_doc.total_code_blocks != tgt_doc.total_code_blocks:
        diffs.append(f"Global Code block count mismatch: Source={src_doc.total_code_blocks} vs Target={tgt_doc.total_code_blocks}")
    if src_doc.total_mermaid != tgt_doc.total_mermaid:
        diffs.append(f"Global Mermaid diagram count mismatch: Source={src_doc.total_mermaid} vs Target={tgt_doc.total_mermaid}")
    if src_doc.total_images != tgt_doc.total_images:
        diffs.append(f"Global Illustration/Image count mismatch: Source={src_doc.total_images} vs Target={tgt_doc.total_images}")
    if src_doc.total_tables != tgt_doc.total_tables:
        diffs.append(f"Global Table count mismatch: Source={src_doc.total_tables} vs Target={tgt_doc.total_tables}")
    if src_doc.total_alerts != tgt_doc.total_alerts:
        diffs.append(f"Global Alert/Callout count mismatch: Source={src_doc.total_alerts} vs Target={tgt_doc.total_alerts}")

    # 2. Recursive Hierarchical Tree Diff
    tree_diffs = []
    matched_pts, total_pts = diff_section_subtrees(src_doc.root, tgt_doc.root, tree_diffs, "")

    all_diffs = diffs + tree_diffs
    
    # Tree Structural Alignment Score (0.0% to 100.0%)
    if total_pts == 0:
        tree_score = 100.0
    else:
        tree_score = max(0.0, (matched_pts / total_pts) * 100.0)

    # Cap at 99.0% if there are unresolved diffs
    if all_diffs and tree_score > 95.0:
        tree_score = 95.0

    return {
        "score": tree_score,
        "diffs": all_diffs,
        "is_aligned": len(all_diffs) == 0,
        "src_metrics": {
            "h1": src_doc.h_counts[1], "h2": src_doc.h_counts[2], "h3": src_doc.h_counts[3],
            "code": src_doc.total_code_blocks, "mermaid": src_doc.total_mermaid,
            "images": src_doc.total_images, "tables": src_doc.total_tables, "alerts": src_doc.total_alerts
        },
        "tgt_metrics": {
            "h1": tgt_doc.h_counts[1], "h2": tgt_doc.h_counts[2], "h3": tgt_doc.h_counts[3],
            "code": tgt_doc.total_code_blocks, "mermaid": tgt_doc.total_mermaid,
            "images": tgt_doc.total_images, "tables": tgt_doc.total_tables, "alerts": tgt_doc.total_alerts
        }
    }


def collect_directory_files(base_dir: str) -> List[str]:
    """Collects all relative markdown filepaths within a directory recursively."""
    file_list = []
    if not os.path.exists(base_dir):
        return []
    
    subdirs = [d for d in os.listdir(base_dir) if os.path.isdir(os.path.join(base_dir, d))]
    has_ssot_domains = any(d in subdirs for d in SSOT_DOMAINS)
    
    if has_ssot_domains:
        for domain in SSOT_DOMAINS:
            domain_dir = os.path.join(base_dir, domain)
            if os.path.exists(domain_dir):
                for root, _, files in os.walk(domain_dir):
                    for f in files:
                        if f.endswith(".md"):
                            rel = os.path.relpath(os.path.join(root, f), base_dir).replace("\\", "/")
                            file_list.append(rel)
        if os.path.exists(os.path.join(base_dir, "README.md")):
            file_list.append("README.md")
    else:
        for root, _, files in os.walk(base_dir):
            for f in files:
                if f.endswith(".md"):
                    rel = os.path.relpath(os.path.join(root, f), base_dir).replace("\\", "/")
                    file_list.append(rel)
                    
    return sorted(list(set(file_list)))


def generate_markdown_report(report_data: Dict[str, Any]) -> str:
    """Formats verification results into a clean GitHub-Flavored Markdown summary report."""
    md = []
    md.append("# 🌐 Document AST Tree Structural Alignment & Quality Report\n")
    md.append(f"- **Source Directory**: `{report_data['source_dir']}`")
    md.append(f"- **Target Directory**: `{report_data['target_dir']}`")
    md.append(f"- **Total Source Docs**: `{report_data['total_src']}`")
    md.append(f"- **Total Target Docs**: `{report_data['total_tgt']}`")
    md.append(f"- **Directory Tree Symmetry Coverage**: `{report_data['tree_coverage_pct']:.1f}%`")
    md.append(f"- **1:1 Matched Document Pairs**: `{report_data['matched_count']}`\n")

    if report_data["missing_in_target"]:
        md.append(f"### ⚠️ Missing in Target ({len(report_data['missing_in_target'])})\n")
        for f in report_data["missing_in_target"]:
            md.append(f"- [ ] `{f}`")
        md.append("")

    if report_data["orphan_in_target"]:
        md.append(f"### ⚠️ Extra / Orphan Target Files ({len(report_data['orphan_in_target'])})\n")
        for f in report_data["orphan_in_target"]:
            md.append(f"- ⚠️ `{f}`")
        md.append("")

    if "structural_results" in report_data:
        res = report_data["structural_results"]
        md.append("## 📝 Hierarchical AST Structural Skeleton & Ordering Alignment\n")
        md.append(f"- **Analyzed Pairs**: `{res['total_analyzed']}`")
        md.append(f"- **100% Perfect AST Tree Matches**: `{res['fully_aligned_count']}`")
        md.append(f"- **Structural Mismatches**: `{len(res['discrepancies'])}`\n")
        
        if res["discrepancies"]:
            md.append("| Document Path | Tree Score | Discrepancies (Hierarchical Path & Sequence) |")
            md.append("|---|:---:|---|")
            for item in res["discrepancies"]:
                diff_summary = "<br/>".join([f"• {d}" for d in item["diffs"][:6]])
                if len(item["diffs"]) > 6:
                    diff_summary += f"<br/>• ... and {len(item['diffs'])-6} more discrepancies."
                md.append(f"| `{item['rel_path']}` | **{item['score']:.1f}%** | {diff_summary} |")
            md.append("")

    return "\n".join(md)


def resolve_directory(path_str: str) -> str:
    """Smartly resolves a directory path from absolute, cwd, repo-root, or docs-root."""
    if os.path.isabs(path_str):
        return os.path.abspath(path_str)
    if os.path.exists(path_str):
        return os.path.abspath(path_str)
    repo_root = os.path.abspath(os.path.join(DOCS_DIR, ".."))
    repo_candidate = os.path.join(repo_root, path_str)
    if os.path.exists(repo_candidate):
        return os.path.abspath(repo_candidate)
    docs_candidate = os.path.join(DOCS_DIR, path_str)
    if os.path.exists(docs_candidate):
        return os.path.abspath(docs_candidate)
    return os.path.abspath(repo_candidate)


def main():
    parser = argparse.ArgumentParser(description="WinkMicroOS Document AST Tree Structural Alignment Verifier")
    parser.add_argument("--source-dir", type=str, default=DEFAULT_ZH_DIR, help="Source directory (default: docs/zh)")
    parser.add_argument("--target-dir", type=str, default=DEFAULT_EN_DIR, help="Target directory (default: docs/en)")
    parser.add_argument("--check-content", action="store_true", help="Enable deep Hierarchical AST tree structural & sequence check")
    parser.add_argument("--strict", action="store_true", help="CI strict mode: exit non-zero on tree mismatch or missing files")
    parser.add_argument("--strict-content", action="store_true", help="CI strict mode: exit non-zero on structural discrepancies")
    parser.add_argument("--min-score", type=float, default=80.0, help="Minimum structural similarity score threshold (default: 80.0)")
    parser.add_argument("--file", type=str, default=None, help="Inspect a specific document relative path (e.g. 01-system-overall/01-system-overview.md)")
    parser.add_argument("--report-md", type=str, default=None, help="Output markdown summary report to a file")
    parser.add_argument("--json-output", type=str, default=None, help="Output structured results to a JSON file")
    
    args = parser.parse_args()

    src_dir = resolve_directory(args.source_dir)
    tgt_dir = resolve_directory(args.target_dir)

    repo_root = os.path.abspath(os.path.join(DOCS_DIR, ".."))
    try:
        src_rel_display = os.path.relpath(src_dir, repo_root).replace("\\", "/")
    except ValueError:
        src_rel_display = src_dir
    try:
        tgt_rel_display = os.path.relpath(tgt_dir, repo_root).replace("\\", "/")
    except ValueError:
        tgt_rel_display = tgt_dir

    print("=" * 95)
    print(f" 🌐 Hierarchical AST Tree Structural Alignment Suite: [{src_rel_display}] ⟷ [{tgt_rel_display}]")
    print("=" * 95)

    # 1. Collect and Diff Directory Trees
    src_files = set(collect_directory_files(src_dir))
    tgt_files = set(collect_directory_files(tgt_dir))

    if not src_files:
        print(f"❌ Error: No markdown files found in source directory: {src_dir}")
        return 1

    matched_files = sorted(list(src_files.intersection(tgt_files)))
    missing_in_target = sorted(list(src_files - tgt_files))
    orphan_in_target = sorted(list(tgt_files - src_files))

    total_src = len(src_files)
    total_tgt = len(tgt_files)
    matched_count = len(matched_files)
    tree_coverage_pct = (matched_count / total_src * 100.0) if total_src > 0 else 0.0

    print(f"\n[📁 1. DIRECTORY TREE ALIGNMENT (1:1 SYMMETRY)]")
    print(f"  • Total Source Documents   : {total_src} ({src_rel_display})")
    print(f"  • Total Target Documents   : {total_tgt} ({tgt_rel_display})")
    print(f"  • Matched 1:1 Pairs        : {matched_count}")
    print(f"  • Tree Symmetry Coverage   : {tree_coverage_pct:.1f}%\n")

    if missing_in_target:
        print(f"⚠️  Missing in Target ({len(missing_in_target)} files):")
        for f in missing_in_target[:10]:
            print(f"    - [Missing] {f}")
        if len(missing_in_target) > 10:
            print(f"    ... and {len(missing_in_target) - 10} more files.")

    if orphan_in_target:
        print(f"\n⚠️  Extra / Orphan Files in Target ({len(orphan_in_target)} files):")
        for f in orphan_in_target:
            print(f"    - [Extra] {f}")

    if not missing_in_target and not orphan_in_target:
        print("  ✅ Directory Tree is 100% Perfectly 1:1 Symmetrical!")

    report_payload = {
        "source_dir": src_rel_display,
        "target_dir": tgt_rel_display,
        "total_src": total_src,
        "total_tgt": total_tgt,
        "matched_count": matched_count,
        "tree_coverage_pct": tree_coverage_pct,
        "missing_in_target": missing_in_target,
        "orphan_in_target": orphan_in_target
    }

    # 2. Hierarchical AST Structural Skeleton & Sequential Block Check
    content_failures = []
    discrepancy_list = []

    if args.check_content or args.strict_content or args.file:
        print("\n" + "-" * 95)
        print(" [📝 2. DOCUMENT AST STRUCTURAL TREE & SEQUENCE ALIGNMENT CHECK]")
        print(" Checking: Heading Hierarchy (Root->H1->H2->H3->H4), Section Numbering Patterns,")
        print("           Intra-Section Block Sequence ([Alert, Diagram, Table, Code]), and Schemas...")
        print("-" * 95)

        targets = [args.file] if args.file else matched_files
        fully_aligned_count = 0

        for rel_path in targets:
            s_path = os.path.join(src_dir, rel_path)
            t_path = os.path.join(tgt_dir, rel_path)

            if not os.path.exists(s_path) or not os.path.exists(t_path):
                continue

            src_ast = DocumentAST(s_path)
            tgt_ast = DocumentAST(t_path)
            cmp_res = compare_document_ast_trees(src_ast, tgt_ast)

            score = cmp_res["score"]
            is_aligned = cmp_res["is_aligned"]

            if is_aligned:
                fully_aligned_count += 1
                if args.file:
                    print(f"  ✅ [100% PERFECT AST TREE MATCH] {rel_path}")
            else:
                item_diff = {
                    "rel_path": rel_path,
                    "score": score,
                    "diffs": cmp_res["diffs"]
                }
                discrepancy_list.append(item_diff)

                if score < args.min_score:
                    content_failures.append(item_diff)

                if args.file or not args.strict_content:
                    print(f"  ⚠️ [{score:4.1f}% Tree Score] {rel_path}")
                    for d in cmp_res["diffs"][:4]:
                        print(f"      ↳ {d}")
                    if len(cmp_res["diffs"]) > 4:
                        print(f"      ↳ ... and {len(cmp_res['diffs']) - 4} more structural discrepancies.")

        print(f"\n[📊 HIERARCHICAL AST TREE RESULTS]")
        print(f"  • Total Pairs Analyzed     : {len(targets)}")
        print(f"  • 100% Perfect AST Matches : {fully_aligned_count}")
        print(f"  • Structural Notice Alerts : {len(discrepancy_list) - len(content_failures)}")
        print(f"  • Low Score Mismatches     : {len(content_failures)} (Below threshold {args.min_score}%)")

        report_payload["structural_results"] = {
            "total_analyzed": len(targets),
            "fully_aligned_count": fully_aligned_count,
            "discrepancies": discrepancy_list
        }

    print("\n" + "=" * 95)

    # 3. Export Reports if specified
    if args.report_md:
        md_text = generate_markdown_report(report_payload)
        with open(args.report_md, "w", encoding="utf-8") as f:
            f.write(md_text)
        print(f"📄 Markdown summary report saved to: {args.report_md}")

    if args.json_output:
        with open(args.json_output, "w", encoding="utf-8") as f:
            json.dump(report_payload, f, indent=2, ensure_ascii=False)
        print(f"📊 JSON structured data saved to: {args.json_output}")

    # 4. CI Quality Gate Exit Codes
    exit_code = 0
    if args.strict and (missing_in_target or orphan_in_target):
        print("❌ Strict Tree Check FAILED: Directory tree is not 1:1 symmetrical.")
        exit_code = 1

    if args.strict_content and content_failures:
        print(f"❌ Strict Content Structure Check FAILED: {len(content_failures)} documents failed AST tree threshold.")
        exit_code = 1

    return exit_code

if __name__ == "__main__":
    sys.exit(main())
