"""updmidi allows to send midi messages via UDP packets."""


import struct
import socket


class DecoderError(Exception):
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

    def get_port(self):
        return self.port

    def get_msg(self):
        return self.msg

    def get_sysex(self):
        return self.sysex

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
