# Builds libtracker.so (and the stock Tracker app) natively on Haiku, without the full OS build.
#   make            -> out/libtracker.so
#   make install    -> /boot/system/non-packaged/lib/libtracker.so  (overrides the packaged one)
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
LISTED := AttributeStream AutoMounterSettings BackgroundImage Bitmaps ContainerWindow CountView DeskWindow \
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
CXXFLAGS := -O2 -fPIC -Wno-multichar -Wno-deprecated-declarations -D_BUILDING_tracker=1 -D_SUPPORTS_RESOURCES=1 $(HDR)
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

install: all
	mkdir -p /boot/system/non-packaged/lib
	cp $(OUT)/libtracker.so /boot/system/non-packaged/lib/libtracker.so
uninstall:
	rm -f /boot/system/non-packaged/lib/libtracker.so
clean:
	rm -rf $(OUT)
.PHONY: all install uninstall clean
