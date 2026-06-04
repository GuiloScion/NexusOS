#!/usr/bin/env python3
"""
NexusOS download tracker.

Polls the GitHub Releases API for this repository and appends today's
download counts for every release asset to a CSV history file. Designed
to be run daily via .github/workflows/track-downloads.yml (or manually).

CSV schema:
    timestamp,release_tag,asset_name,download_count,created_at,size_bytes
"""
import csv
import datetime
import json
import os
import sys
import urllib.error
import urllib.request
from pathlib import Path

REPO = os.environ.get("GITHUB_REPO", "")
HISTORY_FILE = Path(os.environ.get("DOWNLOAD_HISTORY", "data/download_history.csv"))
USER_AGENT = "NexusOS-DownloadTracker/1.0"


def fetch_releases(repo: str) -> list:
    """Pull every release object for the given repo."""
    url = f"https://api.github.com/repos/{repo}/releases?per_page=100"
    req = urllib.request.Request(
        url,
        headers={
            "User-Agent": USER_AGENT,
            "Accept": "application/vnd.github+json",
        },
    )
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        req.add_header("Authorization", f"Bearer {token}")

    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return json.loads(resp.read())
    except urllib.error.HTTPError as e:
        body = e.read().decode("utf-8", "ignore")
        print(f"HTTP {e.code} from GitHub: {body}", file=sys.stderr)
        sys.exit(1)
    except urllib.error.URLError as e:
        print(f"Network error: {e}", file=sys.stderr)
        sys.exit(1)


def collect_rows(releases: list, timestamp: str) -> list:
    rows = []
    for release in releases:
        tag = release.get("tag_name", "(no-tag)")
        for asset in release.get("assets", []):
            rows.append(
                {
                    "timestamp": timestamp,
                    "release_tag": tag,
                    "asset_name": asset["name"],
                    "download_count": asset["download_count"],
                    "created_at": asset["created_at"],
                    "size_bytes": asset["size"],
                }
            )
    return rows


def append_to_csv(rows: list, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    is_new = not path.exists()
    fieldnames = [
        "timestamp",
        "release_tag",
        "asset_name",
        "download_count",
        "created_at",
        "size_bytes",
    ]
    with path.open("a", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        if is_new:
            writer.writeheader()
        for row in rows:
            writer.writerow(row)


def main() -> int:
    if not REPO:
        print(
            "Set GITHUB_REPO=<owner>/<repo>. In GitHub Actions the workflow "
            "passes ${{ github.repository }} automatically.",
            file=sys.stderr,
        )
        return 1

    timestamp = datetime.datetime.now(datetime.timezone.utc).strftime(
        "%Y-%m-%dT%H:%M:%SZ"
    )
    releases = fetch_releases(REPO)
    rows = collect_rows(releases, timestamp)

    if not rows:
        print(f"No release assets found for {REPO}. Nothing to log.")
        return 0

    append_to_csv(rows, HISTORY_FILE)

    total = sum(r["download_count"] for r in rows)
    tags = len({r["release_tag"] for r in rows})
    print(
        f"[{timestamp}] {len(rows)} assets across {tags} releases, "
        f"cumulative downloads: {total}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
