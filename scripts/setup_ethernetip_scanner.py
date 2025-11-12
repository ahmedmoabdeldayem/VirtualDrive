#!/usr/bin/env python3
"""
VirtualDrive EtherNet/IP Scanner (PC side)

Connects to the RPi5 running drive-simulator --protocol ethernetip,
opens a Class 1 (cyclic I/O) connection to the Output/Input assemblies,
sends ControlWord=0x000F + TargetSpeed=1500.0 RPM, and prints
the received StatusWord and ActualSpeed in a loop.

Requirements:
    pip install cpppo

Usage:
    python3 setup_ethernetip_scanner.py <RPi5-IP> [--cycles N]
    python3 setup_ethernetip_scanner.py 192.168.1.100
"""
import argparse
import struct
import sys
import time

try:
    import cpppo
    from cpppo.server.enip import client
    from cpppo.server.enip.client import connector
except ImportError:
    print("ERROR: cpppo not installed.  Run: pip install cpppo")
    sys.exit(1)


def enumerate_identity(host: str, port: int = 44818) -> None:
    """Query the EtherNet/IP identity object and print device information."""
    print(f"[Scanner] Connecting to {host}:{port} ...")
    try:
        with connector(host=host, port=port, timeout=5.0) as conn:
            ids = list(conn.read(
                [client.parse_operations(["@1/1/1"])],  # Class 1, Instance 1, Attr 1
                timeout=5.0,
            ))
            for _op, _plc, val in ids:
                print(f"[Scanner] Identity response: {val!r}")
    except Exception as exc:  # noqa: BLE001
        print(f"[Scanner] Identity query failed: {exc}")


def run_cyclic(host: str, cycles: int, period_ms: float) -> None:
    """
    Open a Class 3 (explicit) connection and periodically write the Output
    assembly and read the Input assembly.

    Note: cpppo does not natively support Class 1 (implicit/cyclic) connections
    without a UCMM-capable device.  We simulate a cyclic exchange via Class 3
    explicit messaging at the specified period — sufficient for laboratory
    testing of the VirtualDrive adapter.
    """
    output_instance = 0x64   # Output Assembly (scanner -> adapter)
    input_instance  = 0x65   # Input Assembly  (adapter -> scanner)

    # Output assembly: ControlWord(UINT16) + TargetSpeed(REAL) = 6 bytes
    control_word  = 0x000F   # Enable Operation
    target_speed  = 1500.0   # RPM

    print(f"[Scanner] Starting cyclic exchange with {host} ({cycles} cycles)")
    print(f"          ControlWord=0x{control_word:04X}  TargetSpeed={target_speed} RPM")
    print()

    with connector(host=host, port=44818, timeout=10.0) as conn:
        for cycle in range(cycles):
            t0 = time.monotonic()

            # --- Write Output Assembly ---
            out_data = struct.pack("<Hf", control_word, target_speed)
            # EtherNet/IP Set_Attribute_Single on Assembly Object, instance 0x64, attribute 3
            write_ops = [
                client.parse_operations(
                    [f"@4/{output_instance}/3={out_data.hex()}"]
                )
            ]
            try:
                for _op, _plc, _val in conn.write(write_ops, timeout=1.0):
                    pass
            except Exception as exc:  # noqa: BLE001
                print(f"[Scanner] Write failed at cycle {cycle}: {exc}")
                continue

            # --- Read Input Assembly ---
            read_ops = [
                client.parse_operations([f"@4/{input_instance}/3"])
            ]
            status_word  = 0
            actual_speed = 0.0
            position     = 0
            try:
                for _op, _plc, val in conn.read(read_ops, timeout=1.0):
                    raw = bytes(val) if val else b""
                    if len(raw) >= 10:
                        status_word,  = struct.unpack_from("<H", raw, 0)
                        actual_speed, = struct.unpack_from("<f", raw, 2)
                        position,     = struct.unpack_from("<i", raw, 6)
            except Exception as exc:  # noqa: BLE001
                print(f"[Scanner] Read failed at cycle {cycle}: {exc}")
                continue

            elapsed_ms = (time.monotonic() - t0) * 1000.0
            print(
                f"[{cycle:5d}]  SW=0x{status_word:04X}  "
                f"speed={actual_speed:8.1f} RPM  "
                f"pos={position:10d}  rtt={elapsed_ms:.1f} ms"
            )

            sleep_s = max(0.0, period_ms / 1000.0 - (time.monotonic() - t0))
            time.sleep(sleep_s)


def main() -> None:
    parser = argparse.ArgumentParser(description="VirtualDrive EtherNet/IP Scanner")
    parser.add_argument("host",    help="RPi5 IP address")
    parser.add_argument("--cycles",  type=int,   default=100,   help="Number of cycles (default 100)")
    parser.add_argument("--period",  type=float, default=100.0, help="Cycle period in ms (default 100)")
    args = parser.parse_args()

    enumerate_identity(args.host)
    print()
    run_cyclic(args.host, args.cycles, args.period)


if __name__ == "__main__":
    main()
