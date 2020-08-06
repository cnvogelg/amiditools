import time


class PerfSample:
    """A midi cmd to be sent and a time stamp for transfer and receiption."""
    def __init__(self, midi_cmd, delay=None):
        self.midi_cmd = midi_cmd
        self.delay = delay
        self.tx_ts = None
        self.rx_ts = None
        self.latency = None

    def __repr__(self):
        return "PerfSample(%r, %r)" % (self.midi_cmd, self.delay)

    def send(self, midi_out, ts):
        midi_out.send_message(self.midi_cmd)
        self.tx_ts = ts
        self.rx_ts = None

    def recv(self, midi_cmd, ts):
        if midi_cmd != self.midi_cmd:
            return False
        self.rx_ts = ts
        self.latency = self.rx_ts - self.tx_ts
        return True

    def reset(self):
        self.tx_ts = None
        self.rx_ts = None
        self.latency = None

    def get_latency(self):
        return self.latency


class PerfBurst:
    """Send out a set of midi commands and wait for the receiption."""
    def __init__(self, default_delay=0):
        self.samples = []
        self.default_delay = default_delay
        self._expect_rx_pos = 0
        self._lost_samples = 0

    def _get_timestamp(self):
        # return value in 1ms uni
        return time.perf_counter() * 1000.0

    def add_sample(self, sample):
        self.samples.append(sample)

    def add_sample_list(self, samples):
        self.samples += list(samples)

    def send_samples(self, midi_out):
        self._expect_rx_pos = 0
        self._lost_samples = 0
        for sample in self.samples:
            ts = self._get_timestamp()
            sample.send(midi_out, ts)

    def is_done(self):
        return self._expect_rx_pos == len(self.samples)

    def incoming_message(self, midi_cmd):
        ts = self._get_timestamp()
        pos = self._expect_rx_pos
        n = len(self.samples)
        while pos < n:
            # found sample
            if self.samples[pos].recv(midi_cmd, ts):
                self._expect_rx_pos = pos + 1
                return True
            self._lost_samples += 1
            pos += 1
        self._expect_rx_pos = n
        return False

    def get_num_lost(self):
        return self._lost_samples

    def wait_done(self, time_out=5, time_sleep=0.1):
        start = time.perf_counter()
        delta = 0
        while delta < time_out:
            if self.is_done():
                return True
            time.sleep(time_sleep)
            delta = time.perf_counter() - start
        return False

    def get_latencies(self):
        result = []
        for sample in self.samples:
            latency = sample.get_latency()
            if latency:
                result.append(latency)
        return result

    def reset(self):
        for sample in self.samples:
            sample.reset()


class SampleGenerator:
    @staticmethod
    def note_on_sweep(start=0, end=128, step=1, velocity=42, channel=0):
        result = []
        for note in range(start, end, step):
            cmd = [0x90 | channel, note, velocity]
            result.append(PerfSample(cmd))
        return result

    @staticmethod
    def note_off_sweep(start=0, end=128, step=1, velocity=42, channel=0):
        result = []
        for note in range(start, end, step):
            cmd = [0x80 | channel, note, velocity]
            result.append(PerfSample(cmd))
        return result
