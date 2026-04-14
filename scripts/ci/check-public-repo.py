"""Fail in GitHub context if repository visibility is private."""
from __future__ import annotations

import json
import os
import urllib.request


def main() -> int:
    repo = os.environ.get("GITHUB_REPOSITORY", "").strip()
    token = os.environ.get("GITHUB_TOKEN", "").strip()

    if not repo or not token:
        print("Skipping public repo check (missing GITHUB_REPOSITORY or GITHUB_TOKEN).")
        return 0

    url = f"https://api.github.com/repos/{repo}"
    req = urllib.request.Request(
        url,
        headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {token}",
            "User-Agent": "uboot-ci",
        },
    )

    with urllib.request.urlopen(req, timeout=30) as response:
        payload = json.loads(response.read().decode("utf-8"))

    if payload.get("private") is True:
        raise RuntimeError(f"Repository {repo} is private; public visibility is required for OSS signing eligibility.")

    print(f"Public repository check passed: {repo} is public.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
