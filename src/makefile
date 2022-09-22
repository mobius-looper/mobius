
default:
	@echo PortAudio
	@cd PortAudio
	@$(MAKE) /nologo
	@echo SoundTouch
	@cd ../SoundTouch
	@$(MAKE) /nologo
	@echo vst
	@cd ../vst
	@$(MAKE) /nologo
	@echo util
	@cd ..\util
	@$(MAKE) /nologo
	@echo osc
	@cd ..\osc
	@$(MAKE) /nologo
	@echo qwin
	@cd ..\qwin
	@$(MAKE) /nologo
	@echo midi
	@cd ..\midi
	@$(MAKE) /nologo
	@echo audio
	@cd ..\audio
	@$(MAKE) /nologo
	@echo mobius
	@cd ..\mobius
	@$(MAKE) /nologo
	@cd ..

clean:
	@echo util
	@cd util
	@$(MAKE) /nologo clean
	@echo osc
	@cd ..\osc
	@$(MAKE) /nologo clean
	@echo qwin
	@cd ..\qwin
	@$(MAKE) /nologo clean
	@echo midi
	@cd ..\midi
	@$(MAKE) /nologo clean
	@echo audio
	@cd ..\audio
	@$(MAKE) /nologo clean
	@echo mobius
	@cd ..\mobius
	@$(MAKE) /nologo clean
	@cd ..

allclean: clean
	@echo PortAudio
	@cd PortAudio
	@$(MAKE) /nologo clean
	@echo SoundTouch
	@cd ../SoundTouch
	@$(MAKE) /nologo clean
	@echo vst
	@cd ../vst
	@$(MAKE) /nologo clean
	@cd ..
