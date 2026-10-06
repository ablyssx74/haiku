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

app: $(OUT)/Tracker

install: all app
	mkdir -p /boot/system/non-packaged/lib
	# remove first: overwriting a library a running Tracker has mapped crashes that Tracker
	rm -f /boot/system/non-packaged/lib/libtracker.so /boot/system/non-packaged/Tracker
	cp $(OUT)/libtracker.so /boot/system/non-packaged/lib/libtracker.so
	cp $(OUT)/Tracker /boot/system/non-packaged/Tracker

# Makes the launch daemon start /boot/system/non-packaged/Tracker instead of the packaged one (a user
# launch file that amends the Tracker service). Takes effect the next time Tracker is started.
install-launch:
	mkdir -p /boot/home/config/settings/launch
	printf 'service x-vnd.Be-TRAK {\n\tlaunch /system/non-packaged/Tracker\n}\n' > /boot/home/config/settings/launch/custom-tracker
uninstall-launch:
	rm -f /boot/home/config/settings/launch/custom-tracker

uninstall: uninstall-launch
	rm -f /boot/system/non-packaged/lib/libtracker.so /boot/system/non-packaged/Tracker

clean:
	rm -rf $(OUT)
.PHONY: all app install install-launch uninstall-launch uninstall clean

-include $(shell find $(OUT) -name "*.d" 2>/dev/null)
