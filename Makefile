all: dist-clean dist

.PHONY: dist

dist:
	@mkdir -p $@
	$(MAKE) -C amiga dist
	$(MAKE) -C host dist
	$(MAKE) -C amiga/contrib/barsnpipes dist
	@cp amiga/dist/*.lha $@/
	@cp host/dist/* $@/
	@cp amiga/contrib/barsnpipes/dist/*.lha $@/

dist-clean:
	$(MAKE) -C amiga dist-clean
	$(MAKE) -C host dist-clean
	$(MAKE) -C amiga/contrib/barsnpipes dist-clean
	@rm -rf dist
