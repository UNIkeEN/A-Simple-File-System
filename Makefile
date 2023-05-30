.PHONY: all clean

all:
	@$(MAKE) -C step1
	@$(MAKE) -C step2
	@$(MAKE) -C step3

clean:
	@$(MAKE) clean -C step1
	@$(MAKE) clean -C step2
	@$(MAKE) clean -C step3
