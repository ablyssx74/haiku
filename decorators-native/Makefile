# Builds the xfwm4-theme decorator on a running Haiku, without the full OS build.
#   make                       -> out/XfwmDecorator.o stuff; the add-on binary out/Xfwm-base
#   make install THEME=b6      -> /boot/system/non-packaged/add-ons/decorators/Xfwm-b6 (+ the theme's pictures)
#   make install-all           -> every theme in ablyss/xfwm4-themes-4.10.0/themes
#   make activate THEME=b6     -> select it (like Appearance > Decorator)
#   make deactivate            -> back to the default decorator
# The decorator is an app_server add-on: it must be built against the app_server it runs in (same revision).
TOP     := ..
OUT     := out
SRC     := $(TOP)/src/add-ons/decorators/XfwmDecorator
APP     := $(TOP)/src/servers/app
DEV     := /boot/system/develop/headers
THEMES  := $(TOP)/ablyss/xfwm4-themes-4.10.0/themes
ADDONS  := /boot/system/non-packaged/add-ons/decorators
DATA    := /boot/system/non-packaged/data/xfwm4-themes
THEME   ?= b6
# x86_64, or x86_gcc2 (32-bit Haiku: no -MP, and gcc 2 links its own libstdc++)
ARCH     := $(shell getarch)
ARCH_INC := $(if $(findstring x86_64,$(ARCH)),x86_64,x86)
GCC2     := $(findstring gcc2,$(ARCH))
CXXLIBS  := $(if $(GCC2),,-lsupc++ -lstdc++)

# gcc 2 has no -iquote: -I- limits the server's own directories to "..." includes, so that its Window.h
# etc. don't shadow the system's <Window.h>
QUOTE := $(if $(GCC2),-I$(APP) -I$(APP)/stackandtile -I$(APP)/decorator -I$(APP)/drawing -I$(APP)/drawing/Painter -I$(APP)/font -I$(SRC) -I-,-iquote $(APP) -iquote $(APP)/stackandtile -iquote $(APP)/decorator -iquote $(APP)/drawing    -iquote $(APP)/drawing/Painter -iquote $(APP)/font -iquote $(SRC))
INC := $(QUOTE) \
  -I$(DEV)/private/app -I$(DEV)/private/shared -I$(DEV)/private/interface -I$(DEV)/private/graphics \
  -I$(DEV)/private/system -I$(DEV)/private/system/arch/$(ARCH_INC) -I$(DEV)/private -I$(TOP)/headers/libs/agg \
  -I$(DEV)/$(if $(GCC2),x86/)freetype2 -I$(DEV)/private/binary_compatibility
CXXFLAGS := -MMD $(if $(GCC2),,-MP) -O2 -fPIC -Wno-multichar -DDEBUG=0 $(INC)
OBJS := $(OUT)/XfwmDecorator.o $(OUT)/XfwmTheme.o

all: $(OUT)/Xfwm-base

$(OUT)/%.o: $(SRC)/%.cpp
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUT)/Xfwm-base: $(OBJS)
	$(CXX) -shared -Xlinker -soname=Xfwm-decorator -o $@ $(OBJS) /boot/system/servers/app_server -lbe $(CXXLIBS)

# one add-on file per theme: the same code with its own name in Appearance
install: all
	@test -d $(THEMES)/$(THEME) || { echo "no theme $(THEME)"; exit 1; }
	mkdir -p $(ADDONS) $(DATA)/$(THEME)
	cp $(THEMES)/$(THEME)/themerc $(THEMES)/$(THEME)/*.xpm $(DATA)/$(THEME)/
	sed 's/"name"\t\t\t\t= "xfwm4 theme"/"name"\t\t\t\t= "xfwm4: $(THEME)"/' $(SRC)/resources.rdef > $(OUT)/res-$(THEME).rdef
	rc -o $(OUT)/res-$(THEME).rsrc $(OUT)/res-$(THEME).rdef
	cp $(OUT)/Xfwm-base $(OUT)/Xfwm-$(THEME)
	xres -o $(OUT)/Xfwm-$(THEME) $(OUT)/res-$(THEME).rsrc
	rm -f $(ADDONS)/Xfwm-$(THEME)
	cp $(OUT)/Xfwm-$(THEME) $(ADDONS)/Xfwm-$(THEME)

install-all: all
	for t in $$(ls $(THEMES)); do [ -f $(THEMES)/$$t/themerc ] && $(MAKE) install THEME=$$t; done

$(OUT)/setdecor: setdecor.cpp
	$(CXX) -o $@ setdecor.cpp -I$(DEV)/private/interface -lbe

activate: $(OUT)/setdecor
	$(OUT)/setdecor $(ADDONS)/Xfwm-$(THEME)

deactivate: $(OUT)/setdecor
	$(OUT)/setdecor Default

# A package with the add-on for every theme:
#   make package    -> out/xfwm_decorators-$(PKG_VERSION)-1-$(ARCH).hpkg
# /boot/system/add-ons/decorators/Xfwm-<theme> and /boot/system/data/xfwm4-themes/<theme>/
PKG_VERSION := 1.0.1
PKG := $(OUT)/package
package: all
	rm -rf $(PKG)
	mkdir -p $(PKG)/add-ons/decorators $(PKG)/data/xfwm4-themes $(PKG)/documentation/packages/xfwm_decorators
	for t in $$(ls $(THEMES)); do [ -f $(THEMES)/$$t/themerc ] || continue; \
		mkdir -p $(PKG)/data/xfwm4-themes/$$t; \
		cp $(THEMES)/$$t/themerc $(THEMES)/$$t/*.xpm $(PKG)/data/xfwm4-themes/$$t/; \
		sed 's/"name"\t\t\t\t= "xfwm4 theme"/"name"\t\t\t\t= "xfwm4: '$$t'"/' $(SRC)/resources.rdef > $(OUT)/res-$$t.rdef; \
		rc -o $(OUT)/res-$$t.rsrc $(OUT)/res-$$t.rdef; \
		cp $(OUT)/Xfwm-base $(PKG)/add-ons/decorators/Xfwm-$$t; \
		xres -o $(PKG)/add-ons/decorators/Xfwm-$$t $(OUT)/res-$$t.rsrc; \
	done
	cp $(TOP)/ablyss/xfwm4-themes-4.10.0/AUTHORS $(TOP)/ablyss/xfwm4-themes-4.10.0/COPYING \
		$(PKG)/documentation/packages/xfwm_decorators/
	sed 's/@VERSION@/$(PKG_VERSION)/; s/@ARCH@/$(ARCH)/' package/PackageInfo.in > $(PKG)/.PackageInfo
	rm -f $(OUT)/xfwm_decorators-$(PKG_VERSION)-1-$(ARCH).hpkg
	package create -C $(PKG) $(OUT)/xfwm_decorators-$(PKG_VERSION)-1-$(ARCH).hpkg

clean:
	rm -rf $(OUT)

-include $(wildcard $(OUT)/*.d)


.PHONY: package all install install-all activate deactivate clean
