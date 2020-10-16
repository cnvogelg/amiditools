"""updmidi allows to send midi messages via UDP packets."""


import struct

import miditools.proto as proto


class InvalidPortError(Exception):
    """An invalid port was used in a Midi packet"""
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
            raise proto.DecoderError("MidiMsg size must be 4 bytes")

        status, data1, data2, size = struct.unpack("BBBB", data)
        if size < 1 or size > 3:
            raise proto.DecoderError("Wrong size in MidiMsg: {}".format(size))
        return cls([status, data1, data2][:size])

    def encode(self):
        """encode message into 4 bytes of raw data"""
        # pad to 3 entries
        msg = (self.msg + [0, 0])[:3]
        msg.append(len(self.msg))
        return struct.pack("BBBB", *msg)


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


class MidiClient:

    DEFAULT_SERVER_PORT = 6820
    DEFAULT_CLIENT_PORT = 6821

    def __init__(self, host_addr=None, max_pkt_size=1024,
                 max_ports=8, peer_addr=None):
        self.max_ports = max_ports
        self.client = proto.Client(host_addr, peer_addr, max_pkt_size,
                                   default_host_port=self.DEFAULT_CLIENT_PORT,
                                   default_peer_port=self.DEFAULT_SERVER_PORT)
        self.port_map = {}

    def get_host_and_peer_addr(self):
        return self.client.get_host_addr(), self.client.get_peer_addr()

    @classmethod
    def parse_from_str(cls, host_str, peer_str,
                       max_ports=8, max_pkt_size=1024):
        host_addr = proto.Client.parse_addr_str(host_str,
                                                cls.DEFAULT_CLIENT_PORT)
        peer_addr = proto.Client.parse_addr_str(peer_str,
                                                cls.DEFAULT_SERVER_PORT)
        return cls(host_addr, max_pkt_size, max_ports, peer_addr)

    def __repr__(self):
        return "MidiServer(client={}, max_ports={}, peer_addr={})" \
            .format(self.client, self.max_ports, self.peer_addr)

    def get_lost_pkts(self):
        return self.client.get_num_lost_packets()

    def get_port_nums(self):
        return sorted(self.port_map.keys())

    def get_port(self, num):
        if num in self.port_map:
            return self.port_map[num]
        else:
            return None

    def setup_port(self, num):
        self.port_map[num] = MidiPort(num, self)

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

    def connect(self):
        self.client.connect()

    def disconnect(self):
        self.client.disconnect()

    def recv(self):
        """recv next midi packet and return port, msg"""
        port_num, data = self.client.recv()
        port = self._ensure_port(port_num)
        return port, MidiMsg.decode(data)

    def send_msg(self, port_num, msg):
        return self.client.send(port_num, msg.encode())

    def send_raw_msg(self, port_num, raw_msg):
        msg = MidiMsg(raw_msg)
        return self.send_msg(port_num, msg)

    def send_sysex(self, port_num, sysex):
        return self.client.send(port_num, sysex)

    def close(self):
        self.client.close()

    def __enter__(self):
        return self

    def __exit__(self, typ, val, tb):
        self.close()
