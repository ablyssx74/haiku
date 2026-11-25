# Builds the xfwm4 theme decorator style (DecorStyle.h, RenkuOS/Source#626) on a running RenkuOS:
#   make                       -> out/XfwmStyle-base
#   make install THEME=b6      -> ~/config/non-packaged/add-ons/decorators/XfwmStyle-b6
# The style needs the theme's folder (xfwm4-themes/<theme>) where XfwmTheme looks for it, like the XfwmDecorator add-on.
# It links with --no-undefined: a symbol it lacks would otherwise bind to app_server's private code when it loads.
OUT      := out
THEME    ?= b6
ADDONS   := $(HOME)/config/non-packaged/add-ons/decorators
CXX      ?= g++
# Nothing but instantiate_decor_style is exported (-fvisibility=hidden), and the stand-in bitmap classes have names of their own:
# app_server exports a ServerBitmap and a UtilityBitmap, and a style's calls to its own copies of them were bound to
# app_server's (the first definition the loader finds), which crashed it.
CXXFLAGS := -O2 -fPIC -Wall -Wno-multichar -fvisibility=hidden -fvisibility-inlines-hidden \
	-DServerBitmap=XfwmStyleBitmap -DUtilityBitmap=XfwmStyleUtilityBitmap -I. -I../XfwmDecorator

all: $(OUT)/XfwmStyle-base

$(OUT)/XfwmStyle.o: XfwmStyle.cpp DecorStyle.h ServerBitmap.h ../XfwmDecorator/XfwmTheme.h
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) -c XfwmStyle.cpp -o $@

$(OUT)/XfwmTheme.o: ../XfwmDecorator/XfwmTheme.cpp ../XfwmDecorator/XfwmTheme.h ServerBitmap.h
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) -c ../XfwmDecorator/XfwmTheme.cpp -o $@

$(OUT)/XfwmStyle-base: $(OUT)/XfwmStyle.o $(OUT)/XfwmTheme.o
	$(CXX) -shared -Wl,--no-undefined -Wl,-soname=XfwmStyle-decorator -o $@ $^ -lbe -lroot -lstdc++

# one add-on file per theme: the same code with its own name in Appearance
install: all
	mkdir -p $(ADDONS)
	rc -o $(OUT)/res.rsrc resources.rdef
	cp $(OUT)/XfwmStyle-base $(OUT)/XfwmStyle-$(THEME)
	xres -o $(OUT)/XfwmStyle-$(THEME) $(OUT)/res.rsrc
	rm -f $(ADDONS)/XfwmStyle-$(THEME)
	cp $(OUT)/XfwmStyle-$(THEME) $(ADDONS)/XfwmStyle-$(THEME)

clean:
	rm -rf $(OUT)
