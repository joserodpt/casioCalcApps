# casioPrizmApps — build every add-in into dist/
#   make                 build all apps
#   make app APP=base-n  build one app
#   make send APP=base-n build + send one app to a connected calculator
#   make new APP=my-app [NAME=My-App]  scaffold a new app
#   make clean

export PATH := $(HOME)/.local/bin:$(PATH)

all:
	@scripts/build.sh

app:
	@scripts/build.sh $(APP)

send: app
	cd apps/$(APP) && fxsdk send-cg

new:
	@scripts/new-app.sh $(APP) $(NAME)

clean:
	rm -rf apps/*/build-cg apps/*/build-fx apps/*/*.g3a apps/*/*.g1a dist/*.g3a

.PHONY: all app send new clean
