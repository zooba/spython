#!/usr/bin/env python3.8
"""Add spython extended attributes to Python files
"""
import argparse
import compileall
import hashlib
import os

BASE = os.path.abspath(os.path.dirname(os.__file__))
XATTR_NAME = "user.org.python.x-spython-hash"

parser = argparse.ArgumentParser("mkxattr for spython")
parser.add_argument("--basedir", default=BASE)
parser.add_argument("--xattr-name", default=XATTR_NAME)
parser.add_argument("--hash", default="sha256")
parser.add_argument("--verbose", action="store_true")


def main():
    args = parser.parse_args()
    setxattr(args)


def setxattr(args):
    xattr_name = args.xattr_name.encode("ascii")
    for root, dirs, files in os.walk(args.basedir, topdown=True):
        for filename in sorted(files):
            if not filename.endswith((".py", ".pyc")):
                continue
            filename = os.path.join(root, filename)
            hasher = hashlib.new(args.hash)
            with open(filename, "rb") as f:
                hasher.update(f.read())
                hexdigest = hasher.hexdigest().encode("ascii")
                try:
                    value = os.getxattr(f.fileno(), xattr_name)
                except OSError:
                    value = None
                if value != hexdigest:
                    if args.verbose:
                        if value is None:
                            print(f"Adding spython hash to '{filename}'")
                        else:
                            print(f"Updating spython hash of '{filename}'")
                    # it's likely that the pyc file is also out of sync
                    compileall.compile_file(filename, quiet=2)
                    os.setxattr(filename, xattr_name, hexdigest)


if __name__ == "__main__":
    main()
