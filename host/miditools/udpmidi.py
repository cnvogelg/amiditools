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


class MidiPacket:

    MAGIC = 0x43414d44

    """The MidiPacket is stored in an UDP frame."""
    def __init__(self, seq_num=0, port=0, msg=None, sysex=None):
        if not msg:
            msg = MidiMsg()
        self.seq_num = seq_num
        self.port = port
        self.msg = msg
        self.sysex = sysex

    def __repr__(self):
        return "MidiPacket(seq_num={}, port={}, msg={}, sysex={})".format(
            self.seq_num, self.port, self.msg, self.sysex
        )

    def get_seq_num(self):
        return self.seq_num

    def get_port_num(self):
        return self.port

    def get_msg(self):
        return self.msg

    def get_sysex(self):
        return self.sysex

    def is_sysex(self):
        return self.sysex is not None

    @classmethod
    def decode(cls, data):
        n = len(data)
        if n < 16:
            raise DecoderError("MidiPacket too small: {}".format(n))

        # decode packet
        magic, seq_num, port, msg_data = struct.unpack(">III4s", data[:16])
        # check magic
        if magic != cls.MAGIC:
            raise DecoderError("Invalid Magic: got={!x}".format(magic))
        msg = MidiMsg.decode(msg_data)

        # got sysex?
        if n > 16:
            sysex = data[16:]
        else:
            sysex = None

        return cls(seq_num, port, msg, sysex)

    def encode(self):
        msg_data = self.msg.encode()
        data = struct.pack(">III4s", self.MAGIC, self.seq_num, self.port,
                           msg_data)
        if self.sysex:
            data += self.sysex
        return data


class UDPServer:
    def __init__(self, host="localhost", port=6820, max_size=1024):
        self.host = host
        self.port = port
        self.max_size = max_size
        self.sock = socket.socket(family=socket.AF_INET, 
                                  type=socket.SOCK_DGRAM)
        self.sock.bind((host, port))

    def __repr__(self):
        return "UDPServer(host={}, port={}, max_size={})".format(
            self.host, self.port, self.max_size
        )

    def recv(self):
        """receive next MidiPacket and return (pkt, peer_addr)"""
        data, addr = self.sock.recvfrom(self.max_size)
        return MidiPacket.decode(data), addr

    def send(self, pkt, addr):
        """send a MidiPacket to client at addr"""
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
    def __init__(self, host="localhost", port=6820, max_size=1024,
                 max_ports=8, peer_addr=None):
        self.max_ports = max_ports
        self.udp_srv = UDPServer(host, port, max_size)
        self.port_map = {}
        # state
        self.recv_seq_num = 0
        self.send_seq_num = 0
        self.peer_addr = peer_addr
        self.lost_pkts = 0

    def __repr__(self):
        return "MidiServer(udp_srv={}, max_ports={})".format(self.udp_srv,
                                                             self.max_ports)

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
            raise InvalidClientError("On port {}: client is {} but was {}"
                                     .format(self.port_num, addr,
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
        pkt = MidiPacket(self.send_seq_num, port_num, msg)
        self.send(pkt)

    def send_sysex(self, port_num, sysex):
        # midi msg is first three bytes of sysex
        msg_raw = list(map(int, sysex[:3]))
        msg = MidiMsg(msg_raw)
        pkt = MidiPacket(self.send_seq_num, port_num, msg, sysex)
        self.send(pkt)

    def close(self):
        self.udp_srv.close()

    def __enter__(self):
        return self

    def __exit__(self, typ, val, tb):
        self.close()
