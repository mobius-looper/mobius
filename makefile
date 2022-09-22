
default:
	@echo src
	@cd src
	@$(MAKE) /nologo
	@cd ..


clean:
	@echo src
	@cd src
	@$(MAKE) /nologo clean
	@cd ..

allclean: 
	@echo src
	@cd src
	@$(MAKE) /nologo allclean
	@cd ..

tag:
	