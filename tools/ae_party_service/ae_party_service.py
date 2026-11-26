#!/usr/bin/env python3
"""AE's party and QUICK PLAY service: friend-code parties, the leader's follow and an instant best-server pick, over HTTP JSON.

One process, standard library only. Players are known by their friend code (the first 60 bits of SHA-256 of the party key's
public half, Crockford base32; the party key is the game's own, not the Discord-link player key); a session starts with a `challenge` (a single-use nonce) and a `hello` signed over this
service's own address and that nonce, and every later call carries its `sid`. The service keeps
sessions, parties, invites, searches and held seats in memory (a small state file lets parties survive a restart); it shows no IP address and keeps none (only a keyed code of one, new each run, for the rate limits), no chat line and no friend list. Seat tickets are signed with the service's own Ed25519 key.

Wire 2: every call is a POST of a JSON body (strings only); no query string carries a secret, a key, a signature or a session. GET is
only /v1/health. Run: ae_party_service.py --port 8780
--key-file service.key --servers servers.json --state party-state.json. Seats held on a server for a party last 60 s; every call is rate limited per session; a number or seat count that does not parse is refused with 400.
"""
import argparse
import base64
import collections
import hashlib
import hmac
import ipaddress
import json
import os
import re
import secrets
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ed25519  # noqa: E402

WIRE = 2
PARTY_MAX = 8
SEATS_MAX = 4
SESSION_OFFLINE_S = 30
MEMBER_REMOVE_S = 120
SESSION_DROP_S = 600
INVITE_S = 300
HOLD_S = 60
FAILED_SERVER_S = 300
FAILED_REPORT_S = 120      # a failed server counts only when reported this soon after the party was sent there
FOLLOW_S = 600             # a follow stays in the party's view this long
INVITES_SHOWN = 4
STRANGER_INVITES_MAX = 3   # pending invites a player holds from codes that are not mutual friends
INVITES_MAX = 10           # pending invites in all
BLOCKS_MAX = 200
FAILED_MAX = 8             # failed servers kept per party
NONCE_S = 60
POLL_MS = 1000             # the poll interval a client uses with a party or a search; it may be raised under load
SESSIONS_MAX = 4096
SESSIONS_PER_CLIENT = 16
HELLO_GLOBAL_PER_S = 20
# per client key: (events, seconds)
CLIENT_LIMITS = {"challenge": ((30, 60),), "hello": ((12, 60), (120, 3600)), "quick": ((20, 60),), "invite": ((60, 60),)}
NONCE_MAX = 50000
NONCES_PER_SOURCE = 10
BODY_MAX = 8192
ALPHABET = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"
LINK_ALPHABET = ALPHABET

# the playlists the matchmaking screen offers (port/assets/network/matchmaking.txt ids) and each one's party limit in seats
PLAYLISTS = {
    "casual": {"label": "CASUAL", "party_max": 4, "ask_bots": False},
    "btb": {"label": "BTB", "party_max": 8, "ask_bots": False},
    "comp": {"label": "COMP", "party_max": 4, "ask_bots": True},
    "classic": {"label": "CLASSIC COMP", "party_max": 2, "ask_bots": True},
    "community": {"label": "MODDED & COMMUNITY", "party_max": 8, "ask_bots": False},
}


class Refusal(Exception):
    """A refused call: a stable code, a status and a plain detail."""

    def __init__(self, code, detail="", status=400, **extra):
        super().__init__(code)
        self.code, self.detail, self.status, self.extra = code, detail, status, extra


# ---------------------------------------------------------------- friend code

def friend_code(pub):
    """XXXX-XXXX-XXXX from a 32-byte public key: the first 60 bits of its SHA-256, Crockford base32."""
    n = int.from_bytes(hashlib.sha256(pub).digest()[:8], "big") >> 4
    text = "".join(ALPHABET[(n >> (55 - 5 * i)) & 31] for i in range(12))
    return "-".join(text[i:i + 4] for i in (0, 4, 8))


def parse_code(text):
    """A typed code in its written form, or None (I and L read as 1, O as 0, any case, dashes and spaces ignored)."""
    digits = []
    for c in text or "":
        if c in "- \t\r\n":
            continue
        c = c.upper()
        c = "1" if c in "IL" else "0" if c == "O" else c
        if c not in ALPHABET or len(digits) == 12:
            return None
        digits.append(c)
    if len(digits) != 12:
        return None
    s = "".join(digits)
    return "-".join(s[i:i + 4] for i in (0, 4, 8))


def clean_name(name):
    name = "".join(c for c in (name or "") if c.isprintable())[:24].strip()
    return name or "Player"


def random_text(n):
    return "".join(secrets.choice(LINK_ALPHABET) for _ in range(n))


# ---------------------------------------------------------------- rate limits

class Window:
    """At most `limit` events in a sliding `seconds`."""

    def __init__(self, limit, seconds):
        self.limit, self.seconds, self.stamps = limit, seconds, []

    def hit(self, now):
        self.stamps = [t for t in self.stamps if now - t < self.seconds]
        if len(self.stamps) >= self.limit:
            return False
        self.stamps.append(now)
        return True

    def full(self, now):
        self.stamps = [t for t in self.stamps if now - t < self.seconds]
        return len(self.stamps) >= self.limit

    def idle(self, now):
        """No event within the span: the window holds nothing worth keeping."""
        return not self.stamps or now - self.stamps[-1] >= self.seconds


# ---------------------------------------------------------------- QUICK PLAY pick (pure)

def pick_server(servers, playlist, seats, pings, held, failed=(), prefer=None):
    """The best AE server for a party of `seats` in `playlist` now.

    servers: [{address, playlist, name, players, max_players, reachable}]; pings: {address: ms}; held: {address: seats held for
    parties on their way}; failed: addresses to skip; prefer: the server the party just played. Returns
    {"server": entry, "ping_ms": ms or None, "room": free seats} or {"server": None, "partial": {...} | None}."""
    best, partial = None, None
    anyone = any(s.get("players", 0) > 0 for s in servers if s.get("playlist") == playlist and s.get("reachable", True))
    have_ping = any(s["address"] in pings for s in servers if s.get("playlist") == playlist)
    for s in servers:
        if s.get("playlist") != playlist or not s.get("reachable", True) or s["address"] in failed:
            continue
        taken = max(0, s.get("players", 0)) + held.get(s["address"], 0)
        room = max(0, s.get("max_players", 16) - taken)
        ping = pings.get(s["address"])
        entry = {"server": s, "ping_ms": ping, "room": room}
        if room < seats:
            if partial is None or room > partial["room"]:
                partial = entry
            continue
        score = (ping if ping is not None else 150)
        if have_ping and ping is None:
            score += 400
        if anyone and s.get("players", 0) == 0:
            score += 25
        if held.get(s["address"], 0):
            score += 15
        if prefer and s["address"] == prefer:
            score -= 40
        key = (score, -s.get("players", 0))
        if best is None or key < best[0]:
            best = (key, entry)
    if best:
        return best[1]
    return {"server": None, "partial": partial}


# ---------------------------------------------------------------- seat tickets

def make_ticket(seed, party_id, address, member_codes, seats, expires):
    """A signed seat ticket: magic AEPT, version 1, party hash, server hash, expiry, seats, member code hashes (8 bytes each)."""
    body = bytearray(b"AEPT\x01")
    body += hashlib.sha256(party_id.encode()).digest()[:8]
    body += hashlib.sha256(address.encode()).digest()[:8]
    body += int(expires).to_bytes(8, "big")
    body.append(seats)
    body.append(len(member_codes))
    for code in member_codes:
        body += hashlib.sha256(code.encode()).digest()[:8]
    return base64.urlsafe_b64encode(bytes(body) + ed25519.sign(seed, bytes(body))).decode().rstrip("=")


def check_ticket(pub, ticket, address, code, now):
    """Whether a ticket is the service's, for this server and member, not expired. Returns the seats or 0."""
    try:
        raw = base64.urlsafe_b64decode(ticket + "=" * (-len(ticket) % 4))
    except Exception:
        return 0
    if len(raw) < 5 + 8 + 8 + 8 + 2 + 64 or raw[:5] != b"AEPT\x01":
        return 0
    body, sig = raw[:-64], raw[-64:]
    if not ed25519.verify(pub, body, sig):
        return 0
    if body[13:21] != hashlib.sha256(address.encode()).digest()[:8] or int.from_bytes(body[21:29], "big") < now:
        return 0
    count = body[30]
    if len(body) != 31 + 8 * count:
        return 0
    mine = hashlib.sha256(code.encode()).digest()[:8]
    if not any(body[31 + 8 * i:39 + 8 * i] == mine for i in range(count)):
        return 0
    return body[29]


# ---------------------------------------------------------------- the service

class Session:
    def __init__(self, sid, key, code, name, build, net, seats, now, client="?", client_coarse=None):
        self.sid, self.key, self.code, self.name = sid, key, code, name
        self.client = client         # the keyed code of the connection's address, for the limits (never the address)
        self.client_coarse = client if client_coarse is None else client_coarse
        self.build, self.net, self.seats = build, net, seats
        self.last, self.offline_since = now, None
        self.rev = 1
        self.party = None            # party id
        self.invites = {}            # party id -> {"from": code, "name": name, "expires": t}
        self.notices = []            # [{"text":..., "n": counter}]
        self.watch = set()
        self.blocks = set()          # codes this player blocked (the game keeps the list; it is sent with presence)
        self.limits = {"any": Window(15, 5), "invite_min": Window(20, 60), "invite_day": Window(100, 86400),
                       "link": Window(10, 60), "quick": Window(10, 60)}
        self.link_failures, self.link_locked_until = [], 0
        self.steam_hash = None       # 12 hex digits of SHA-256("ae-steam-1" + steam id), set by steam_register (opt-in)
        self.steam_watch = set()     # the friends' hashes this player asked about
        self.limits["steam"] = Window(30, 60)


class Party:
    def __init__(self, pid, leader):
        self.id, self.secret = pid, random_text(12)
        self.members = [leader]      # codes in join order
        self.leader = leader
        self.privacy = "invite"
        self.search = None           # the object members get as `search`
        self.follow = None
        self.follow_to = set()       # the codes that were members when the follow was made: only they see it
        self.follow_at = 0
        self.search_n = 0
        self.holds = []              # [(address, seats, expires)]
        self.follow_limits = (Window(1, 10), Window(60, 3600))   # one follow per 10 s, 60 an hour
        self.last_found = None       # (address, at): the server this party was last sent to
        self.failed = {}             # address -> until: servers this party reported it could not join

    @property
    def link(self):
        return self.id + self.secret


class Service:
    def __init__(self, key_seed=None, servers=None, now=time.time, origin="http://127.0.0.1:8780", trusted_proxies=("127.0.0.1", "::1"), steam_dev=False):
        self.seed = key_seed or secrets.token_bytes(32)
        self.public = ed25519.public_key(self.seed)
        self.now = now
        self.trusted = {ipaddress.ip_address(a) for a in trusted_proxies}
        self.ip_key = secrets.token_bytes(32)    # this run's key for the address codes: no address is kept, nothing is linkable across runs
        self.steam_dev = steam_dev    # the steam_* calls (reversible short hashes of Steam ids): a DEV test's only, off on a public service
        self.origin = origin          # this service's own address as players write it: a hello is signed for it
        self.nonces = collections.OrderedDict()   # challenge nonce -> (expiry, source keys), oldest first
        self.nonces_by_source = {}    # source key -> ordered set of its live nonces
        self.servers = servers or []
        self.sessions, self.by_code, self.parties = {}, {}, {}
        self.holds_by_address = {}    # address -> party id -> (seats, expiry, source keys)
        self.holding_by_client = {}   # client key -> party ids with live holds
        self.restored = {}           # code -> party id, from the state file
        self.client_limits = {}      # (kind, client key) -> Window
        self.hello_global = Window(HELLO_GLOBAL_PER_S, 1)
        self.stats = {"hello": 0, "quick_play": 0, "found": 0, "bots": 0, "refused": 0}
        self.lock = threading.RLock()
        self.log = lambda text: None

    # ---- the client key
    def client_keys(self, peer, forwarded=None):
        """Keyed codes for the source's IPv4 /32 or IPv6 /64 and its coarser IPv6 /48."""
        try:
            address = ipaddress.ip_address(peer)
            if address.version == 6 and address.ipv4_mapped:
                address = address.ipv4_mapped
            if address in self.trusted and forwarded:
                last = forwarded.split(",")[-1].strip()
                try:
                    address = ipaddress.ip_address(last)
                except ValueError:
                    pass
            if address.version == 6 and address.ipv4_mapped:
                address = address.ipv4_mapped
            bits = (64, 48) if address.version == 6 else (32, 32)
            texts = [str(ipaddress.ip_network((address, prefix), strict=False)) for prefix in bits]
        except ValueError:
            texts = ["?", "?"]
        return tuple(hmac.new(self.ip_key, text.encode(), hashlib.sha256).digest()[:8].hex() for text in texts)

    def client_key(self, peer, forwarded=None):
        """The fine source key kept by sessions and the existing per-address limits."""
        return self.client_keys(peer, forwarded)[0]

    @staticmethod
    def _source_keys(peer):
        keys = peer if isinstance(peer, tuple) else (peer,)
        return tuple(dict.fromkeys(keys))

    def _client_hit(self, kind, client, now):
        """Counts an event against every limit of kind for the client; False when one is full."""
        windows = [self.client_limits.setdefault((kind, i, client), Window(*spec)) for i, spec in enumerate(CLIENT_LIMITS[kind])]
        if any(w.full(now) for w in windows):
            return False
        for w in windows:
            w.hit(now)
        return True

    def _source_hit(self, kind, peer, now):
        """Counts atomically against the fine address and, for IPv6, its /48."""
        windows = []
        for client in self._source_keys(peer):
            windows += [self.client_limits.setdefault((kind, i, client), Window(*spec))
                        for i, spec in enumerate(CLIENT_LIMITS[kind])]
        if any(w.full(now) for w in windows):
            return False
        for window in windows:
            window.hit(now)
        return True

    # ---- helpers
    def _touch(self, session):
        session.rev += 1

    def _touch_party(self, party):
        for code in party.members:
            s = self.by_code.get(code)
            if s:
                self._touch(s)

    def _session(self, sid):
        s = self.sessions.get(sid)
        if not s:
            raise Refusal("no_session", "say hello again", 401)
        now = self.now()
        s.last, s.offline_since = now, None
        if not s.limits["any"].hit(now):
            raise Refusal("rate_limited", "slow down", 429)
        return s

    def _party_of(self, s):
        return self.parties.get(s.party) if s.party else None

    def _notice(self, code, text):
        s = self.by_code.get(code)
        if s:
            s.notices.append({"text": text, "n": s.rev})
            del s.notices[:-5]
            self._touch(s)

    def _name(self, code):
        s = self.by_code.get(code)
        return s.name if s else "Player"

    def _seats(self, party):
        return sum(self.by_code[c].seats for c in party.members if c in self.by_code)

    def _live_seats(self, party, now):
        return sum(self.by_code[c].seats for c in party.members
                   if c in self.by_code and now - self.by_code[c].last <= SESSION_OFFLINE_S)

    def _clear_hold(self, party):
        for address, seats, expiry in party.holds:
            if address not in self.holds_by_address:
                continue
            hold = self.holds_by_address[address].pop(party.id, None)
            if hold:
                for client in hold[2]:
                    parties = self.holding_by_client.get(client)
                    if parties:
                        parties.discard(party.id)
                        if not parties:
                            del self.holding_by_client[client]
            if not self.holds_by_address[address]:
                del self.holds_by_address[address]
        party.holds = []

    @staticmethod
    def _server_hold_cap(server):
        open_seats = max(0, server.get("max_players", 16) - max(0, server.get("players", 0)))
        return max(4, open_seats // 2)

    def _add_hold(self, party, server, seats, clients, now):
        clients = tuple(dict.fromkeys(clients))
        for pid in set().union(*(self.holding_by_client.get(client, set()) for client in clients)):
            held_party = self.parties.get(pid)
            if held_party and any(expiry <= now for address, count, expiry in held_party.holds):
                self._clear_hold(held_party)
        held = self._hold_seats(server["address"], server)
        server_cap = self._server_hold_cap(server)
        count = min(seats, max(0, server_cap - held))
        if count <= 0 or any(party.id not in self.holding_by_client.get(client, ()) and
                             len(self.holding_by_client.get(client, ())) >= 2 for client in clients):
            return 0
        expiry = now + HOLD_S
        self.holds_by_address.setdefault(server["address"], {})[party.id] = (count, expiry, clients)
        for client in clients:
            self.holding_by_client.setdefault(client, set()).add(party.id)
        party.holds = [(server["address"], count, expiry)]
        return count

    def _hold_seats(self, address, server=None):
        now = self.now()
        total = 0
        for pid, (seats, expiry, clients) in list(self.holds_by_address.get(address, {}).items()):
            party = self.parties.get(pid)
            if not party or expiry <= now:
                if party:
                    self._clear_hold(party)
                else:
                    del self.holds_by_address[address][pid]
                    for client in clients:
                        parties = self.holding_by_client.get(client)
                        if parties:
                            parties.discard(pid)
                            if not parties:
                                del self.holding_by_client[client]
                continue
            total += min(seats, self._live_seats(party, now))
        if address in self.holds_by_address and not self.holds_by_address[address]:
            del self.holds_by_address[address]
        if server is None:
            server = next((item for item in self.servers if item.get("address") == address), None)
        return min(total, self._server_hold_cap(server)) if server else total

    def _drop_from_party(self, code, reason=None):
        """A member leaves (or is removed): the lead passes to the one who joined first; the last one out ends the party."""
        s = self.by_code.get(code)
        party = self.parties.get(s.party) if s and s.party else None
        if not party or code not in party.members:
            return
        party.members.remove(code)
        s.party = None
        self._clear_follow(party)
        self._touch(s)
        if not party.members:
            self._clear_hold(party)
            del self.parties[party.id]
            return
        if party.leader == code:
            party.leader = party.members[0]
            self._notice(party.leader, "You lead now")
            for c in party.members:
                if c != party.leader:
                    self._notice(c, "%s leads now" % self._name(party.leader))
            self._cancel_search(party, "leader_left")
        elif reason:
            self._notice(party.leader, "%s left the party" % self._name(code))
        self._touch_party(party)

    def _clear_follow(self, party):
        """A follow names a game for the members of that moment: any change of members or leader ends it."""
        if party.follow:
            party.follow, party.follow_to = None, set()

    def _cancel_search(self, party, reason="cancelled"):
        if party.search and party.search.get("state") in ("found", "bots"):
            party.search = {"state": "none", "reason": reason, "n": self._next_n(party)}
        self._clear_hold(party)
        self._touch_party(party)

    def _next_n(self, party):
        party.search_n += 1
        return party.search_n

    # ---- housekeeping
    def tick(self):
        with self.lock:
            now = self.now()
            for s in list(self.sessions.values()):
                for pid in [p for p, i in s.invites.items() if i["expires"] <= now]:
                    del s.invites[pid]
                    self._touch(s)
                age = now - s.last
                if age > SESSION_OFFLINE_S and s.offline_since is None:
                    s.offline_since = now
                    party = self._party_of(s)
                    if party:
                        self._touch_party(party)
                if age > MEMBER_REMOVE_S and s.party:
                    self._drop_from_party(s.code, "offline")
                if age > SESSION_DROP_S:
                    del self.sessions[s.sid]
                    if self.by_code.get(s.code) is s:
                        del self.by_code[s.code]
            for address in list(self.holds_by_address):
                self._hold_seats(address)
            for party in self.parties.values():
                for a in [a for a, t in party.failed.items() if t <= now]:
                    del party.failed[a]
                if party.follow and now - party.follow_at > FOLLOW_S:
                    self._clear_follow(party)
                    self._touch_party(party)
            for nonce, (expiry, sources) in list(self.nonces.items()):
                if expiry < now:
                    self._drop_nonce(nonce)
            for k in [k for k, w in self.client_limits.items() if w.idle(now)]:
                del self.client_limits[k]

    # ---- calls
    def challenge(self, q, peer="?"):
        """A single-use nonce for one hello, indexed and evicted first by source."""
        with self.lock:
            now = self.now()
            if not self._source_hit("challenge", peer, now):
                raise Refusal("rate_limited", "too many challenges", 429)
            sources = self._source_keys(peer)
            for source in sources:
                owned = self.nonces_by_source.setdefault(source, collections.OrderedDict())
                while len(owned) >= NONCES_PER_SOURCE:
                    self._drop_nonce(next(iter(owned)))
            nonce = secrets.token_hex(16)
            self.nonces[nonce] = (now + NONCE_S, sources)
            for source in sources:
                self.nonces_by_source.setdefault(source, collections.OrderedDict())[nonce] = None
            while len(self.nonces) > NONCE_MAX:
                self._drop_nonce(next(iter(self.nonces)))
            return {"nonce": nonce, "expires_s": NONCE_S}

    def _drop_nonce(self, nonce):
        item = self.nonces.pop(nonce, None)
        if not item:
            return None
        expiry, sources = item
        for source in sources:
            owned = self.nonces_by_source.get(source)
            if owned:
                owned.pop(nonce, None)
                if not owned:
                    del self.nonces_by_source[source]
        return expiry

    def _evict_idle(self, now):
        """Makes room for one session: the one idle longest goes if it is idle past the offline mark; False when all are live."""
        oldest = min(self.sessions.values(), key=lambda x: x.last, default=None)
        if oldest is None or now - oldest.last <= SESSION_OFFLINE_S:
            return False
        self._drop_from_party(oldest.code, "offline")
        self.sessions.pop(oldest.sid, None)
        if self.by_code.get(oldest.code) is oldest:
            del self.by_code[oldest.code]
        return True

    def hello(self, q, peer="?"):
        """peer is the client key (client_key): the limits count by it."""
        with self.lock:
            now = self.now()
            try:
                wire = int(q.get("wire", "0"))
            except ValueError:
                wire = 0
            if wire != WIRE:
                raise Refusal("update_required", "this service speaks wire %d" % WIRE, 426)
            nonce = q.get("nonce", "")
            if not re.fullmatch(r"[0-9a-f]{32}", nonce):
                raise Refusal("bad_nonce", "ask for a new challenge", 401)
            try:
                pub = bytes.fromhex(q.get("key", ""))
                sig = bytes.fromhex(q.get("sig", ""))
            except ValueError:
                raise Refusal("bad_value", "key or sig")
            if len(pub) != 32 or len(sig) != 64:
                raise Refusal("bad_value", "key or sig")
            # (the nonce is spent whether or not the signature passes)
            expires = self._drop_nonce(nonce)
            if expires is None or expires < now:
                raise Refusal("bad_nonce", "ask for a new challenge", 401)
            if self.hello_global.full(now) or not self._source_hit("hello", peer, now):
                raise Refusal("rate_limited", "too many hellos", 429)
            self.hello_global.hit(now)
            if not ed25519.verify(pub, ("ae-party-hello-2\n%s\n%s\n%s" % (self.origin, nonce, pub.hex())).encode(), sig):
                raise Refusal("bad_signature", "", 401)
            code = friend_code(pub)
            seats = min(SEATS_MAX, max(1, int(q.get("seats", "1") or 1)))
            old = self.by_code.get(code)
            if not old and len(self.sessions) >= SESSIONS_MAX and not self._evict_idle(now):
                raise Refusal("busy", "full", 503)
            sources = self._source_keys(peer)
            client = sources[0]
            live = sum(1 for x in self.sessions.values() if x.client == client and x is not old and now - x.last <= SESSION_OFFLINE_S)
            if live >= SESSIONS_PER_CLIENT:
                raise Refusal("rate_limited", "too many sessions from here", 429)
            sid = secrets.token_hex(16)
            s = Session(sid, pub.hex(), code, clean_name(q.get("name")), q.get("build", ""), q.get("net", ""), seats, now,
                        client, sources[-1])
            if old:
                s.party, s.invites = old.party, old.invites
                del self.sessions[old.sid]
            elif code in self.restored and self.restored[code] in self.parties:
                s.party = self.restored[code]
            self.restored.pop(code, None)
            self.sessions[sid], self.by_code[code] = s, s
            party = self._party_of(s)
            if party and code not in party.members:
                s.party = None
            if party:
                self._touch_party(party)
            self.stats["hello"] += 1
            self.log("hello %s" % code[:4])
            return {"sid": sid, "code": code, "now": int(now), "config": {"poll_ms": POLL_MS, "party_max": PARTY_MAX, "wire": WIRE},
                    "playlists": [{"id": k, "label": v["label"], "party_max": v["party_max"]} for k, v in PLAYLISTS.items()]}

    def view(self, s):
        now = self.now()
        party = self._party_of(s)
        out = {"rev": s.rev, "code": s.code}
        out["party"] = None if not party else {
            "id": party.id, "leader": party.leader, "privacy": party.privacy, "max": PARTY_MAX, "seats": self._seats(party),
            "link": party.link if party.leader == s.code else None,
            "members": [{"code": c, "name": self._name(c), "seats": self.by_code[c].seats if c in self.by_code else 1,
                         "state": self._state(c), "build": self.by_code[c].build if c in self.by_code else ""}
                        for c in party.members]}
        # (mutual friends first, then the newest; at most four, the number the game shows)
        ordered = sorted(s.invites.items(), key=lambda item: (not item[1].get("mutual"), -item[1]["expires"]))[:INVITES_SHOWN]
        out["invites"] = [{"party": pid, "from": {"code": i["from"], "name": i["name"]}, "seats": i["seats"],
                           "expires_s": max(0, int(i["expires"] - now))} for pid, i in ordered]
        out["search"] = party.search if party and party.search else None
        out["follow"] = party.follow if party and party.follow and s.code in party.follow_to and now - party.follow_at <= FOLLOW_S else None
        out["notice"] = s.notices[-1] if s.notices else None
        out["steam"] = self._steam_found(s)
        return out

    def _steam_found(self, s):
        """The Steam friends running AE, for a player who registered: only those who registered too AND asked about this player
        (a match both ways); each with the AE name, code and state. Nothing for a player who did not register."""
        if not s.steam_hash:
            return []
        out = []
        for other in self.by_code.values():
            if other is s or not other.steam_hash or other.offline_since is not None or self._blocked(s, other):
                continue
            if other.steam_hash in s.steam_watch and s.steam_hash in other.steam_watch:
                out.append({"hash": other.steam_hash, "code": other.code, "name": other.name, "state": self._state(other.code),
                            "verified": False})
        return out

    def _touch_steam(self):
        for o in self.sessions.values():
            if o.steam_hash:
                self._touch(o)

    def _state(self, code):
        s = self.by_code.get(code)
        if not s or s.offline_since is not None:
            return "offline"
        party = self._party_of(s)
        if party and party.search and party.search.get("state") == "found":
            return "searching"
        return "party" if s.party else "menus"

    def poll(self, q):
        with self.lock:
            s = self._session(q.get("sid", ""))
            # (no sweep of every session here: the housekeeping thread's tick does that, every 5 s)
            try:
                if int(q.get("rev", "0")) == s.rev:
                    return {"rev": s.rev, "same": True}
            except ValueError:
                pass
            return self.view(s)

    def _blocked(self, a, b):
        """Whether either of two sessions has blocked the other."""
        return a.code in b.blocks or b.code in a.blocks

    def _mutual(self, a, b):
        """Two players who each added the other (each one's presence list holds the other's code)."""
        return b.code in a.watch and a.code in b.watch

    def presence(self, q):
        with self.lock:
            s = self._session(q.get("sid", ""))
            codes = [parse_code(c) for c in (q.get("codes") or "").split(",")][:200]
            s.watch = {c for c in codes if c}
            if "blocks" in q:
                s.blocks = {c for c in (parse_code(c) for c in (q.get("blocks") or "").split(",")[:BLOCKS_MAX]) if c}
            out = []
            for c in s.watch:
                other = self.by_code.get(c)
                if other and s.code in other.watch and not self._blocked(s, other):
                    out.append({"code": c, "state": self._state(c)})
            return {"presence": out}

    def _need_steam_dev(self):
        if not self.steam_dev:
            raise Refusal("unknown_call", "", 404)

    def steam_register(self, q):
        """Opt in: this install's Steam id as a short hash (the game computes SHA-256("ae-steam-1" + steam id)); withdrawn by `hash=`
        empty. The service cannot check the id is the player's (no Steam secret is kept here): a match is shown as unverified."""
        self._need_steam_dev()
        with self.lock:
            s = self._session(q.get("sid", ""))
            if not s.limits["steam"].hit(self.now()):
                raise Refusal("rate_limited", "slow down", 429)
            h = (q.get("hash", "") or "").lower()
            if h and (len(h) != 12 or any(c not in "0123456789abcdef" for c in h)):
                raise Refusal("bad_value", "hash")
            s.steam_hash = h or None
            if not h:
                s.steam_watch = set()
            self._touch_steam()
            self._touch(s)
            return {"ok": True}

    def steam_lookup(self, q):
        """Ask about friends by their short hashes (40 a call; reset=1 starts the list again, up to 300 in all). The answer comes in
        `poll`'s `steam`, for hashes that asked about this player too."""
        self._need_steam_dev()
        with self.lock:
            s = self._session(q.get("sid", ""))
            if not s.steam_hash:
                raise Refusal("not_registered", "turn Steam friends on first")
            if not s.limits["steam"].hit(self.now()):
                raise Refusal("rate_limited", "slow down", 429)
            if q.get("reset") in ("1", "true"):
                s.steam_watch = set()
            for h in (q.get("hashes", "") or "").lower().split(",")[:40]:
                if len(h) == 12 and all(c in "0123456789abcdef" for c in h) and len(s.steam_watch) < 300:
                    s.steam_watch.add(h)
            self._touch_steam()
            return {"ok": True, "watching": len(s.steam_watch)}

    def _need_party(self, s):
        party = self._party_of(s)
        if not party:
            raise Refusal("not_in_party")
        return party

    def _leader_only(self, s):
        party = self._need_party(s)
        if party.leader != s.code:
            raise Refusal("not_leader", "THE LEADER INVITES")
        return party

    def party_invite(self, q):
        """The answer is {"ok": true} for any well-formed code that is not the caller's own: an invite is delivered only to a player who
        is online, in no party, has not blocked the caller and has room in the queue of invites. Whatever else, it is dropped without a
        word, so an invite tells the sender nothing about who is online. Friends see each other through `presence`."""
        with self.lock:
            s = self._session(q.get("sid", ""))
            now = self.now()
            if not s.limits["invite_min"].hit(now) or not s.limits["invite_day"].hit(now) or not self._client_hit("invite", s.client, now):
                raise Refusal("rate_limited", "too many invites", 429)
            code = parse_code(q.get("to", ""))
            if not code:
                raise Refusal("unknown_code", "That is not a friend code.")
            if code == s.code:
                raise Refusal("self", "that is you")
            party = self._party_of(s)
            if party:
                if party.privacy == "closed":
                    raise Refusal("party_closed", "THE PARTY IS CLOSED")
                if party.privacy == "invite" and party.leader != s.code:
                    raise Refusal("not_leader", "THE LEADER INVITES")
                if self._seats(party) >= PARTY_MAX:
                    raise Refusal("party_full", "THE PARTY IS FULL", room=0)
            if not party:
                # (made whether or not the invite is delivered: the sender's own view must not tell the two apart)
                party = Party(random_text(12), s.code)
                self.parties[party.id] = party
                s.party = party.id
                self._touch(s)
            target = self.by_code.get(code)
            if target and target.offline_since is None and not target.party and not self._blocked(s, target):
                mutual = self._mutual(s, target)
                others = [i for pid, i in target.invites.items() if pid != party.id]
                strangers = [i for i in others if not i.get("mutual")]
                if len(others) < INVITES_MAX and (mutual or len(strangers) < STRANGER_INVITES_MAX):
                    target.invites[party.id] = {"from": s.code, "name": s.name, "seats": self._seats(party), "expires": now + INVITE_S,
                                                "mutual": mutual}
                    self._touch(target)
                    self.log("invite %s -> %s" % (s.code[:4], code[:4]))
            return {"ok": True}

    def _join(self, s, party):
        if party.privacy == "closed":
            raise Refusal("party_closed", "THE PARTY IS CLOSED")
        room = PARTY_MAX - self._seats(party)
        if s.seats > room:
            raise Refusal("party_full", "THE PARTY IS FULL", room=room)
        if s.party:
            self._drop_from_party(s.code)
        party.members.append(s.code)
        self._clear_follow(party)
        s.party = party.id
        s.invites.pop(party.id, None)
        self._notice(party.leader, "%s joined" % s.name)
        self._touch_party(party)
        self._touch(s)

    def party_answer(self, q):
        with self.lock:
            s = self._session(q.get("sid", ""))
            pid = q.get("party", "")
            invite = s.invites.get(pid)
            if not invite:
                raise Refusal("link_expired", "that invite is gone")
            if q.get("join", "1") in ("0", "false", ""):
                del s.invites[pid]
                self._touch(s)
                return {"ok": True, "joined": False}
            party = self.parties.get(pid)
            if not party:
                del s.invites[pid]
                raise Refusal("party_not_found")
            self._join(s, party)
            return {"ok": True, "joined": True}

    def party_link(self, q):
        with self.lock:
            s = self._session(q.get("sid", ""))
            now = self.now()
            if now < s.link_locked_until:
                raise Refusal("rate_limited", "locked for a while", 429)
            if not s.limits["link"].hit(now):
                raise Refusal("rate_limited", "too many tries", 429)
            link = (q.get("link", "") or "").strip().upper()
            party = self.parties.get(link[:12])
            if not party or party.secret != link[12:24] or len(link) != 24 or party.privacy == "closed":
                s.link_failures = [t for t in s.link_failures if now - t < 600] + [now]
                if len(s.link_failures) >= 5:
                    s.link_locked_until = now + 600
                raise Refusal("link_expired", "that link no longer works")
            if s.party == party.id:
                raise Refusal("in_party", "already in this party")
            self._join(s, party)
            return {"ok": True}

    def party_new_link(self, q):
        with self.lock:
            s = self._session(q.get("sid", ""))
            party = self._leader_only(s)
            party.secret = random_text(12)
            self._touch(s)
            return {"ok": True}

    def party_leave(self, q):
        with self.lock:
            s = self._session(q.get("sid", ""))
            self._need_party(s)
            self._drop_from_party(s.code, "left")
            return {"ok": True}

    def party_kick(self, q):
        with self.lock:
            s = self._session(q.get("sid", ""))
            party = self._leader_only(s)
            code = parse_code(q.get("code", ""))
            if code == s.code:
                raise Refusal("self", "that is you")
            if code not in party.members:
                raise Refusal("unknown_member")
            self._notice(code, "You were removed from the party")
            self._drop_from_party(code)
            return {"ok": True}

    def party_promote(self, q):
        with self.lock:
            s = self._session(q.get("sid", ""))
            party = self._leader_only(s)
            code = parse_code(q.get("code", ""))
            if code == s.code:
                raise Refusal("self", "that is you")
            if code not in party.members:
                raise Refusal("unknown_member")
            party.leader = code
            self._clear_follow(party)
            self._cancel_search(party, "leader_changed")
            for c in party.members:
                self._notice(c, "%s leads now" % self._name(code))
            return {"ok": True}

    def party_privacy(self, q):
        with self.lock:
            s = self._session(q.get("sid", ""))
            party = self._leader_only(s)
            level = q.get("level", "")
            if level not in ("invite", "friends", "closed"):
                raise Refusal("bad_value", "level")
            party.privacy = level
            party.secret = random_text(12)
            self._touch_party(party)
            return {"ok": True}

    def party_seats(self, q):
        with self.lock:
            s = self._session(q.get("sid", ""))
            try:
                seats = int(q.get("seats", "0"))
            except ValueError:
                seats = 0
            if not 1 <= seats <= SEATS_MAX:
                raise Refusal("bad_value", "seats")
            party = self._party_of(s)
            if party and seats > s.seats and seats - s.seats > PARTY_MAX - self._seats(party):
                raise Refusal("party_full", "THE PARTY IS FULL", room=PARTY_MAX - self._seats(party))
            s.seats = seats
            if party:
                self._touch_party(party)
            return {"ok": True}

    def party_follow(self, q):
        with self.lock:
            s = self._session(q.get("sid", ""))
            party = self._leader_only(s)
            kind = q.get("kind", "")
            address, invite = q.get("address", ""), q.get("invite", "")
            # (only what a player's PC may be sent to: a server the service lists, or a real hosted-game invite; no LAN follow)
            if kind == "server":
                if address not in {srv["address"] for srv in self.servers}:
                    raise Refusal("bad_value", "address")
                invite = ""
            elif kind == "hosted":
                if not re.fullmatch(r"[0-9a-f]{64}", invite):
                    raise Refusal("bad_value", "invite")
                address = ""
            else:
                raise Refusal("bad_value", "kind")
            now = self.now()
            if any(w.full(now) for w in party.follow_limits):
                raise Refusal("rate_limited", "one follow at a time", 429)
            for w in party.follow_limits:
                w.hit(now)
            n = self._next_n(party)
            party.follow = {"kind": kind, "address": address, "invite": invite, "n": n, "from": s.code}
            party.follow_to, party.follow_at = set(party.members), now
            self._touch_party(party)
            return {"ok": True, "n": n}

    def quick_play(self, q):
        with self.lock:
            s = self._session(q.get("sid", ""))
            now = self.now()
            if not s.limits["quick"].hit(now) or not self._client_hit("quick", s.client, now):
                raise Refusal("rate_limited", "too many searches", 429)
            party = self._party_of(s)
            if party and party.leader != s.code:
                raise Refusal("not_leader", "Chief picks the playlist")
            playlist = q.get("playlist", "")
            if playlist not in PLAYLISTS:
                raise Refusal("bad_playlist", playlist)
            limit = PLAYLISTS[playlist]["party_max"]
            seats = self._seats(party) if party else s.seats
            if seats > limit:
                raise Refusal("party_too_big", "PARTY OF %d, %d MAX" % (seats, limit), limit=limit)
            if not party:
                party = Party(random_text(12), s.code)
                self.parties[party.id] = party
                s.party = party.id
            self._clear_hold(party)
            pings = {}
            for item in (q.get("pings") or "").split(",")[:20]:
                address, _, ms = item.partition("=")
                if address and ms.isdigit() and int(ms) < 5000:
                    pings[address] = int(ms)
            held = {srv["address"]: self._hold_seats(srv["address"], srv) for srv in self.servers}
            # (a report counts only for the server this party was just sent to: nobody can bar a server for another party)
            reported = q.get("failed") if q.get("again") else None
            if reported and party.last_found and reported == party.last_found[0] and now - party.last_found[1] <= FAILED_REPORT_S:
                party.failed[reported] = now + FAILED_SERVER_S
                while len(party.failed) > FAILED_MAX:
                    del party.failed[min(party.failed, key=party.failed.get)]
            failed = {a for a, t in party.failed.items() if t > now}
            result = pick_server(self.servers, playlist, seats, pings, held, failed, q.get("prefer") or None)
            self.stats["quick_play"] += 1
            n = self._next_n(party)
            if result["server"]:
                srv = result["server"]
                expires = int(now + HOLD_S)
                self._add_hold(party, srv, self._live_seats(party, now), (s.client, s.client_coarse), now)
                party.last_found = (srv["address"], now)
                codes = list(party.members)
                ticket = make_ticket(self.seed, party.id, srv["address"], codes, seats, expires)
                party.search = {"state": "found", "playlist": playlist, "n": n, "seats": seats, "expires_s": HOLD_S, "ticket": ticket,
                                "server": {"address": srv["address"], "name": srv.get("name", srv["address"]),
                                           "players": srv.get("players", 0), "max": srv.get("max_players", 16),
                                           "ping_ms": result["ping_ms"]}, "again": bool(q.get("again"))}
                self.stats["found"] += 1
                self.log("quick %s %s seats=%d -> %s" % (s.code[:4], playlist, seats, srv["address"]))
            else:
                partial = result.get("partial")
                party.search = {"state": "bots", "playlist": playlist, "n": n, "seats": seats,
                                "ask": PLAYLISTS[playlist]["ask_bots"], "reason": "no_room" if partial else "none_found",
                                "room_for": partial["room"] if partial else 0,
                                "partial": None if not partial else {"address": partial["server"]["address"],
                                                                      "name": partial["server"].get("name", "")}}
                self.stats["bots"] += 1
                self.log("quick %s %s seats=%d -> bots" % (s.code[:4], playlist, seats))
            self._touch_party(party)
            return {"ok": True, "search": party.search}

    def quick_cancel(self, q):
        with self.lock:
            s = self._session(q.get("sid", ""))
            party = self._leader_only(s)
            self._cancel_search(party, "cancelled")
            return {"ok": True}

    def bye(self, q):
        with self.lock:
            s = self._session(q.get("sid", ""))
            if s.party:
                self._drop_from_party(s.code, "left")
            self.sessions.pop(s.sid, None)
            if self.by_code.get(s.code) is s:
                del self.by_code[s.code]
            return {"ok": True}

    def health(self):
        with self.lock:
            return {"ok": True, "wire": WIRE, "sessions": len(self.sessions), "parties": len(self.parties),
                    "servers": len(self.servers), "stats": dict(self.stats)}

    # ---- state file
    def snapshot(self):
        with self.lock:
            now = self.now()
            return {"saved": int(now), "parties": [{
                "id": p.id, "secret": p.secret, "leader": p.leader, "privacy": p.privacy, "members": p.members,
                "seats": {c: self.by_code[c].seats for c in p.members if c in self.by_code}} for p in self.parties.values()]}

    def restore(self, data):
        with self.lock:
            if self.now() - data.get("saved", 0) > 300:
                return 0
            count = 0
            for d in data.get("parties", []):
                if len(d.get("members", [])) < 1 or d["leader"] not in d["members"]:
                    continue
                p = Party(d["id"], d["leader"])
                p.secret, p.privacy, p.members = d["secret"], d.get("privacy", "invite"), list(d["members"])
                self.parties[p.id] = p
                for c in p.members:
                    self.restored[c] = p.id
                count += 1
            return count


OPS = ("challenge", "hello", "poll", "presence", "steam_register", "steam_lookup", "party_invite", "party_answer", "party_link", "party_new_link", "party_leave", "party_kick",
       "party_promote", "party_privacy", "party_seats", "party_follow", "quick_play", "quick_cancel", "bye")


def load_servers(path):
    try:
        with open(path) as f:
            data = json.load(f)
        return [d for d in data if isinstance(d, dict) and d.get("address") and d.get("playlist")]
    except (OSError, ValueError):
        return None


def write_state(path, snapshot):
    """The state file holds the parties' link secrets: written owner-only (0600), by way of a temporary file."""
    tmp = path + ".tmp"
    fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w") as f:
        os.fchmod(f.fileno(), 0o600)
        json.dump(snapshot, f)
    os.replace(tmp, path)


def make_handler(service, servers_path):
    state = {"mtime": 0}

    class Handler(BaseHTTPRequestHandler):
        server_version = "ae-party/1"
        timeout = 15  # seconds a connection may stall before it is dropped

        def log_message(self, *args):
            pass

        def _answer(self, status, body):
            data = json.dumps(body, separators=(",", ":")).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(data)

        def _handle(self, q):
            path = urlparse(self.path).path
            path = path[len("/party"):] if path.startswith("/party/") else path
            if servers_path:
                try:
                    mtime = os.stat(servers_path).st_mtime
                    if mtime != state["mtime"]:
                        loaded = load_servers(servers_path)
                        if loaded is not None:
                            service.servers, state["mtime"] = loaded, mtime
                except OSError:
                    pass
            if path == "/v1/health":
                return self._answer(200, service.health())
            m = re.fullmatch(r"/v1/([a-z_]+)", path)
            if not m or m.group(1) not in OPS:
                return self._answer(404, {"error": "unknown_call"})
            op = m.group(1)
            try:
                if op in ("hello", "challenge"):
                    forwarded = ",".join(self.headers.get_all("X-Forwarded-For") or [])
                    body = getattr(service, op)(q, service.client_keys(self.client_address[0], forwarded))
                else:
                    body = getattr(service, op)(q)
                self._answer(200, body)
            except ValueError:
                service.stats["refused"] += 1
                self._answer(400, {"error": "bad_value", "detail": "a number"})
            except Refusal as r:
                service.stats["refused"] += 1
                body = {"error": r.code, "detail": r.detail}
                body.update(r.extra)
                self._answer(r.status, body)

        def do_GET(self):
            # (GET is the health check alone: no call of the wire is a GET, so no query carries anything)
            if urlparse(self.path).path in ("/v1/health", "/party/v1/health"):
                return self._handle({})
            self._answer(405, {"error": "use_post", "detail": "every call is a POST of a JSON body"})

        def do_POST(self):
            try:
                n = int(self.headers.get("Content-Length", ""))
            except ValueError:
                n = -1
            if n < 0:
                return self._answer(400, {"error": "bad_length", "detail": "Content-Length"})
            if n > BODY_MAX:
                return self._answer(413, {"error": "too_big", "detail": "body"})
            try:
                q = json.loads(self.rfile.read(n) or b"{}")
            except ValueError:
                q = None
            if not isinstance(q, dict):
                return self._answer(400, {"error": "bad_json", "detail": "a JSON object"})
            self._handle({k: str(v) for k, v in q.items()})

    return Handler


def serve(args):
    seed = None
    if args.key_file:
        with open(args.key_file) as f:
            seed = bytes.fromhex(f.read().split()[0])
    service = Service(seed, origin=args.origin, trusted_proxies=[a.strip() for a in args.trusted_proxy.split(",") if a.strip()],
                      steam_dev=args.steam_dev)
    service.log = lambda text: print("party: " + text, flush=True)
    if args.state and os.path.exists(args.state):
        try:
            with open(args.state) as f:
                print("party: restored %d parties" % service.restore(json.load(f)), flush=True)
        except (OSError, ValueError):
            pass
    if args.servers:
        service.servers = load_servers(args.servers) or []
    server = ThreadingHTTPServer((args.host, args.port), make_handler(service, args.servers))
    stop = threading.Event()

    def housekeeping():
        while not stop.wait(5):
            service.tick()
            if args.state:
                write_state(args.state, service.snapshot())

    threading.Thread(target=housekeeping, daemon=True).start()
    print("party: service key %s listening on %s:%d" % (service.public.hex()[:16], args.host, args.port), flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        if args.state:
            write_state(args.state, service.snapshot())


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8780)
    parser.add_argument("--key-file", help="the service's Ed25519 seed, 64 hex characters (a throwaway key when absent)")
    parser.add_argument("--servers", help="JSON list of the AE servers: address, playlist, name, players, max_players, reachable")
    parser.add_argument("--state", help="the small state file that lets parties survive a restart")
    parser.add_argument("--trusted-proxy", default="127.0.0.1,::1", help="the proxies whose X-Forwarded-For is believed (the last "
                        "entry is the client's address), comma separated")
    parser.add_argument("--steam-dev", action="store_true", help="DEV tests only: take the steam_* calls (reversible short hashes of "
                        "Steam ids). Never on a public service: they are refused (404) until Steam sign-in is built")
    parser.add_argument("--origin", help="this service's address as players write it (network.party_service, scheme and host in "
                        "lower case, no trailing slash), for example https://master.example/party: every hello is signed for it; "
                        "required unless the service listens on 127.0.0.1 (then http://127.0.0.1:<port>)")
    args = parser.parse_args()
    if not args.origin:
        if args.host not in ("127.0.0.1", "::1", "localhost"):
            parser.error("--origin is required unless --host is a loopback address")
        args.origin = "http://%s:%d" % ("127.0.0.1" if args.host == "localhost" else args.host, args.port)
    serve(args)


if __name__ == "__main__":
    main()
