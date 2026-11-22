"""Save JPEG snapshots and per-session metadata; needs Python 3, no pip packages."""
import argparse
import datetime as dt
import json
from pathlib import Path
import re
import time
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="192.168.4.1")
    parser.add_argument("--session", required=True, help="Unique capture session, e.g. pond_morning_01")
    parser.add_argument("--count", type=int, default=100)
    parser.add_argument("--interval", type=float, default=1.0)
    parser.add_argument("--notes", default="")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,80}", args.session):
        parser.error("Session must contain only letters, digits, underscore or hyphen")
    if not 1 <= args.count <= 10000 or args.interval < 0.2:
        parser.error("Use count 1..10000 and interval >=0.2 seconds")
    root = Path(__file__).resolve().parents[1] / "data" / "captures" / args.session
    root.mkdir(parents=True, exist_ok=False)
    (root / "session.json").write_text(json.dumps({
        "session": args.session, "notes": args.notes,
        "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "source": args.host, "labels": "not_annotated"
    }, ensure_ascii=False, indent=2), encoding="utf-8")
    failures = 0
    for index in range(args.count):
        start = time.monotonic()
        try:
            with urllib.request.urlopen(f"http://{args.host}:82/capture?t={time.time_ns()}", timeout=3) as response:
                data = response.read(128 * 1024 + 1)
            if len(data) > 128 * 1024 or not data.startswith(b"\xff\xd8") or not data.endswith(b"\xff\xd9"):
                raise ValueError("Incomplete or invalid JPEG")
            target = root / f"{index:05d}.jpg"
            target.write_bytes(data)
            with (root / "frames.jsonl").open("a", encoding="utf-8") as log:
                log.write(json.dumps({"file": target.name, "received_utc": dt.datetime.now(dt.timezone.utc).isoformat()}) + "\n")
            print(target.name, len(data), "bytes")
        except (OSError, ValueError) as error:
            failures += 1
            print(f"Frame {index}: {error}")
            if failures >= 5:
                raise SystemExit("Stopped after 5 failed captures; check Wi-Fi/camera")
        time.sleep(max(0, args.interval - (time.monotonic() - start)))
    print(f"Saved {args.count - failures}/{args.count} frames to {root}")


if __name__ == "__main__":
    main()
