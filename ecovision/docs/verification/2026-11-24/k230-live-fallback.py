"""Resume the existing K230 UART application while the cold boot is unverified."""

import sys
from pathlib import Path

evidence = Path(__file__).resolve().parent
sys.path.insert(0, str(evidence.parent / "2026-11-22"))

import canmv_backend_client

canmv_backend_client.EVIDENCE = evidence

import k230_link_service

k230_link_service.NAME = "k230-live-fallback"
k230_link_service.STOP = evidence / "k230-live-fallback.stop"
k230_link_service.STATUS = evidence / "k230-live-fallback-status.json"

if __name__ == "__main__":
    k230_link_service.main()
