CAMD_NAMES=udp echo
CAMD_PORTS=0 1 2 3 4 5 6 7

all:
	for name in $(CAMD_NAMES) ; do \
		for port in $(CAMD_PORTS) ; do \
			vamos smake CAMD_NAME=$$name CAMD_PORT=$$port ; \
		done ; \
	done

clean:
	rm -f *.o *.ptool
