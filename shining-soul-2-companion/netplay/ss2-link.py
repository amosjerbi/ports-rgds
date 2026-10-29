#!/usr/bin/env python3
"""Shining Soul II netplay bridge.

Runs next to RetroArch (mGBA core) on each handheld. The patched ROM keeps its link-cable packets in a small block of
EWRAM; this program copies them between the two consoles over wifi using RetroArch's local UDP command port
(READ_CORE_MEMORY / WRITE_CORE_MEMORY), so the two games talk as if they were on a link cable.

  ss2-link.py --peer 192.168.0.101 [--role auto|host|join] [--ra-port 55355] [--port 55501] [--log FILE]

Needs `network_cmd_enable = "true"` in retroarch.cfg. It only touches game memory once the patched ROM's marker is seen.
"""
import argparse, select, socket, struct, sys, time

NET = 0x0203FE00          # network block planted by the patch
ROLE = 0x0203FFD8         # byte: (player id << 4), read by the game where it would read SIOCNT
MAGIC = 0x4E325353        # 'SS2N', written into NET+0 by the patched send hook every frame
SLOTS = 8                 # inbox ring (NET+0x24 + 24*slot)
PKT = 24

def now():
    return time.monotonic()

class Bridge:
    def __init__(self, a):
        self.a = a
        self.ra = ('127.0.0.1', a.ra_port)
        self.sra = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sra.setblocking(False)
        self.sp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sp.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sp.bind(('0.0.0.0', a.port))
        self.sp.setblocking(False)
        self.peer = (a.peer, a.peer_port or a.port)
        self.role = self.pick_role(a.role)
        self.log_f = open(a.log, 'a', buffering=1) if a.log else None
        self.last_out = None          # last out_seq forwarded to the peer
        self.last_in = None           # newest peer seq accepted
        self.tail = 0                 # our copy of the inbox tail
        self.tail_t = 0.0             # when we last wrote the tail
        self.mem = None               # last NET snapshot (31 bytes from NET+4)
        self.magic_ok = False
        self.peer_seen = 0.0
        self.peer_up = False
        self.rtt = None
        self.t_read = self.t_hello = self.t_role = self.t_status = 0.0
        self.t_reply = now()
        self.stats = dict(sent=0, recv=0, dropped=0)

    def pick_role(self, r):
        if r in ('host', 'join'):
            return r
        # auto: the console with the lower address is the host (player 1)
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect((self.a.peer, 9))
            mine = s.getsockname()[0]
            s.close()
        except OSError:
            mine = '0.0.0.0'
        key = lambda ip: tuple(int(x) for x in ip.split('.'))
        return 'host' if key(mine) < key(self.a.peer) else 'join'

    def log(self, *m):
        line = '%.3f %s' % (time.time(), ' '.join(str(x) for x in m))
        if self.log_f:
            self.log_f.write(line + '\n')
        if self.a.verbose:
            print(line, flush=True)

    # ---- RetroArch command port --------------------------------------------------------------------------------------
    def ra_read(self, addr, n):
        self.sra.sendto(b'READ_CORE_MEMORY %x %d' % (addr, n), self.ra)

    def ra_write(self, addr, data):
        self.sra.sendto(b'WRITE_CORE_MEMORY %x %s' % (addr, b' '.join(b'%02x' % x for x in data)), self.ra)

    def on_ra(self, msg):
        parts = msg.split()
        if len(parts) < 3 or parts[0] != b'READ_CORE_MEMORY':
            return
        try:
            data = bytes(int(x, 16) for x in parts[2:])
        except ValueError:
            return                      # "-1 no memory map defined" and friends
        self.t_reply = now()
        if int(parts[1], 16) == NET:
            self.on_net(data)

    def on_net(self, d):
        # d = NET+0 .. NET+0x22  (magic u32, out_seq u32, packet[24], pad[4], peer, head, tail)
        if len(d) < 0x23:
            return
        self.magic_ok = struct.unpack_from('<I', d, 0)[0] == MAGIC
        if not self.magic_ok:
            return
        self.mem = d
        seq = struct.unpack_from('<I', d, 4)[0]
        if seq != self.last_out:
            self.last_out = seq
            if self.peer_up:
                self.sp.sendto(b'P' + d[4:8] + d[8:8 + PKT], self.peer)
                self.stats['sent'] += 1
        flag, head, tail = d[0x20], d[0x21], d[0x22]
        if now() - self.tail_t > 0.15:      # nothing of ours is in flight: trust the game's copy
            self.tail = tail
        want = 1 if self.peer_up else 0
        if flag != want:
            self.ra_write(NET + 0x20, bytes([want]))

    # ---- peer ---------------------------------------------------------------------------------------------------------
    def on_peer(self, msg, addr):
        if not msg:
            return
        k = msg[:1]
        if k == b'P' and len(msg) == 1 + 4 + PKT:
            seq = struct.unpack_from('<I', msg, 1)[0]
            if self.last_in is not None and ((seq - self.last_in) & 0xFFFFFFFF) >= 0x80000000:
                self.stats['dropped'] += 1  # older than what we already delivered
                return
            self.last_in = seq
            self.stats['recv'] += 1
            self.peer_seen = now()
            if self.magic_ok:
                t = self.tail
                self.ra_write(NET + 0x24 + PKT * (t % SLOTS), msg[5:])
                self.tail = (t + 1) & 0xFF
                self.tail_t = now()
                self.ra_write(NET + 0x22, bytes([self.tail]))
        elif k == b'H':
            self.peer_seen = now()
            self.sp.sendto(b'R' + msg[1:], addr)        # echo for rtt
        elif k == b'R' and len(msg) >= 9:
            self.peer_seen = now()
            sent = struct.unpack_from('<d', msg, 1)[0]
            self.rtt = (now() - sent) * 1000

    def tick(self):
        t = now()
        if t - self.t_hello > 0.25:
            self.t_hello = t
            self.sp.sendto(b'H' + struct.pack('<d', t), self.peer)
        up = t - self.peer_seen < 2.0
        if up != self.peer_up:
            self.peer_up = up
            self.log('peer', 'up' if up else 'down', 'role', self.role)
        idle = t - self.t_reply > 1.5           # RetroArch not answering: probe slowly
        if t - self.t_read > (0.5 if idle else 0.004):
            self.t_read = t
            self.ra_read(NET, 0x23)
        if self.magic_ok and t - self.t_role > 0.5:
            self.t_role = t
            self.ra_write(ROLE, bytes([0x10 if self.role == 'join' else 0x00]))
        if t - self.t_status > 5.0:
            self.t_status = t
            self.log('status', 'game' if self.magic_ok and not idle else 'waiting', 'peer', self.peer_up,
                     'rtt', None if self.rtt is None else round(self.rtt, 1), self.stats)

    def run(self):
        self.log('start role', self.role, 'peer', self.peer)
        while True:
            r, _, _ = select.select([self.sra, self.sp], [], [], 0.002)
            for s in r:
                try:
                    while True:
                        msg, addr = s.recvfrom(2048)
                        if s is self.sra:
                            self.on_ra(msg)
                        else:
                            self.on_peer(msg, addr)
                except BlockingIOError:
                    pass
                except ConnectionRefusedError:
                    pass
            self.tick()


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--peer', required=True)
    p.add_argument('--role', default='auto', choices=['auto', 'host', 'join'])
    p.add_argument('--ra-port', type=int, default=55355)
    p.add_argument('--port', type=int, default=55501)
    p.add_argument('--peer-port', type=int, default=0, help='peer bridge port (default: same as --port)')
    p.add_argument('--log')
    p.add_argument('-v', '--verbose', action='store_true')
    a = p.parse_args()
    try:
        Bridge(a).run()
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    main()
