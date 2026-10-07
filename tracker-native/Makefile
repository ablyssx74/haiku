# Builds libtracker.so (and the stock Tracker app) natively on Haiku, without the full OS build.
#   make            -> out/libtracker.so
#   make install    -> /boot/system/non-packaged/{Tracker,lib/libtracker.so}
#   make install-launch -> start that Tracker at boot (user launch file)
#   make uninstall
# Needs the haiku_devel package (headers, libshared.a, libcolumnlistview.a, liblocalestub.a).
TOP    := ..
OUT    := out
KIT    := $(TOP)/src/kits/tracker
SC     := $(TOP)/src/add-ons/input_server/filters/shortcut_catcher
INFO   := $(KIT)/infowindow
DEV    := /boot/system/develop

TRACKER_SRCS := $(filter-out $(KIT)/FilePanel.cpp,$(wildcard $(KIT)/*.cpp))
# libtracker.so's own list (see src/kits/tracker/Jamfile) -- the wildcard above is trimmed to it below.
LISTED := SnakeSelector SnakeMenuItem AttributeStream AutoMounterSettings BackgroundImage Bitmaps ContainerWindow CountView DeskWindow \
  DesktopPoseView DraggableContainerIcon DialogPane DirMenu EntryIterator FBCPadding FSClipboard FSUndoRedo \
  FSUtils FavoritesMenu FilePanel FilePanelPriv FindPanel GroupedMenu IconCache IconMenuItem LiveMenu \
  MimeTypeList MiniMenuField Model MountMenu Navigator NavMenu NodePreloader NodeWalker OpenWithWindow \
  OverrideAlert PendingNodeMonitorCache Pose PoseList PoseView PoseViewScripting QueryContainerWindow \
  QueryPoseView RecentItems RegExp SelectionWindow Settings SettingsViews Shortcuts SlowMenu StatusWindow \
  TaskLoop TemplatesMenu Tests TextWidget Thumbnails TitleView Tracker TrackerInitialState TrackerScripting \
  TrackerSettings TrackerSettingsWindow TrackerString TrashWatcher Utilities ViewState \
  VirtualDirectoryEntryList VirtualDirectoryManager VirtualDirectoryPoseView VirtualDirectoryWindow \
  WidgetAttributeText
INFO_LISTED := AttributesView FilePermissionsView GeneralInfoView HeaderView InfoWindow

SRCS := $(addprefix $(KIT)/,$(addsuffix .cpp,$(LISTED))) $(addprefix $(INFO)/,$(addsuffix .cpp,$(INFO_LISTED)))
OBJS := $(patsubst $(TOP)/%.cpp,$(OUT)/%.o,$(SRCS))
SCSRCS := BitFieldTesters CommandActuators KeyInfos ParseCommandLine
SCOBJS := $(addprefix $(OUT)/sc/,$(addsuffix .o,$(SCSRCS)))

HDR := -I$(TOP)/headers/private/tracker -I$(KIT) -I$(INFO) -I$(SC) \
  -I$(DEV)/headers/private/interface -I$(DEV)/headers/private/mount -I$(DEV)/headers/private/shared \
  -I$(DEV)/headers/private/storage -I$(DEV)/headers/private/support -I$(DEV)/headers/private/tracker \
  -I$(DEV)/headers/private/system -I$(DEV)/headers/private/system/arch/x86_64 -I$(DEV)/headers/private
CXXFLAGS := -MMD -MP -O2 -fPIC -Wno-multichar -Wno-deprecated-declarations -D_BUILDING_tracker=1 -D_SUPPORTS_RESOURCES=1 $(HDR)
LIBS := -lbe -llocalestub -ltranslation -lshared -lcolumnlistview -lsupc++ -lstdc++

all: $(OUT)/libtracker.so

$(OUT)/%.o: $(TOP)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@
$(OUT)/sc/%.o: $(SC)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUT)/libtracker.rsrc: $(KIT)/libtracker.rdef $(KIT)/TrackerIcons.rdef
	@mkdir -p $(OUT)
	cpp -P -DHAIKU_OFFICIAL_RELEASE=1 $(KIT)/TrackerIcons.rdef > $(OUT)/TrackerIcons.pp.rdef
	cpp -P $(KIT)/libtracker.rdef > $(OUT)/libtracker.pp.rdef
	rc -o $@ $(OUT)/TrackerIcons.pp.rdef $(OUT)/libtracker.pp.rdef

$(OUT)/libtracker.so: $(OBJS) $(SCOBJS) $(OUT)/libtracker.rsrc
	$(CXX) -shared -Xlinker -soname=libtracker.so -o $@ $(OBJS) $(SCOBJS) $(LIBS) -L$(DEV)/lib
	xres -o $@ $(OUT)/libtracker.rsrc
	setversion $@ -system 1 -short "custom" 2>/dev/null || true

# The Tracker application itself is stock (src/apps/tracker); it is built and installed next to the
# library because an application loads "%A/lib" -- the lib folder next to it -- before anything else,
# so /boot/system/non-packaged/Tracker picks up /boot/system/non-packaged/lib/libtracker.so, while the
# packaged /boot/system/Tracker always loads the packaged library.
APP := $(TOP)/src/apps/tracker
$(OUT)/Tracker.rsrc: $(APP)/Tracker.rdef
	@mkdir -p $(OUT)
	cpp -P $(APP)/Tracker.rdef > $(OUT)/Tracker.pp.rdef
	rc -o $@ $(OUT)/Tracker.pp.rdef

$(OUT)/Tracker: $(APP)/main.cpp $(OUT)/Tracker.rsrc $(OUT)/libtracker.so
	$(CXX) $(CXXFLAGS) -o $@ $(APP)/main.cpp -L$(OUT) -ltracker -lbe -llocalestub -L$(DEV)/lib
	xres -o $@ $(OUT)/Tracker.rsrc
	mimeset -f $@ >/dev/null 2>&1 || true

# Deskbar (src/apps/deskbar): its Be menu and the Applications/Recents menus use Tracker's menus, so it
# is built here too and installed next to the library for the same reason as Tracker.
DESK := $(TOP)/src/apps/deskbar
DESK_SRCS := BarApp BarMenuBar BarMenuTitle BarView BarWindow DeskbarMenu DeskbarUtils ExpandoMenuBar \
  InlineScrollView PreferencesWindow ScreenCornerSelector ShowHideMenuItem StatusView StatusViewShelf \
  TeamMenu TeamMenuItem TimeView TruncatableMenuItem WindowMenu WindowMenuItem ResourceSet Switcher \
  CalendarMenuWindow
DESK_OBJS := $(addprefix $(OUT)/deskbar/,$(addsuffix .o,$(DESK_SRCS)))
DESK_FLAGS := -DDB_ADDONS -D_INCLUDES_CLASS_DEVICE_MAP=1 -D_SUPPORTS_RESOURCES=1 \
  -D_SUPPORTS_FEATURE_SCRIPTING=1 -D_SHOW_CALENDAR_MENU_WINDOW=1 -DMOUNT_MENU_IN_DESKBAR=1 \
  -I$(DEV)/headers/private/app -I$(DEV)/headers/private/locale

$(OUT)/deskbar/%.o: $(DESK)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DESK_FLAGS) -c $< -o $@

$(OUT)/Deskbar.rsrc: $(DESK)/Deskbar.rdef $(DESK)/icons.rdef
	@mkdir -p $(OUT)
	cpp -P $(DESK)/Deskbar.rdef > $(OUT)/Deskbar.pp.rdef
	cpp -P $(DESK)/icons.rdef > $(OUT)/icons.pp.rdef
	rc -o $@ $(OUT)/Deskbar.pp.rdef $(OUT)/icons.pp.rdef

$(OUT)/Deskbar: $(DESK_OBJS) $(OUT)/Deskbar.rsrc $(OUT)/libtracker.so
	$(CXX) -o $@ $(DESK_OBJS) -L$(OUT) -ltracker -lbe -llocalestub -lshared -lsupc++ -lstdc++ -L$(DEV)/lib
	xres -o $@ $(OUT)/Deskbar.rsrc
	mimeset -f $@ >/dev/null 2>&1 || true

app: $(OUT)/Tracker $(OUT)/Deskbar

install: all app
	mkdir -p /boot/system/non-packaged/lib
	# remove first: overwriting a library a running Tracker has mapped crashes that Tracker
	rm -f /boot/system/non-packaged/lib/libtracker.so /boot/system/non-packaged/Tracker /boot/system/non-packaged/Deskbar
	cp $(OUT)/libtracker.so /boot/system/non-packaged/lib/libtracker.so
	cp $(OUT)/Tracker /boot/system/non-packaged/Tracker
	cp $(OUT)/Deskbar /boot/system/non-packaged/Deskbar

# The control look add-on (src/add-ons/control_look/SnakeControlLook) gives every program's menus the
# snake-trail look, not just Tracker's and Deskbar's. Select it in Appearance, or `make activate-look`.
CL := $(TOP)/src/add-ons/control_look/SnakeControlLook
CL_DIR := /boot/system/non-packaged/add-ons/control_look
$(OUT)/SnakeControlLook: $(CL)/SnakeControlLook.cpp
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) -I$(DEV)/headers/private/interface -shared -Xlinker -soname=SnakeControlLook \
		-o $@ $< -lbe -lsupc++ -lstdc++ -L$(DEV)/lib

look: $(OUT)/SnakeControlLook
install-look: look
	mkdir -p $(CL_DIR)
	rm -f $(CL_DIR)/SnakeControlLook
	cp $(OUT)/SnakeControlLook $(CL_DIR)/SnakeControlLook
	mimeset -f $(CL_DIR)/SnakeControlLook >/dev/null 2>&1 || true
# Programs read the control look when they start; restart Tracker/Deskbar (or log out and in).
activate-look: install-look
	$(CXX) -o $(OUT)/setlook setlook.cpp -I$(DEV)/headers/private/interface -lbe
	$(OUT)/setlook $(CL_DIR)/SnakeControlLook
deactivate-look:
	$(CXX) -o $(OUT)/setlook setlook.cpp -I$(DEV)/headers/private/interface -lbe
	$(OUT)/setlook ""

# Makes the launch daemon start /boot/system/non-packaged/{Tracker,Deskbar} instead of the packaged ones
# (a user launch file that amends the two services). Takes effect the next time Tracker is started.
install-launch:
	mkdir -p /boot/home/config/settings/launch
	printf 'service x-vnd.Be-TRAK {\n\tlaunch /system/non-packaged/Tracker\n}\nservice x-vnd.Be-TSKB {\n\tlaunch /system/non-packaged/Deskbar\n}\n' > /boot/home/config/settings/launch/custom-tracker
uninstall-launch:
	rm -f /boot/home/config/settings/launch/custom-tracker

uninstall: uninstall-launch
	rm -f /boot/system/non-packaged/lib/libtracker.so /boot/system/non-packaged/Tracker /boot/system/non-packaged/Deskbar

# A package for people who want to try the look without a source build:
#   make package    -> out/snaketracker-$(PKG_VERSION)-1-x86_64.hpkg
# It installs next to (not over) the system's Tracker and Deskbar:
#   /boot/system/apps/SnakeTracker/{Tracker,Deskbar,lib/libtracker.so}
#   /boot/system/add-ons/control_look/SnakeControlLook
#   /boot/system/data/user_launch/x-snake-tracker   (makes the launch daemon start the two above)
# It has to be installed for the system (not "home"): the launch file points at /system/apps, and the
# launch daemon only reads /boot/system/data/user_launch, so a home install is silently ignored.
# Removing the package puts the stock Tracker and Deskbar back. A package can't replace files that
# another package (haiku.hpkg) owns, hence the separate folder and the launch file.
PKG_VERSION := 1.0.7
PKG := $(OUT)/package
package: all app look
	rm -rf $(PKG)
	mkdir -p $(PKG)/apps/SnakeTracker/lib $(PKG)/add-ons/control_look $(PKG)/data/user_launch \
		$(PKG)/documentation/packages/snaketracker
	cp $(OUT)/Tracker $(OUT)/Deskbar $(PKG)/apps/SnakeTracker/
	cp package/handoff $(PKG)/apps/SnakeTracker/handoff
	chmod +x $(PKG)/apps/SnakeTracker/handoff
	cp $(OUT)/libtracker.so $(PKG)/apps/SnakeTracker/lib/
	cp $(OUT)/SnakeControlLook $(PKG)/add-ons/control_look/
	cp package/x-snake-tracker $(PKG)/data/user_launch/
	cp package/README.md $(PKG)/documentation/packages/snaketracker/
	sed 's/@VERSION@/$(PKG_VERSION)/' package/PackageInfo.in > $(PKG)/.PackageInfo
	rm -f $(OUT)/snaketracker-$(PKG_VERSION)-1-x86_64.hpkg
	package create -C $(PKG) $(OUT)/snaketracker-$(PKG_VERSION)-1-x86_64.hpkg

clean:
	rm -rf $(OUT)
.PHONY: package look install-look activate-look deactivate-look all app install install-launch uninstall-launch uninstall clean

-include $(shell find $(OUT) -name "*.d" 2>/dev/null)
