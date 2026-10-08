"""Stand-in system link machines for testing large multiplayer sessions.

Joins a native-build Halo host (port/) with many
lightweight machines, each with one player, speaking the game's system link
protocol directly: TCP to the host's port 5150 for the game's messages, UDP for
player input. Each machine binds its own loopback address (127.0.0.2,
127.0.0.3, ...) because the host tells machines apart by address, so run it on
the host's computer (or give it --first-address on a network where the
addresses are yours).

    python tools/system_link_bots.py --machines 127 --start

joins 127 machines to the host at 127.0.0.1, marks the map precached, asks
the host to start once everyone is in, then plays: every machine
acknowledges each tick's update and sends the client's game update message
(standing still and slowly turning), which keeps its connection alive. The
bots do not simulate the game, so by default their players stand where they
spawn: the host no longer takes the input in that message. Ctrl+C leaves.

    python tools/system_link_bots.py --machines 8 --start --move [--fire] [--seed N]

makes the players move. Each machine then also sends its player's input the
way a real client's game does, through the distributed netcode
(port/linux/NETCODE.md, network_distributed.c): a player_inputs message
every tick, unreliably to the host's port, stamped with the machine's own
tick, which starts at the host's (the game time in the host's own messages)
and only ever jumps forward to it, as a client's clock does. The input is an
open-loop pattern from a random generator seeded per bot (--seed and the
bot's number): run forward for 1 to 3 seconds, strafe left or right, turn by
random amounts, jump and crouch now and then, and with --fire hold the
trigger in bursts. The host runs the players from that input: a client's
own predicted position (the player_prediction message) is only a
correction the host takes within a tolerance, so a bot that sends none
moves as the host's simulation of its input has it, which is the host's
copy of any client's player anyway. Shots: a client deals no damage, it
reports its hits, and the bots report none, so their shots fly but hurt no
one (nor do the bots aim, pick things up, or change a weapon that runs dry:
the input asks for no weapon, grenade or zoom). The host's PRE-GAME COUNTDOWN (when the gametype has it) holds
everyone's input but the facing for its first 3 seconds. With --move every
status line also says how many bots the host has moved since the last one,
from the host's own unit states, which it sends each machine.

The protocol (network_messages.c): a message is a 2-byte big-endian header
(length << 4 | type << 2, length including the header), then a packet: a
version byte, the fields big-endian (bytes and raw fields as they are), and
the packet type in a trailing byte. The native builds send the game settings
record in pieces of HALO_PORT_NETWORK_GAME_SETTINGS_FRAGMENT_SIZE bytes.
The distributed netcode's messages are of the "data" type (2) instead: the
same header, then its own (network_distributed.h: the message's kind and
count bytes, the sender's tick as a little-endian long) and its entries as
the structures are in memory (little-endian). A batch (kind 16) holds a
tick's messages to one machine, each a little-endian length word and its
bytes past the 2-byte header.
"""

import argparse
import errno
import ipaddress
import math
import random
import selectors
import socket
import struct
import sys
import time

SERVER_PORT = 0x141E
CLIENT_PORT = 0x141F
MESSAGE_TYPE_PACKET = 3
PACKET_VERSION = 1
JOIN_TOKEN = b"message in a bottle"[:16]  # network_game_generate_join_game_token (DEBUG builds)
SETTINGS_FRAGMENT_SIZE = 0xE00
NONE = -1

# network_game_message_type
CLIENT_BROADCAST_GAME_SEARCH = 0
SERVER_GAME_ADVERTISE = 2
SERVER_MACHINE_ACCEPTED = 4
SERVER_MACHINE_REJECTED = 5
SERVER_GAME_SETTINGS_UPDATE = 6
SERVER_PREGAME_COUNTDOWN = 7
SERVER_BEGIN_GAME = 8
SERVER_GRACEFUL_GAME_EXIT_PREGAME = 9
SERVER_PREGAME_KEEP_ALIVE = 10
SERVER_POSTGAME_KEEP_ALIVE = 11
CLIENT_JOIN_GAME_REQUEST = 12
CLIENT_ADD_PLAYER_REQUEST_PREGAME = 13
CLIENT_SETTINGS_REQUEST = 15
CLIENT_GAME_START_REQUEST = 17
CLIENT_MAP_IS_PRECACHED_PREGAME = 19
SERVER_GAME_UPDATE = 20
SERVER_ADD_PLAYER_INGAME = 21
SERVER_REMOVE_PLAYER_INGAME = 22
SERVER_GAME_OVER = 23
CLIENT_LOADED = 24
CLIENT_GAME_UPDATE = 25
SERVER_SWITCH_TO_PREGAME = 30
SERVER_GRACEFUL_GAME_EXIT_POSTGAME = 31

COUNTDOWN_EVENT_START_IMMEDIATELY = 3

# the distributed netcode (port/linux/game/network_distributed.h)
MESSAGE_TYPE_DATA = 2
DISTRIBUTED_UNIT_STATES = 2
DISTRIBUTED_PLAYER_INPUTS = 14
DISTRIBUTED_BATCH = 16
DISTRIBUTED_INPUT_HISTORY = 4
TICKS_PER_SECOND = 30
NO_PLAYER = 0xFF
NUMBER_OF_PLAYER_POWERUPS = 2
# struct player_action control flags (units.h, _unit_control_..._bit)
CONTROL_CROUCH = 1 << 0
CONTROL_JUMP = 1 << 1
CONTROL_PRIMARY_TRIGGER = 1 << 11

MESSAGE_NAMES = {
    SERVER_GAME_ADVERTISE: "advertise", SERVER_MACHINE_ACCEPTED: "machine_accepted",
    SERVER_MACHINE_REJECTED: "machine_rejected", SERVER_GAME_SETTINGS_UPDATE: "settings",
    SERVER_PREGAME_COUNTDOWN: "countdown", SERVER_BEGIN_GAME: "begin_game",
    SERVER_GRACEFUL_GAME_EXIT_PREGAME: "exit_pregame", SERVER_PREGAME_KEEP_ALIVE: "keep_alive",
    SERVER_POSTGAME_KEEP_ALIVE: "postgame_keep_alive", SERVER_GAME_UPDATE: "game_update",
    SERVER_ADD_PLAYER_INGAME: "add_player_ingame", SERVER_REMOVE_PLAYER_INGAME: "remove_player_ingame",
    SERVER_GAME_OVER: "game_over", SERVER_SWITCH_TO_PREGAME: "switch_to_pregame",
    SERVER_GRACEFUL_GAME_EXIT_POSTGAME: "exit_postgame",
}


def network_game_layout(machines, players):
    """Offsets of struct network_game (port/linux/include/halo_port_limits.h)."""
    player_count = 0x114 + machines * 0x44
    players_offset = player_count + 2
    players_end = players_offset + players * 0x20
    # (the record ends 0x2A bytes past the players since the gametype's PC
    # options, HALO_PORT_NETWORK_GAME_SIZE; 0xE before them)
    return {"map_name": 0x24, "machine_count": 0x112, "player_count": player_count,
            "players": players_offset, "sizes": (players_end + 0x2A, players_end + 0xE)}


def wide(text, count):
    units = [ord(c) for c in text[:count - 1]]
    units += [0] * (count - len(units))
    return struct.pack(">%dH" % count, *units)


def message(packet_type, payload):
    packet = bytes([PACKET_VERSION]) + payload + bytes([packet_type])
    length = len(packet) + 2
    assert length <= 0xFFF, "message too long for its header"
    return struct.pack(">H", (length << 4) | (MESSAGE_TYPE_PACKET << 2)) + packet


def network_player(name, machine_index, color):
    # shorts 12 (name), shorts 2 (colour, icon), bytes 4 (machine, controller, team, list index)
    return (wide(name, 12) + struct.pack(">hh", color, 0) +
            struct.pack("bbbb", machine_index, 0, NONE, NONE))


def player_action(yaw):
    # longs 6 (control flags, desired facing yaw and pitch, throttle i and j,
    # primary trigger), shorts 3 (weapon, grenade, zoom level); the pad is not sent
    return struct.pack(">Ifffff", 0, yaw, 0.0, 0.0, 0.0, 0.0) + struct.pack(">hhh", 0, 0, NONE)


def distributed_message(kind, count, game_time, entries):
    """a distributed netcode message: the message header (big-endian, as every
    message's), then struct distributed_message_header's kind, count and the
    sender's tick, then the entries (little-endian, as in memory)"""
    length = 2 + 6 + len(entries)
    assert length <= 0xFFF, "message too long for its header"
    return (struct.pack(">H", (length << 4) | (MESSAGE_TYPE_DATA << 2)) +
            struct.pack("<BBi", kind, count, game_time) + entries)


def distributed_player_input(player_index, tick, host_time, flags, yaw, pitch, forward, left, trigger):
    """struct distributed_player_input: the player's absolute index (its slot
    in the host's player list), 3 pad bytes, the client's tick, the host's
    latest tick it has had (NONE: none), the tick's struct player_action
    (control flags, facing yaw and pitch, throttle forward and left, primary
    trigger, weapon, grenade and zoom: NONE, the current ones, pad), and the
    control flags of this tick and the 3 before it, newest first"""
    history = (list(flags) + [0] * DISTRIBUTED_INPUT_HISTORY)[:DISTRIBUTED_INPUT_HISTORY]
    return (struct.pack("<B3xii", player_index, tick, host_time) +
            struct.pack("<Ifffffhhhh", history[0], yaw, pitch, forward, left, trigger, NONE, NONE, NONE, 0) +
            struct.pack("<%dH" % DISTRIBUTED_INPUT_HISTORY, *history))


def unit_states(data):
    """the entries of a _distributed_message_unit_states message (each of a
    size of its own: distributed_unit_state_write), as (player index, alive,
    position or None)"""
    cursor = 0
    while cursor + 3 <= len(data):
        player_index, flags, parts = data[cursor], data[cursor + 1], data[cursor + 2]
        cursor += 3
        position = None
        alive = bool(flags & 1)
        if alive:
            cursor += 4  # unit
            if parts & 1:  # riding: vehicle, seat
                cursor += 6
            if flags & 2:  # placed: position, velocity, forward, (up)
                if cursor + 12 > len(data):
                    return
                position = struct.unpack_from("<fff", data, cursor)
                cursor += 12 + 6 + 6 + (6 if parts & 2 else 0)
            if flags & 0x80:  # the client's predicted tick
                cursor += 2
            cursor += 4  # health, shields
            if parts & 4:
                cursor += 8
            if parts & 8:
                cursor += 2 * NUMBER_OF_PLAYER_POWERUPS
            if parts & 16:
                cursor += 2
        elif parts & 32:  # who killed it
            cursor += 1
        if cursor > len(data):
            return
        yield player_index, alive, position


class Mover:
    """a bot's open-loop input, a tick at a time: runs of 1 to 3 seconds,
    strafes, turns of random amounts, now and then a jump or a crouch, and
    (fire) the trigger held in bursts, from a generator of its own"""

    def __init__(self, seed, fire):
        self.random = random.Random(seed)
        self.fire = fire
        self.yaw = self.random.uniform(0, 2 * math.pi)
        self.pitch = 0.0
        self.forward = self.left = 0.0
        self.segment_ticks = 0
        self.turn_rate = 0.0
        self.turn_ticks = 0
        self.crouch_ticks = 0
        self.jump_ticks = 0
        self.fire_ticks = 0
        self.rest_ticks = self.random.randint(1, 3) * TICKS_PER_SECOND

    def seconds(self, low, high):
        return max(1, int(self.random.uniform(low, high) * TICKS_PER_SECOND))

    def step(self):
        """the next tick's (control flags, yaw, pitch, forward, left, trigger)"""
        rnd = self.random
        if self.segment_ticks <= 0:
            choice = rnd.random()
            if choice < 0.55:
                # run forward, perhaps a little to one side
                self.forward, self.left = 1.0, rnd.choice((0.0, 0.0, 0.5, -0.5))
                self.segment_ticks = self.seconds(1.0, 3.0)
            elif choice < 0.85:
                # strafe left or right, perhaps moving forward too
                self.forward, self.left = rnd.choice((0.0, 0.5)), rnd.choice((1.0, -1.0))
                self.segment_ticks = self.seconds(0.5, 1.5)
            else:
                # stand, or back away
                self.forward, self.left = rnd.choice((0.0, -1.0)), 0.0
                self.segment_ticks = self.seconds(0.3, 1.0)
            # a turn of a random amount, over a quarter to a full second
            if rnd.random() < 0.7:
                amount = rnd.uniform(math.radians(20), math.radians(200)) * rnd.choice((1, -1))
                self.turn_ticks = self.seconds(0.25, 1.0)
                self.turn_rate = amount / self.turn_ticks
            self.pitch = rnd.uniform(-0.2, 0.2)
            if rnd.random() < 0.3:
                self.jump_ticks = 2
            if rnd.random() < 0.15:
                self.crouch_ticks = self.seconds(0.5, 1.5)
        self.segment_ticks -= 1
        if self.turn_ticks > 0:
            self.turn_ticks -= 1
            self.yaw = (self.yaw + self.turn_rate) % (2 * math.pi)
        flags = 0
        if self.jump_ticks > 0:
            self.jump_ticks -= 1
            flags |= CONTROL_JUMP
        if self.crouch_ticks > 0:
            self.crouch_ticks -= 1
            flags |= CONTROL_CROUCH
        trigger = 0.0
        if self.fire:
            if self.fire_ticks > 0:
                self.fire_ticks -= 1
                flags |= CONTROL_PRIMARY_TRIGGER
                trigger = 1.0
                if self.fire_ticks == 0:
                    self.rest_ticks = self.seconds(1.0, 3.0)
            elif self.rest_ticks > 0:
                self.rest_ticks -= 1
                if self.rest_ticks == 0:
                    self.fire_ticks = self.seconds(0.3, 1.5)
        return flags, self.yaw, self.pitch, self.forward, self.left, trigger


class Machine:
    def __init__(self, index, address, host, log):
        self.index = index
        self.address = address
        self.host = host
        self.log = log
        self.name = "bot%d" % index
        self.state = "connecting"
        self.machine_index = None
        self.buffer = b""
        self.settings = bytearray()
        self.settings_complete = None
        self.map_name = None
        self.last_update_number = None
        self.updates_received = 0
        self.bytes_received = 0
        self.last_precache_time = 0
        self.player_added = False
        self.player_list_index = None
        self.connect_attempts = 0
        self.retry_time = 0
        self.load_seconds = 1.0
        # --move: the distributed netcode's input (Mover), this machine's
        # player's slot, its clock, and the host's latest tick and word on
        # where the player is
        self.mover = None
        self.player_index = None
        self.host_time = NONE
        self.clock_tick = None
        self.clock_time = 0.0
        self.sent_tick = None
        self.flag_history = []
        self.inputs_sent = 0
        self.position = None
        self.tcp = None
        self.udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # the host's own client holds 0.0.0.0 on this port (with SO_REUSEADDR);
        # Linux lets another socket bind a single address on it only if it
        # sets SO_REUSEADDR too
        self.udp.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.udp.bind((address, CLIENT_PORT))
        self.udp.setblocking(False)

    def connect(self):
        self.connect_attempts += 1
        self.state = "connecting"
        self.tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.tcp.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1 << 20)
        self.tcp.bind((self.address, 0))
        self.tcp.setblocking(False)
        try:
            self.tcp.connect((self.host, SERVER_PORT))
        except BlockingIOError:
            pass
        except OSError as error:
            # a non-blocking connect in progress (Windows says would block)
            if error.errno not in (errno.EINPROGRESS, errno.EWOULDBLOCK, 10035):
                raise

    def close(self, selector):
        """stops using the connection: a connection the host keeps but no one
        reads would fill its send buffer"""
        self.state = "closed"
        if self.tcp:
            try:
                selector.unregister(self.tcp)
            except (KeyError, ValueError):
                pass
            self.tcp.close()
            self.tcp = None

    def send(self, data):
        view = memoryview(data)
        deadline = time.monotonic() + 5
        while view:
            try:
                sent = self.tcp.send(view)
                view = view[sent:]
            except BlockingIOError:
                if time.monotonic() > deadline:
                    raise
                time.sleep(0.001)

    def joined(self):
        # the name, the token, and (since the bans: network_messages.c) the
        # machine's hardware id, 0x20 bytes of hex, one of its own per bot
        hardware_id = ("%032x" % (0xB07 << 64 | self.index)).encode("ascii")
        self.send(message(CLIENT_JOIN_GAME_REQUEST, wide(self.name, 32) + JOIN_TOKEN + hardware_id))
        self.state = "joining"

    def receive(self):
        try:
            data = self.tcp.recv(1 << 20)
        except BlockingIOError:
            return True
        except ConnectionError:
            self.log("%s: connection lost" % self.name)
            self.state = "closed"
            return False
        if not data:
            self.log("%s: host closed the connection" % self.name)
            self.state = "closed"
            return False
        self.bytes_received += len(data)
        self.buffer += data
        while len(self.buffer) >= 2:
            header = struct.unpack(">H", self.buffer[:2])[0]
            length = header >> 4
            if length < 3:
                self.log("%s: bad message header %04x" % (self.name, header))
                self.state = "closed"
                return False
            if len(self.buffer) < length:
                break
            packet = self.buffer[2:length]
            self.buffer = self.buffer[length:]
            # (the distributed netcode's reliable messages are "data", not
            # packets: no packet type at their end)
            if (header >> 2) & 3 == MESSAGE_TYPE_DATA:
                self.handle_data(packet)
            else:
                self.handle(packet[-1], packet[1:-1])
        return True

    def receive_datagrams(self):
        """(--move) the host's datagrams: the distributed netcode's per-tick
        messages, for the host's tick and where it has this machine's player"""
        while True:
            try:
                data = self.udp.recv(0x10000)
            except (BlockingIOError, InterruptedError):
                return
            except OSError:
                return
            if len(data) >= 8 and (struct.unpack(">H", data[:2])[0] >> 2) & 3 == MESSAGE_TYPE_DATA:
                self.handle_data(data[2:])

    def handle_data(self, data):
        """a distributed message past its 2-byte header (a batch's messages
        each in turn)"""
        if len(data) < 6:
            return
        kind, count, game_time = struct.unpack_from("<BBi", data, 0)
        if self.host_time == NONE or game_time > self.host_time:
            self.host_time = game_time
        if kind == DISTRIBUTED_BATCH:
            offset = 6
            while offset + 2 <= len(data):
                length = struct.unpack_from("<H", data, offset)[0]
                offset += 2
                if length < 6 or offset + length > len(data):
                    break
                if data[offset] != DISTRIBUTED_BATCH:
                    self.handle_data(data[offset:offset + length])
                offset += length
        elif kind == DISTRIBUTED_UNIT_STATES and self.player_index is not None:
            for player_index, alive, position in unit_states(data[6:]):
                if player_index == self.player_index and position:
                    self.position = position

    def find_player_index(self):
        """this machine's player's slot in the host's player list (the
        settings' players), which is its datum's absolute index"""
        settings = self.settings_complete
        if not settings or self.machine_index is None:
            return None
        for machine_slots, player_slots in ((128, 128), (4, 16)):
            layout = network_game_layout(machine_slots, player_slots)
            if len(settings) not in layout["sizes"]:
                continue
            for slot in range(player_slots):
                offset = layout["players"] + slot * 0x20
                machine, controller = struct.unpack_from("bb", settings, offset + 0x1C)
                if machine == self.machine_index and controller == 0:
                    return slot
        return None

    def send_inputs(self, now):
        """(--move) the player's input of each tick this machine's clock has
        reached since the last it sent: its clock starts at the host's tick
        and only ever jumps forward to it, as a real client's, and the host
        drops a message whose tick is not newer than the last"""
        if self.host_time == NONE or self.player_index is None:
            return
        if self.clock_tick is None or self.host_time > self.clock_tick + (now - self.clock_time) * TICKS_PER_SECOND:
            self.clock_tick, self.clock_time = self.host_time, now
        tick = self.clock_tick + int((now - self.clock_time) * TICKS_PER_SECOND)
        if self.sent_tick is not None and tick <= self.sent_tick:
            return
        # (each tick's buttons, the ticks skipped too, the history newest first)
        steps = 1 if self.sent_tick is None else min(tick - self.sent_tick, TICKS_PER_SECOND)
        for _ in range(steps):
            flags, yaw, pitch, forward, left, trigger = self.mover.step()
            self.flag_history = ([flags] + self.flag_history)[:DISTRIBUTED_INPUT_HISTORY]
        self.sent_tick = tick
        entry = distributed_player_input(self.player_index, tick, self.host_time, self.flag_history,
                                         yaw, pitch, forward, left, trigger)
        try:
            self.udp.sendto(distributed_message(DISTRIBUTED_PLAYER_INPUTS, 1, tick, entry), (self.host, SERVER_PORT))
            self.inputs_sent += 1
        except OSError:
            pass

    def handle(self, packet_type, payload):
        if packet_type == SERVER_MACHINE_ACCEPTED:
            random_seed, self.machine_index = struct.unpack(">ih", payload[:6])
            self.state = "pregame"
            self.send(message(CLIENT_SETTINGS_REQUEST, wide(self.name, 32) + bytes([self.machine_index & 0xFF])))
        elif packet_type == SERVER_MACHINE_REJECTED:
            self.log("%s: rejected, reason %d" % (self.name, struct.unpack(">h", payload[:2])[0]))
            self.state = "rejected"
        elif packet_type == SERVER_GAME_SETTINGS_UPDATE:
            total, offset, length = struct.unpack(">HHH", payload[:6])
            data = payload[6:6 + length]
            if offset == 0:
                self.settings = bytearray()
            if offset == len(self.settings):
                self.settings += data
            if len(self.settings) == total:
                self.settings_complete = bytes(self.settings)
                self.map_name = self.settings_complete[0x24:0x24 + 0x80].split(b"\0")[0]
                if not self.player_added and self.machine_index is not None:
                    self.player_added = True
                    color = self.index % 18
                    self.send(message(CLIENT_ADD_PLAYER_REQUEST_PREGAME,
                                      network_player(self.name, self.machine_index, color)))
        elif packet_type == SERVER_BEGIN_GAME:
            self.state = "loading"
            self.loaded_at = time.monotonic() + self.load_seconds
            # (a new game: a new clock, and the player's slot as the
            # settings have it now)
            self.host_time = NONE
            self.clock_tick = self.sent_tick = None
            self.flag_history = []
            self.position = None
            self.player_index = self.find_player_index()
            if self.mover and self.player_index is None:
                self.log("%s: no player of this machine in the game's settings: it will not move" % self.name)
        elif packet_type == SERVER_GAME_UPDATE:
            update_number = struct.unpack(">I", payload[:4])[0]
            self.last_update_number = update_number
            self.updates_received += 1
            if self.state != "ingame":
                self.state = "ingame"
        elif packet_type in (SERVER_GAME_OVER,):
            self.state = "postgame"
        elif packet_type in (SERVER_SWITCH_TO_PREGAME,):
            self.state = "pregame"
            self.last_update_number = None
            self.player_added = True
        elif packet_type in (SERVER_GRACEFUL_GAME_EXIT_PREGAME, SERVER_GRACEFUL_GAME_EXIT_POSTGAME):
            self.log("%s: host ended the game" % self.name)
            self.state = "closed"

    def tick(self, now):
        if self.state == "pregame" and self.map_name and now - self.last_precache_time > 1.0:
            self.last_precache_time = now
            self.send(message(CLIENT_MAP_IS_PRECACHED_PREGAME, self.map_name.ljust(256, b"\0")[:256]))
        if self.state == "loading" and now >= self.loaded_at:
            self.send(message(CLIENT_LOADED, struct.pack(">i", 0)))
            self.state = "loaded"
        if self.state == "ingame" and self.last_update_number is not None:
            # the next update this machine expects (acknowledging the ones it
            # has), then an array of its players' actions: a count byte and one
            # action
            yaw = self.mover.yaw if self.mover else (now * 0.5 + self.index) % (2 * math.pi)
            payload = (struct.pack(">I", (self.last_update_number + 1) & 0x7FFFFFFF) + bytes([1]) +
                       player_action(yaw))
            try:
                self.udp.sendto(message(CLIENT_GAME_UPDATE, payload), (self.host, SERVER_PORT))
            except OSError:
                pass


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--machines", type=int, default=8)
    parser.add_argument("--first-address", default="127.0.0.2",
                        help="each machine binds this address plus its number")
    parser.add_argument("--start", action="store_true",
                        help="ask the host to start as soon as every machine has a player")
    parser.add_argument("--start-delay", type=float, default=3.0)
    parser.add_argument("--seconds", type=float, default=0, help="leave after this long (0: until Ctrl+C)")
    parser.add_argument("--rate", type=float, default=30.0, help="input messages per second per machine")
    parser.add_argument("--join-rate", type=float, default=20.0, help="machines connecting per second")
    parser.add_argument("--load-seconds", type=float, default=1.0,
                        help="how long each machine takes to load the map once the game begins")
    parser.add_argument("--status-every", type=float, default=5.0)
    parser.add_argument("--move", action="store_true",
                        help="send each player's input through the distributed netcode: they run, strafe, "
                             "turn, jump and crouch")
    parser.add_argument("--fire", action="store_true", help="(--move) hold the trigger in bursts too")
    parser.add_argument("--seed", type=int, default=1, help="(--move) the bots' patterns' seed")
    parser.add_argument("--join-delay", type=float, default=0.0,
                        help="seconds to wait before the first machine connects (after a host's game setup)")
    options = parser.parse_args()
    if options.fire and not options.move:
        parser.error("--fire needs --move")

    started = time.monotonic()

    def log(text):
        print("[%7.2f] %s" % (time.monotonic() - started, text), flush=True)

    try:
        first_address = ipaddress.IPv4Address(options.first_address)
        last_address = ipaddress.IPv4Address(int(first_address) + max(options.machines, 1) - 1)
    except ValueError as error:
        parser.error("--first-address: %s" % error)
    if first_address.is_loopback and not last_address.is_loopback:
        parser.error("--first-address: %d machines from %s run past 127.255.255.255" % (options.machines, first_address))
    if not first_address.is_loopback:
        log("warning: %s is not a loopback address; the machines bind real addresses" % first_address)
    machines = []
    for index in range(options.machines):
        address = str(first_address + index)
        machine = Machine(index + 1, address, options.host, log)
        machine.state = "waiting"
        machine.load_seconds = options.load_seconds
        if options.move:
            machine.mover = Mover(options.seed * 7919 + machine.index, options.fire)
        machines.append(machine)

    # machines connect a few at a time: a host accepts connections from its
    # frame loop, and one whose listen backlog is full refuses the rest
    selector = selectors.DefaultSelector()
    if options.move:
        for machine in machines:
            selector.register(machine.udp, selectors.EVENT_READ, (machine, "udp"))
    waiting = list(machines)
    next_connect_time = time.monotonic() + max(0.0, options.join_delay)
    log("connecting %d machines to %s:%d" % (len(machines), options.host, SERVER_PORT))

    start_requested = False
    all_in_time = None
    last_status = 0
    last_input = 0
    try:
        while True:
            now = time.monotonic()
            if waiting and now >= next_connect_time:
                machine = next((m for m in waiting if m.retry_time <= now), None)
                if machine:
                    waiting.remove(machine)
                    machine.connect()
                    selector.register(machine.tcp, selectors.EVENT_READ | selectors.EVENT_WRITE, machine)
                    next_connect_time = now + 1.0 / options.join_rate
            for key, events in selector.select(timeout=0.005):
                if isinstance(key.data, tuple):
                    key.data[0].receive_datagrams()
                    continue
                machine = key.data
                if machine.state == "connecting" and events & selectors.EVENT_WRITE:
                    error = machine.tcp.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR)
                    if error:
                        machine.close(selector)
                        if error in (errno.ECONNREFUSED, 10061) and machine.connect_attempts < 20:
                            machine.state = "waiting"
                            machine.retry_time = now + 0.5
                            waiting.append(machine)
                        else:
                            log("%s: connect failed (%d)" % (machine.name, error))
                            machine.state = "closed"
                        continue
                    selector.modify(machine.tcp, selectors.EVENT_READ, machine)
                    machine.joined()
                if events & selectors.EVENT_READ and machine.state != "closed":
                    try:
                        if not machine.receive() or machine.state == "closed":
                            machine.close(selector)
                    except OSError as error:
                        # a reply the host stopped reading, or a reset
                        log("%s: %s" % (machine.name, error))
                        machine.close(selector)
            if options.move:
                for machine in machines:
                    if machine.state == "ingame":
                        machine.send_inputs(now)
            if now - last_input >= 1.0 / options.rate:
                last_input = now
                for machine in machines:
                    if machine.state != "closed":
                        try:
                            machine.tick(now)
                        except OSError as error:
                            log("%s: %s" % (machine.name, error))
                            machine.close(selector)
            if options.start and not start_requested:
                ready = [m for m in machines if m.state == "pregame" and m.player_added and m.settings_complete]
                if len(ready) == len(machines) and all_in_time is None:
                    all_in_time = now
                    log("all %d machines are in the lobby" % len(machines))
                if all_in_time is not None and now - all_in_time >= options.start_delay:
                    start_requested = True
                    log("asking the host to start the game")
                    try:
                        machines[0].send(message(CLIENT_GAME_START_REQUEST,
                                                 struct.pack(">h", COUNTDOWN_EVENT_START_IMMEDIATELY)))
                    except OSError as error:
                        log("%s: %s" % (machines[0].name, error))
                        machines[0].close(selector)
            if now - last_status >= options.status_every:
                last_status = now
                states = {}
                for machine in machines:
                    states[machine.state] = states.get(machine.state, 0) + 1
                sample = next((m for m in machines if m.settings_complete), None)
                players = machines_in_game = None
                if sample:
                    for machine_slots in (128, 4):
                        layout = network_game_layout(machine_slots, 128 if machine_slots == 128 else 16)
                        if len(sample.settings_complete) in layout["sizes"]:
                            machines_in_game = struct.unpack_from("<h", sample.settings_complete, layout["machine_count"])[0]
                            players = struct.unpack_from("<h", sample.settings_complete, layout["player_count"])[0]
                updates = [m.last_update_number for m in machines if m.last_update_number is not None]
                log("states %s; host game: %s machines, %s players; latest update %s; received %.1f MB" % (
                    states, machines_in_game, players, max(updates) if updates else None,
                    sum(m.bytes_received for m in machines) / 1e6))
                if options.move:
                    # (how many of the bots the host has moved, by its word on
                    # where their players are, since the last status)
                    moved = placed = 0
                    for machine in machines:
                        if machine.position:
                            placed += 1
                            last = getattr(machine, "status_position", None)
                            if last and math.dist(last, machine.position) > 0.5:
                                moved += 1
                            machine.status_position = machine.position
                    sample = next((m for m in machines if m.position), None)
                    log("moving: %d of %d placed bots moved; host tick %s; %s" % (
                        moved, placed, max((m.host_time for m in machines), default=None),
                        "%s (player %d) at (%.1f %.1f %.1f), %d inputs sent" % (
                            sample.name, sample.player_index, *sample.position, sample.inputs_sent)
                        if sample else "no bot placed yet"))
            if options.seconds and now - started >= options.seconds:
                break
            if all(m.state in ("closed", "rejected") for m in machines):
                log("every machine has left")
                break
    except KeyboardInterrupt:
        pass
    for machine in machines:
        if machine.tcp:
            machine.tcp.close()
        machine.udp.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
