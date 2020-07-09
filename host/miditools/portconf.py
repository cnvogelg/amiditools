import logging
import rtmidi
import rtmidi.midiutil


class MidiPortPairArray:
    """manage an array of input/output midi ports.

    Either use existing midi ports or create virtual ports if requested.
    """

    def __init__(self):
        self.ports = []

    def get_num_ports(self):
        return len(self.ports)

    def get_port_pair(self, num):
        if num < len(self.ports):
            return self.ports[num]

    def get_midi_in(self, num):
        if num < len(self.ports):
            return self.ports[num][0]

    def get_midi_out(self, num):
        if num < len(self.ports):
            return self.ports[num][1]

    def get_midi_in_ports(self):
        """Return [(port_num, midi_in), ...]."""
        result = []
        num = 0
        for pair in self.ports:
            if pair[0]:
                result.append((num, pair[0]))
            num += 1
        return result

    def get_midi_out_ports(self):
        """Return [(port_num, midi_out), ...]."""
        result = []
        num = 0
        for pair in self.ports:
            if pair[1]:
                result.append((num, pair[1]))
            num += 1
        return result

    def get_port_pairs(self):
        return self.ports

    def add_port_pair(self, midi_in_name, midi_out_name, virtual=False):
        index = len(self.ports)
        # open midi in
        if midi_in_name:
            try:
                midi_in, name = rtmidi.midiutil.open_midiinput(
                    midi_in_name,
                    interactive=False,
                    use_virtual=virtual)
                logging.info("#%d: midi in:  %s -> %s (virtual=%s)", index,
                             midi_in_name, name, virtual)
            except rtmidi.InvalidPortError:
                logging.error("Invalid midi in port: %s", midi_in_name)
                return None
        else:
            midi_in = None
        # open midi out
        if midi_out_name:
            try:
                midi_out, name = rtmidi.midiutil.open_midioutput(
                    midi_out_name,
                    interactive=False,
                    use_virtual=virtual)
                logging.info("#%d: midi out: %s -> %s (virtual=%s)", index,
                             midi_out_name, name, virtual)
            except rtmidi.InvalidPortError:
                logging.error("Invalid midi out port: %s", midi_out_name)
                return None
        else:
            midi_out = None
        # create pair
        pair = (midi_in, midi_out)
        self.ports.append(pair)
        return pair

    def parse_port_pair_from_dict(self, obj):
        """Parse port pair from a JSON like dict.

            {
                "midi_in" : "foo",
                "midi_out" : "bar",
                "virtual" : false
            }
        """
        if not type(obj) is dict:
            logging.error("Error parsing port pair: no dict object: %r", obj)
            return None
        # get midi_in
        if "midi_in" in obj:
            midi_in_name = obj["midi_in"]
        else:
            midi_in_name = None
            logging.warn("No midi_in found in: %r", obj)
        # get midi_out
        if "midi_out" in obj:
            midi_out_name = obj["midi_out"]
        else:
            midi_out_name = None
            logging.warn("No midi_out found in: %r", obj)
        # get virtual flag
        if "virtual" in obj:
            virtual = obj["virtual"]
        else:
            virtual = False
        # add port pair
        return self.add_port_pair(midi_in_name, midi_out_name, virtual)

    def parse_port_pairs_from_array(self, arr):
        """Add a set of port pairs from an array"""
        if not type(arr) in (list, tuple):
            logging.error("Error parsing port pairs: no array: %r", arr)
            return None
        result = []
        for obj in arr:
            pair = self.parse_port_pair_from_dict(obj)
            if not pair:
                return None
            result.append(pair)
        return result

    def parse_port_pair_from_str(self, string):
        """parse port pair from a string.

        Syntax:  midi_in:midi_out+

        Add plus at end to denote virtual ports
        """
        if not string or len(string) == 0:
            logging.error("invalid ports string: %s", string)
            return None
        # check for virtual flag
        virtual = False
        if string[-1] == '+':
            virtual = True
            string = string[:-1]
        # expect colon
        pos = string.find(':')
        if pos == -1:
            logging.error("invalid ports string: no colon: %s", string)
            return None
        midi_in_name = string[:pos]
        midi_out_name = string[pos+1:]
        return self.add_port_pair(midi_in_name, midi_out_name, virtual)
