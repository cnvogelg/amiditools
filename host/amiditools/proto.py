"""updmidi allows to send midi messages via UDP packets."""


import struct
import socket
import time
import random
import logging


class DecoderError(Exception):
    """Decoding a Proto Packet caused trouble"""
    pass


class ProtocolError(Exception):
    """Protocol failed"""
    pass


class Packet:

    MAGIC = 0x43414d00
    MAGIC_MASK = 0xffffff00
    CMD_INV = 0x49
    CMD_INV_OK = 0x4f
    CMD_INV_NO = 0x4e
    CMD_EXIT = 0x45
    CMD_MIDI_MSG = 0x4d
    CMD_MIDI_SYSEX = 0x53
    CMD_CLOCK = 0x43

    """The Packet is stored in an UDP frame."""
    def __init__(self, cmd, seq_num=0, port=0, data=None, ts=None):
        self.cmd = cmd
        self.seq_num = seq_num
        self.port = port
        self.data = data
        if ts:
            self.ts = ts
        else:
            t = time.time()
            sec = int(t)
            micro = int(t * 1000000) % 1000000
            self.ts = (sec, micro)

    def __repr__(self):
        return "ProtoPacket(cmd={}, seq_num={}, port={}, data={}, ts={})" \
            .format(self.cmd, self.seq_num, self.port, self.data, self.ts)

    def get_cmd(self):
        return self.cmd

    def get_seq_num(self):
        return self.seq_num

    def get_port_num(self):
        return self.port

    def get_data(self):
        return self.data

    def get_timestamp(self):
        return self.ts

    @classmethod
    def decode(cls, data):
        n = len(data)
        if n < 24:
            raise DecoderError("MidiPacket too small: {}".format(n))

        # decode packet
        magic, port, seq_num, ts_sec, ts_micro, data_size = \
            struct.unpack(">IIIIII", data[:24])

        # check magic
        if (magic & cls.MAGIC_MASK) != cls.MAGIC:
            raise DecoderError("Invalid Magic: got={!x}".format(magic))
        cmd = magic & 0xff

        # check size
        if data_size != n - 24:
            raise DecoderError("Invalid Size: got={!x}".format(data_size))

        # extract data
        if n > 24:
            pkt_data = data[24:]
        else:
            pkt_data = None

        return cls(cmd, seq_num, port, pkt_data, (ts_sec, ts_micro))

    def encode(self):
        if self.data:
            data_size = len(self.data)
        else:
            data_size = 0
        if self.ts:
            ts0 = self.ts[0]
            ts1 = self.ts[1]
        else:
            ts0 = 0
            ts1 = 0
        raw_pkt = struct.pack(">IIIIII",
                              self.MAGIC | self.cmd, self.port, self.seq_num,
                              ts0, ts1, data_size)
        if self.data:
            raw_pkt += self.data
        return raw_pkt


class Client:
    def __init__(self, host_addr=None, peer_addr=None, max_pkt_size=1024,
                 default_host_port=0, default_peer_port=0):
        if not host_addr:
            host_addr = ('localhost', default_host_port)
        if not peer_addr:
            peer_addr = ('localhost', default_peer_port)
        host_addr = (socket.gethostbyname(host_addr[0]), host_addr[1])
        peer_addr = (socket.gethostbyname(peer_addr[0]), peer_addr[1])
        self.host_addr = host_addr
        self.peer_addr = peer_addr
        self.max_pkt_size = max_pkt_size
        self.sock = socket.socket(family=socket.AF_INET,
                                  type=socket.SOCK_DGRAM)
        self.sock.bind(host_addr)
        # state
        self.connected = False
        self.tx_seq_num = 0
        self.rx_seq_num = 0
        self.lost_packets = 0

    def get_num_lost_packets(self):
        return self.lost_packets

    def get_host_addr(self):
        return self.host_addr

    def get_peer_addr(self):
        return self.peer_addr

    @staticmethod
    def parse_addr_str(host_str, default_port):
        """parse a string in hostname[:port] notation

        Return (host_name, port)
        """
        pos = host_str.find(':')
        if pos == -1:
            return (host_str, default_port)
        host_name = host_str[:pos]
        port = int(host_str[pos+1:])
        return (host_name, port)

    def __repr__(self):
        return "Client(host_addr={}, max_pkt_size={})".format(
            self.host_addr, self.max_pkt_size
        )

    def connect(self, timeout=5, default_port=0):
        """talk invite protocol to connect to server."""
        if self.connected:
            raise RuntimeError("already connected!")

        # send invitation packet
        self.tx_seq_num = random.randint(0, 0xffffffff)
        inv_pkt = Packet(Packet.CMD_INV, seq_num=self.tx_seq_num)
        logging.debug("send inv packet")
        self.sock.sendto(inv_pkt.encode(), self.peer_addr)

        # wait for reply
        self.sock.settimeout(timeout)
        try:
            logging.debug("wait for reply")
            data, got_addr = self.sock.recvfrom(self.max_pkt_size)
            if got_addr != self.peer_addr:
                raise ProtocolError("Wrong peer in invitation!")
            ret_pkt = Packet.decode(data)
        except socket.timeout:
            raise ProtocolError("No invitation reply received!")

        # check reply packet
        cmd = ret_pkt.get_cmd()
        logging.debug("got reply: %02x", cmd)
        if cmd == Packet.CMD_INV_NO:
            raise ProtocolError("Invitation rejected!")
        elif cmd != Packet.CMD_INV_OK:
            raise ProtocolError("Unexpected packet in connect!")

        # connected
        self.connected = True
        self.rx_seq_num = ret_pkt.get_seq_num()
        self.last_rx_ts = time.monotonic()
        self.last_clock_ts = self.last_rx_ts

    def disconnect(self):
        if not self.connected:
            raise RuntimeError("not connected!")

        # send exit packet
        exit_pkt = Packet(Packet.CMD_EXIT)
        self._send_pkt(exit_pkt)

        self.connected = False

    def _send_clock(self, now):
        pkt = Packet(Packet.CMD_CLOCK)
        self._send_pkt(pkt)
        logging.debug("send clock: %r", pkt.get_timestamp())

    def _recv_pkt(self, send_clock_interval, idle_time):
        # choose a suitable timeout
        timeout = send_clock_interval / 5
        self.sock.settimeout(timeout)

        # loop until idle time reached
        last_rx_ts = self.last_rx_ts
        last_clock_ts = self.last_clock_ts
        while True:
            now = time.monotonic()
            delta = now - last_rx_ts
            # idle time reached - abort
            if delta >= idle_time:
                raise ProtocolError("server is idle for %d sec" % idle_time)

            # send clock?
            delta = now - last_clock_ts
            if delta >= send_clock_interval:
                self._send_clock(now)
                last_clock_ts = now

            try:
                data, addr = self.sock.recvfrom(self.max_pkt_size)
                self.last_rx_ts = time.monotonic()
                self.last_clock_ts = last_clock_ts
                return data, addr
            except socket.timeout:
                pass

    def recv(self, send_clock_interval=2, idle_time=5):
        """receive next data packet and return port_num, data, sysex"""
        if not self.connected:
            raise RuntimeError("not connected!")

        while True:
            # receive packet and handle clock
            data, addr = self._recv_pkt(send_clock_interval, idle_time)

            # check peer addr
            if addr != self.peer_addr:
                raise ProtocolError("wrong peer: " + addr)

            # check seq num
            pkt = Packet.decode(data)
            self.rx_seq_num += 1
            pkt_seq_num = pkt.get_seq_num()
            my_seq_num = self.rx_seq_num
            if pkt_seq_num < my_seq_num:
                raise ProtocolError("invalid seq_num: " + pkt_seq_num)
            elif pkt_seq_num > my_seq_num:
                # packets lost!
                num_lost = pkt_seq_num - my_seq_num
                self.rx_seq_num = pkt_seq_num
                self.lost_packets += num_lost

            # check cmd
            cmd = pkt.get_cmd()
            if cmd == Packet.CMD_MIDI_MSG:
                # return midi msg
                return pkt.get_port_num(), pkt.get_data(), False
            elif cmd == Packet.CMD_MIDI_SYSEX:
                # return midi sysex
                return pkt.get_port_num(), pkt.get_data(), True
            elif cmd == Packet.CMD_CLOCK:
                logging.debug("peer clock: %r", pkt.get_timestamp())
            else:
                raise ProtocolError("no data: " + pkt)

    def _send_pkt(self, pkt):
        """send a ProtoPacket to client at addr"""
        if not self.connected:
            raise RuntimeError("not connected!")

        # inject seq num
        self.tx_seq_num += 1
        pkt.seq_num = self.tx_seq_num

        # encode and send packet
        data = pkt.encode()
        self.sock.sendto(data, self.peer_addr)
        return pkt

    def send_msg(self, port_num, data):
        pkt = Packet(Packet.CMD_MIDI_MSG, port=port_num, data=data)
        return self._send_pkt(pkt)

    def send_sysex(self, port_num, data):
        pkt = Packet(Packet.CMD_MIDI_SYSEX, port=port_num, data=data)
        return self._send_pkt(pkt)

    def close(self):
        if self.connected:
            self.disconnect()
        self.sock.close()

    def __enter__(self):
        return self

    def __exit__(self, typ, val, tb):
        self.close()
