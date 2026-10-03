#!/usr/bin/env python3
"""Wrap an EmuTOS image in the RT68ICE monitor transfer format."""

import argparse
import struct
import zlib


IMAGE_MAGIC = b"RT68"


def parse_address(value):
    try:
        address = int(value, 16)
    except ValueError as error:
        raise argparse.ArgumentTypeError(f"invalid hexadecimal address: {value}") from error
    if not 0 <= address <= 0xFFFFFFFF:
        raise argparse.ArgumentTypeError("address must fit in 32 bits")
    return address


def main():
    parser = argparse.ArgumentParser(
        description="Create a CRC-32-protected RT68ICE monitor image"
    )
    parser.add_argument(
        "--address", required=True, type=parse_address,
        help="payload load address (hexadecimal)"
    )
    parser.add_argument("raw_file", help="raw EmuTOS payload")
    parser.add_argument("image_file", help="headered RT68ICE image")
    args = parser.parse_args()

    with open(args.raw_file, "rb") as file:
        payload = file.read()

    header = struct.pack(
        ">4sIII",
        IMAGE_MAGIC,
        args.address,
        len(payload),
        zlib.crc32(payload) & 0xFFFFFFFF,
    )
    with open(args.image_file, "wb") as file:
        file.write(header)
        file.write(payload)


if __name__ == "__main__":
    main()
