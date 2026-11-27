#!/usr/bin/env python3
"""Builds the party service's --servers file from the master's signed list (read-only: one GET of the public list, nothing sent).

Run from cron every 10-15 seconds' worth of freshness the service needs (the list itself is cached 5 s):
  export_servers.py --out servers.json [--url https://master.arenaevolved.com/v1/servers?ae=1] [--playlists matchmaking.txt]

The list's body is checked against the master's key (ed25519, the Signature header) and its time (300 s) before anything is
used; a list that fails the check leaves the old file alone. Each playlist line of matchmaking.txt (id|LABEL|description|address ...)
gives the service's candidates: a listed entry whose `address` is the line's address supplies its name, players (people and bots) and
reachability; an address the list does not show (the master shows addresses only with the operator's consent) is kept as a candidate
with an unknown population (players -1, reachable true), so QUICK PLAY still sends a party to it.
"""
import argparse
import base64
import ipaddress
import json
import os
import socket
import sys
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ed25519  # noqa: E402

MASTER_KEY = "7dd4055858d8e37c60cd3f3c4a944deb6f515d063727e93087fa3ef37b316993"
DEFAULT_URL = "https://master.arenaevolved.com/v1/servers?ae=1"
MAX_AGE_S = 300


def read_playlists(text):
    """[(playlist id, label, [addresses])] from matchmaking.txt's text."""
    out = []
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split("|")
        if len(parts) < 4 or not parts[0].strip() or not parts[3].split():
            continue
        out.append((parts[0].strip(), parts[1].strip(), parts[3].split()))
    return out


def check_list(body, signature, key_hex, now):
    """The parsed list when its signature (the header's `ed25519=<base64>`), protocol and time hold; else raises ValueError."""
    if not signature.startswith("ed25519="):
        raise ValueError("no ed25519 signature")
    try:
        sig = base64.b64decode(signature[len("ed25519="):])
    except Exception:
        raise ValueError("signature is not base64")
    if not ed25519.verify(bytes.fromhex(key_hex), body, sig):
        raise ValueError("signature does not match the master's key")
    data = json.loads(body)
    if data.get("protocol") != 1:
        raise ValueError("protocol is not 1")
    age = now - int(data.get("master", {}).get("time", 0))
    if age > MAX_AGE_S or age < -MAX_AGE_S:
        raise ValueError("list is %d s from now" % age)
    return data


def build(playlists, data, resolve=None):
    """The service's numeric IPv4 candidates, with the list's numbers when it shows the address."""
    resolve = resolve or socket.gethostbyname
    listed = {}
    for entry in data.get("servers", []):
        if entry.get("address"):
            listed[entry["address"]] = entry
    out = []
    resolved = {}
    for pid, label, addresses in playlists:
        for address in addresses:
            host, separator, port = address.rpartition(":")
            if not separator or not port.isdigit() or not 1 <= int(port) <= 65535:
                print("export_servers: skipped %s (not a host and port)" % address, file=sys.stderr)
                continue
            try:
                numeric = str(ipaddress.IPv4Address(host))
            except ValueError:
                if host not in resolved:
                    try:
                        resolved[host] = str(ipaddress.IPv4Address(resolve(host)))
                    except (OSError, ValueError):
                        resolved[host] = None
                numeric = resolved[host]
            if numeric is None:
                print("export_servers: skipped %s (no IPv4 address)" % host, file=sys.stderr)
                continue
            published = "%s:%s" % (numeric, port)
            e = listed.get(published) or listed.get(address)
            if e:
                bots = (e.get("ae") or {}).get("bots") or 0
                out.append({"address": published, "playlist": pid, "name": e.get("name") or label,
                            "players": int(e.get("players", 0)) + int(bots), "max_players": int(e.get("max_players", 16)),
                            "reachable": bool(e.get("reachable", True)) and bool(e.get("open", True)) and not e.get("password")})
            else:
                out.append({"address": published, "playlist": pid, "name": label, "players": -1, "max_players": 16,
                            "reachable": True})
    return out


def write_atomic(path, servers):
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(servers, f, indent=1)
    os.replace(tmp, path)


def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": "ae-party-export/1"})
    with urllib.request.urlopen(req, timeout=15) as r:
        return r.read(), r.headers.get("Signature", "")


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("--url", default=DEFAULT_URL)
    p.add_argument("--playlists", default=os.path.join(HERE, "..", "..", "port", "assets", "network", "matchmaking.txt"))
    p.add_argument("--out", required=True)
    p.add_argument("--key", default=MASTER_KEY, help="the master's public key, 64 hex digits")
    p.add_argument("--from-file", help="a saved body (and --signature) instead of a fetch, for tests")
    p.add_argument("--signature", default="")
    a = p.parse_args(argv)
    with open(a.playlists) as f:
        playlists = read_playlists(f.read())
    try:
        if a.from_file:
            with open(a.from_file, "rb") as f:
                body, signature = f.read(), a.signature
        else:
            body, signature = fetch(a.url)
        data = check_list(body, signature, a.key, int(time.time()))
    except (OSError, ValueError) as e:
        print("export_servers: kept the old file (%s)" % e, file=sys.stderr)
        return 1
    servers = build(playlists, data)
    write_atomic(a.out, servers)
    print("export_servers: %d candidates, %d listed" % (len(servers), sum(1 for s in servers if s["players"] >= 0)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
