#
# Windows makefile for the VST SDKs
#

default: make2.4
clean: clean2.4

make2.4:
	@echo vst/2.4
	@cd 2.4
	@$(MAKE) /nologo
	@cd ..

clean2.4:
	@echo vst/2.4
	@cd 2.4
	@$(MAKE) /nologo clean
	@cd ..


make2.3:
	@echo vst/2.3
	@cd 2.3
	@$(MAKE) /nologo
	@echo vst/2.3/examples
	@cd examples
	@$(MAKE) /nologo
	@cd ..\..

clean2.3:
	@echo vst/2.3
	@cd 2.3
	@$(MAKE) /nologo clean
	@echo vst/2.3/examples
	@cd examples
	@$(MAKE) /nologo clean
	@cd ..\..

