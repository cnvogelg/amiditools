"""updmidi allows to send midi messages via UDP packets."""


import struct
import socket


class DecoderError(Exception):
    """Decoding a Midi UDP Packet caused trouble"""
    pass


class InvalidPortError(Exception):
    """An invalid port was used in a Midi packet"""
    pass


class InvalidClientError(Exception):
    """Multiple clients seem to use a single port"""
    pass


class MidiMsg:
    """store a midi message as a tuple and allow conversion to UDP format.

       sysex messages are not stored in this class. however two prefix bytes
       of a sysex can be stored.
    """

    def __init__(self, msg_tuple=None):
        if msg_tuple:
            self.msg = msg_tuple
        else:
            self.msg = []

    def __repr__(self):
        return "MidiMsg({})".format(self.msg)

    def get_tuple(self):
        return tuple(self.msg)

    @classmethod
    def decode(cls, data):
        """decode message from 4 bytes raw data"""
        if len(data) != 4:
            raise DecoderError("MidiMsg size must be 4 bytes")

        status, data1, data2, size = struct.unpack("BBBB", data)
        if size < 1 or size > 3:
            raise DecoderError("Wrong size in MidiMsg: {}".format(size))
        return cls([status, data1, data2][:size])

    def encode(self):
        """encode message into 4 bytes of raw data"""
        # pad to 3 entries
        msg = (self.msg + [0, 0])[:3]
        msg.append(len(self.msg))
        return struct.pack("BBBB", *msg)


class ProtoPacket:

    MAGIC = 0x43414d00
    MAGIC_MASK = 0xffffff00
    CMD_INV = 0x49
    CMD_INV_OK = 0x4f
    CMD_INV_NO = 0x4e
    CMD_EXIT = 0x45
    CMD_DATA = 0x44
    CMD_CLOCK = 0x43

    """The ProtoPacket is stored in an UDP frame."""
    def __init__(self, cmd, seq_num=0, port=0, data=None, ts=None):
        self.cmd = cmd
        self.seq_num = seq_num
        self.port = port
        self.data = data
        self.ts = ts

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
            pkt_data = data[:24]
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


class UDPServer:
    def __init__(self, host_addr=None, max_pkt_size=1024, default_port=0):
        if not host_addr:
            host_addr = ('localhost', default_port)
        host_addr = (socket.gethostbyname(host_addr[0]), host_addr[1])
        self.host_addr = host_addr
        self.max_pkt_size = max_pkt_size
        self.sock = socket.socket(family=socket.AF_INET, 
                                  type=socket.SOCK_DGRAM)
        self.sock.bind(host_addr)

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
        return "UDPServer(host_addr={}, max_pkt_size={})".format(
            self.host_addr, self.max_pkt_size
        )

    def recv(self):
        """receive next ProtoPacket and return (pkt, peer_addr)"""
        data, addr = self.sock.recvfrom(self.max_pkt_size)
        return ProtoPacket.decode(data), addr

    def send(self, pkt, addr):
        """send a ProtoPacket to client at addr"""
        data = pkt.encode()
        self.sock.sendto(data, addr)

    def close(self):
        self.sock.close()

    def __enter__(self):
        return self

    def __exit__(self, typ, val, tb):
        self.close()


class MidiPort:
    def __init__(self, port_num, midi_srv, peer_addr=None):
        self.port_num = port_num
        self.midi_srv = midi_srv

    def get_port_num(self):
        return self.port_num

    def get_lost_pkts(self):
        return self.lost_pkts

    def send_msg(self, msg):
        self.midi_srv.send_msg(self.port_num, msg)

    def send_sysex(self, sysex):
        self.midi_srv.send_sysex(self.port_num, sysex)

    def send_pkt(self, pkt):
        assert pkt.get_port_num() == self.port_num
        self.midi_srv.send(pkt)


class MidiServer:

    DEFAULT_SERVER_PORT = 6820
    DEFAULT_CLIENT_PORT = 6821

    def __init__(self, host_addr=None, max_pkt_size=1024,
                 max_ports=8, peer_addr=None):
        self.max_ports = max_ports
        self.udp_srv = UDPServer(host_addr, max_pkt_size,
                                 default_port=self.DEFAULT_CLIENT_PORT)
        self.port_map = {}
        # state
        self.recv_seq_num = 0
        self.send_seq_num = 0
        if peer_addr:
            peer_addr = (socket.gethostbyname(peer_addr[0]), peer_addr[1])
        self.peer_addr = peer_addr
        self.host_addr = self.udp_srv.host_addr
        self.lost_pkts = 0

    def get_host_and_peer_addr(self):
        return self.host_addr, self.peer_addr

    @classmethod
    def parse_from_str(cls, host_str, peer_str,
                       max_ports=8, max_pkt_size=1024):
        host_addr = UDPServer.parse_addr_str(host_str,
                                             cls.DEFAULT_CLIENT_PORT)
        peer_addr = UDPServer.parse_addr_str(peer_str,
                                             cls.DEFAULT_SERVER_PORT)
        return cls(host_addr, max_pkt_size, max_ports, peer_addr)

    def __repr__(self):
        return "MidiServer(udp_srv={}, max_ports={}, peer_addr={})" \
            .format(self.udp_srv, self.max_ports, self.peer_addr)

    def get_peer_addr(self):
        return self.peer_addr

    def get_lost_pkts(self):
        return self.lost_pkts

    def get_port_nums(self):
        return sorted(self.port_map.keys())

    def get_port(self, num):
        if num in self.port_map:
            return self.port_map[num]
        else:
            return None

    def setup_port(self, num):
        self.port_map[num] = MidiPort(num, self)

    def _reset_state(self):
        self.recv_seq_num = 1
        self.send_seq_num = 0
        self.peer_addr = None
        self.lost_pkts = 0

    def _ensure_port(self, num):
        if num >= self.max_ports:
            raise InvalidPortError("Invalid port: {} > {}"
                                   .format(num, self.max_ports))
        # create new port
        if num not in self.port_map:
            port = MidiPort(num, self)
            self.port_map[num] = port
            return port
        else:
            return self.port_map[num]

    def _check_pkt(self, pkt, addr):
        # check seq num
        seq_num = pkt.get_seq_num()
        rsn = self.recv_seq_num
        # reset client
        if seq_num < rsn:
            self._reset_state()
        # lost packets..
        elif seq_num > rsn:
            self.lost_pkts = seq_num - rsn
            self.recv_seq_num = seq_num + 1
        # all ok
        else:
            self.lost_pkts = 0
            self.recv_seq_num += 1
        # check peer
        if self.peer_addr is None:
            self.peer_addr = addr
        elif self.peer_addr != addr:
            raise InvalidClientError("Client is {} but was {}"
                                     .format(addr,
                                             self.peer_addr))

    def recv(self):
        """recv next midi packet and return pkt, port"""
        pkt, addr = self.udp_srv.recv()
        port = self._ensure_port(pkt.get_port_num())
        self._check_pkt(pkt, addr)
        return pkt, port

    def send(self, pkt):
        self.udp_srv.send(pkt, self.peer_addr)

    def send_msg(self, port_num, msg):
        pkt = ProtoPacket(ProtoPacket.CMD_DATA, self.send_seq_num, port_num, msg.encode())
        self.send(pkt)
        return pkt

    def send_raw_msg(self, port_num, raw_msg):
        msg = MidiMsg(raw_msg)
        pkt = ProtoPacket(ProtoPacket.CMD_DATA, self.send_seq_num, port_num, msg.encode())
        self.send(pkt)
        return pkt

    def send_sysex(self, port_num, sysex):
        pkt = ProtoPacket(ProtoPacket.CMD_DATA, self.send_seq_num, port_num, sysex )
        self.send(pkt)
        return pkt

    def close(self):
        self.udp_srv.close()

    def __enter__(self):
        return self

    def __exit__(self, typ, val, tb):
        self.close()
