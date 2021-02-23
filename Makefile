all: dist-clean dist

.PHONY: dist

dist:
	@mkdir -p $@
	$(MAKE) -C amiga dist
	$(MAKE) -C host dist
	@cp amiga/dist/*.lha $@/
	@cp host/dist/* $@/

dist-clean:
	$(MAKE) -C amiga dist-clean
	$(MAKE) -C host dist-clean
	@rm -rf dist
