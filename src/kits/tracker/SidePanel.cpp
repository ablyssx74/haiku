#include "SidePanel.h"

#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <Application.h>
#include <Bitmap.h>
#include <ControlLook.h>
#include <Directory.h>
#include <fs_info.h>
#include <File.h>
#include <FindDirectory.h>
#include <GroupLayout.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <Node.h>
#include <NodeInfo.h>
#include <NodeMonitor.h>
#include <OutlineListView.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <String.h>
#include <Volume.h>
#include <VolumeRoster.h>
#include <Window.h>

#include <vector>

#include "Commands.h"
#include "FSUtils.h"
#include "ContainerWindow.h"
#include "Model.h"
#include "SnakeMenuItem.h"
#include "SnakeSelector.h"
#include "Tracker.h"
#include "TrackerSettings.h"


namespace BPrivate {

static const uint32 kFollowFolder = 'sdFl';
static const uint32 kNavigateTo = 'sdNv';
static const uint32 kAddFavorite = 'sfAd';
static const uint32 kRemoveFavorite = 'sfRm';
static const uint32 kMoveFavorite = 'sfMv';
static const uint32 kOpenInNewWindow = 'sfNw';
static const uint32 kToggleTrash = 'sfTr';
static const float kIconSize = 16;


// A line of the tree: a folder (a volume, or a folder on one), or the heading of a group. A folder that has not been
// opened yet holds one empty line below it, so that it shows the sign for opening.
class SideItem : public BListItem {
public:
	SideItem(const entry_ref* ref, const char* label, uint32 level, bool heading = false, bool placeholder = false)
		:
		BListItem(level),
		fLabel(label),
		fIcon(NULL),
		fHeading(heading),
		fPlaceholder(placeholder),
		fLoaded(false),
		fFavorite(false),
		fTrash(false),		fFavoriteIndex(-1),
		fHint(false),
		fVolume(-1)
	{
		if (ref != NULL) {
			fRef = *ref;
			fIcon = new BBitmap(BRect(0, 0, kIconSize - 1, kIconSize - 1), B_RGBA32);
			if (BNodeInfo::GetTrackerIcon(ref, fIcon, (icon_size)(int)kIconSize) != B_OK) {
				delete fIcon;
				fIcon = NULL;
			}
		}
		if (heading || placeholder)
			SetEnabled(false);
	}

	virtual ~SideItem()
	{
		delete fIcon;
	}

	virtual void Update(BView* owner, const BFont* font)
	{
		BListItem::Update(owner, font);
		font_height height;
		font->GetHeight(&height);
		SetHeight(std::max(ceilf(height.ascent + height.descent + height.leading) + 6, kIconSize + 6));
	}

	virtual void DrawItem(BView* owner, BRect frame, bool complete)
	{
		owner->PushState();
		const rgb_color background = owner->ViewColor();
		owner->SetHighColor(background);
		owner->FillRect(frame);
		if (fPlaceholder) {
			owner->PopState();
			return;
		}

		rgb_color text = ui_color(B_DOCUMENT_TEXT_COLOR);
		if (IsSelected() && !fHeading) {
			owner->SetLowColor(background);
			text = SnakeSelector::DrawSelectionPill(owner, frame.InsetByCopy(1, 1), owner->Window()->IsActive());
		}

		font_height height;
		owner->GetFontHeight(&height);
		float x = frame.left + 4;
		if (fHint) {
			text = make_color((uint8)(background.red + (text.red - background.red) * 0.45f),
				(uint8)(background.green + (text.green - background.green) * 0.45f),
				(uint8)(background.blue + (text.blue - background.blue) * 0.45f));
		} else if (fHeading) {
			BFont bold(be_bold_font);
			bold.SetSize(be_plain_font->Size() * 0.9f);
			owner->SetFont(&bold);
			bold.GetHeight(&height);
			text = tint_color(text, B_DARKEN_1_TINT);
			text.alpha = 255;
			rgb_color dimmed = make_color((uint8)(background.red + (text.red - background.red) * 0.6f),
				(uint8)(background.green + (text.green - background.green) * 0.6f),
				(uint8)(background.blue + (text.blue - background.blue) * 0.6f));
			text = dimmed;
		} else if (fIcon != NULL) {
			owner->SetDrawingMode(B_OP_ALPHA);
			owner->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
			owner->DrawBitmap(fIcon, BPoint(x, frame.top + floorf((frame.Height() - kIconSize) / 2)));
			x += kIconSize + 6;
		}

		owner->SetDrawingMode(B_OP_OVER);
		owner->SetHighColor(text);
		owner->DrawString(fLabel.String(),
			BPoint(x, frame.top + floorf((frame.Height() + height.ascent - height.descent) / 2) + 1));
		owner->PopState();
	}

	// reads the icon again (the Trash's changes when it is filled or emptied); true if it is not the same now
	bool ReloadIcon()
	{
		BNode node(&fRef);
		BNodeInfo info(&node);
		BBitmap* fresh = new BBitmap(BRect(0, 0, kIconSize - 1, kIconSize - 1), B_RGBA32);
		if (node.InitCheck() != B_OK || info.InitCheck() != B_OK
			|| info.GetTrackerIcon(fresh, (icon_size)(int)kIconSize) != B_OK) {
			delete fresh;
			return false;
		}
		if (fIcon != NULL && fIcon->BitsLength() == fresh->BitsLength()
			&& memcmp(fIcon->Bits(), fresh->Bits(), fresh->BitsLength()) == 0) {
			delete fresh;
			return false;
		}
		delete fIcon;
		fIcon = fresh;
		return true;
	}

	const entry_ref&	Ref() const { return fRef; }
	const char*			Label() const { return fLabel.String(); }
	bool				IsHeading() const { return fHeading; }
	bool				IsPlaceholder() const { return fPlaceholder; }
	bool				IsLoaded() const { return fLoaded; }
	void				SetLoaded() { fLoaded = true; }
	const char*			Path() const { return fPath.String(); }
	void				SetPath(const char* path) { fPath = path; }
	bool				IsTrash() const { return fTrash; }
	void				SetTrash() { fTrash = true; }
	bool				IsFavorite() const { return fFavorite; }
	int32				FavoriteIndex() const { return fFavoriteIndex; }
	void				SetFavoriteIndex(int32 index) { fFavoriteIndex = index; }
	void				SetFavorite() { fFavorite = true; }
	bool				IsHint() const { return fHint; }
	void				SetHint() { fHint = true; SetEnabled(false); }
	dev_t				Volume() const { return fVolume; }
	void				SetVolume(dev_t volume) { fVolume = volume; }

private:
	entry_ref	fRef;
	BString		fLabel;
	BString		fPath;
	BBitmap*	fIcon;
	bool		fHeading;
	bool		fPlaceholder;
	bool		fLoaded;
	bool		fFavorite;
	bool		fTrash;
	int32		fFavoriteIndex;
	bool		fHint;
	dev_t		fVolume;
};


struct SideFolder {
	BString		name;
	entry_ref	ref;
};


static bool
SideFolderBefore(const SideFolder& a, const SideFolder& b)
{
	return strcasecmp(a.name.String(), b.name.String()) < 0;
}


class SideTree : public BOutlineListView {
public:
	SideTree(BHandler* target)
		:
		BOutlineListView("sidetree", B_SINGLE_SELECTION_LIST),
		fTarget(target),
		fProgrammatic(false)
	{
		SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
		SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
		// the selected line is a pill with rounded ends: drawn whole again when the panel changes width, so that
		// no old end is left behind
		SetFlags(Flags() | B_FULL_UPDATE_ON_RESIZE);
	}

	virtual void FrameResized(float width, float height)
	{
		BOutlineListView::FrameResized(width, height);
		Invalidate();
	}

	void Populate()
	{
		// the favourites: the folders kept here, the user's own; every folder opens into its subfolders
		AddItem(new SideItem(NULL, "Favorites", 0, true));
		_LoadFavorites();
		_AddFavoriteItems(1);
		_AddTrash();
		_AddVolumes();
	}

	// the Trash line's icon follows the Trash: full, or empty
	void RefreshTrashIcon()
	{
		for (int32 i = 0; i < FullListCountItems(); i++) {
			SideItem* item = dynamic_cast<SideItem*>(FullListItemAt(i));
			if (item != NULL && item->IsTrash()) {
				if (item->ReloadIcon()) {
					const int32 index = IndexOf(item);
					if (index >= 0)
						InvalidateItem(index);
				}
				break;
			}
		}
	}

	// the volumes again, after one was mounted or unmounted
	void RebuildVolumes()
	{
		std::vector<BListItem*> old;
		for (int32 i = 0; i < FullListCountItems(); i++) {
			SideItem* item = dynamic_cast<SideItem*>(FullListItemAt(i));
			if (item != NULL && item->OutlineLevel() == 0 && item->Volume() >= 0)
				old.push_back(item);
		}
		// (a folder's line also takes out and deletes the lines under it)
		for (size_t i = 0; i < old.size(); i++)
			RemoveItem(old[i]);
		for (size_t i = 0; i < old.size(); i++)
			delete old[i];
		_AddVolumes();
		Invalidate();
	}

	// the lines under a folder are read when it is opened
	virtual void ExpandOrCollapse(BListItem* underItem, bool expand)
	{
		SideItem* item = dynamic_cast<SideItem*>(underItem);
		if (expand && item != NULL && !item->IsLoaded())
			_Load(item);
		BOutlineListView::ExpandOrCollapse(underItem, expand);
	}

	virtual void SelectionChanged()
	{
		BOutlineListView::SelectionChanged();
		if (fProgrammatic)
			return;
		SideItem* item = dynamic_cast<SideItem*>(ItemAt(CurrentSelection()));
		if (item == NULL || item->IsHeading() || item->IsPlaceholder())
			return;
		BMessage message(kNavigateTo);
		message.AddRef("refs", &item->Ref());
		BMessenger(fTarget).SendMessage(&message);
	}

	// selects the folder, opening the folders above it: it is found under the favourite that holds it (the longest
	// one, if more than one does)
	void ShowFolder(const entry_ref& ref)
	{
		BEntry entry(&ref, true);
		BPath path;
		if (entry.GetPath(&path) != B_OK)
			return;
		const BString target(path.Path());

		SideItem* top = NULL;
		int32 topLength = -1;
		for (int32 i = 0; i < FullListCountItems(); i++) {
			SideItem* item = dynamic_cast<SideItem*>(FullListItemAt(i));
			if (item == NULL || item->OutlineLevel() != 0 || !(item->IsFavorite() || item->Volume() >= 0))
				continue;
			const BString favorite(item->Path());
			const bool holds = target == favorite
				|| (target.FindFirst(favorite) == 0
					&& (favorite == "/" || target[favorite.Length()] == '/'));
			if (holds && favorite.Length() > topLength) {
				top = item;
				topLength = favorite.Length();
			}
		}
		if (top == NULL) {
			// not under a favourite or a volume: nothing is selected
			fProgrammatic = true;
			DeselectAll();
			fProgrammatic = false;
			return;
		}
		BString rest(target);
		rest.Remove(0, top->Path()[0] == '/' && strlen(top->Path()) == 1 ? 0 : (int32)strlen(top->Path()));

		SideItem* current = top;
		int32 start = 0;
		while (current != NULL) {
			while (start < rest.Length() && rest[start] == '/')
				start++;
			if (start >= rest.Length())
				break;
			int32 end = rest.FindFirst('/', start);
			if (end < 0)
				end = rest.Length();
			BString name;
			rest.CopyInto(name, start, end - start);
			start = end;

			Expand(current);
			SideItem* next = NULL;
			for (int32 i = FullListIndexOf(current) + 1; i < FullListCountItems(); i++) {
				SideItem* child = dynamic_cast<SideItem*>(FullListItemAt(i));
				if (child == NULL || child->OutlineLevel() <= current->OutlineLevel())
					break;
				if (child->OutlineLevel() == current->OutlineLevel() + 1 && !child->IsPlaceholder()
					&& name == child->Label()) {
					next = child;
					break;
				}
			}
			current = next;
		}
		if (current == NULL)
			return;

		fProgrammatic = true;
		Select(IndexOf(current));
		ScrollToSelection();
		fProgrammatic = false;
	}

	virtual void MouseDown(BPoint where)
	{
		BMessage* current = Window()->CurrentMessage();
		int32 buttons = 0;
		if (current != NULL)
			current->FindInt32("buttons", &buttons);
		if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0) {
			_ShowContextMenu(IndexOf(where), where);
			return;
		}
		BOutlineListView::MouseDown(where);
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kAddFavorite:
			case B_SIMPLE_DATA:
			{
				// folders dropped on the panel, or chosen from the menu
				if (message->what == B_SIMPLE_DATA && !message->WasDropped()) {
					BOutlineListView::MessageReceived(message);
					break;
				}
				entry_ref ref;
				bool added = false;
				for (int32 i = 0; message->FindRef("refs", i, &ref) == B_OK; i++) {
					added |= _AddFavorite(ref);
				}
				if (added)
					_SaveAndRebuildFavorites();
				break;
			}

			case kRemoveFavorite:
			{
				int32 index;
				if (message->FindInt32("index", &index) == B_OK && index >= 0 && index < (int32)fFavorites.size()) {
					fFavorites.erase(fFavorites.begin() + index);
					_SaveAndRebuildFavorites();
				}
				break;
			}

			case kMoveFavorite:
			{
				int32 index, delta;
				if (message->FindInt32("index", &index) == B_OK && message->FindInt32("delta", &delta) == B_OK) {
					const int32 other = index + delta;
					if (index >= 0 && index < (int32)fFavorites.size() && other >= 0 && other < (int32)fFavorites.size()) {
						BString moved = fFavorites[index];
						fFavorites[index] = fFavorites[other];
						fFavorites[other] = moved;
						_SaveAndRebuildFavorites();
					}
				}
				break;
			}

			case kEmptyTrash:
				FSEmptyTrash();
				break;

			case kToggleTrash:
			{
				TrackerSettings settings;
				const bool show = !settings.SnakeSidePanelTrash();
				settings.SetSnakeSidePanelTrash(show);
				settings.SaveSettings(false);
				if (show) {
					_AddTrash();
				} else {
					for (int32 i = 0; i < FullListCountItems(); i++) {
						SideItem* item = dynamic_cast<SideItem*>(FullListItemAt(i));
						if (item != NULL && item->IsTrash()) {
							RemoveItem(item);
							delete item;
							break;
						}
					}
				}
				Invalidate();
				break;
			}

			case kOpenInNewWindow:
			{
				entry_ref ref;
				TTracker* tracker = dynamic_cast<TTracker*>(be_app);
				if (message->FindRef("refs", &ref) == B_OK && tracker != NULL) {
					// a window of its own, even where Tracker already shows the folder in one (it would only bring
					// that one to the front)
					tracker->OpenFolderInNewWindow(&ref);
				}
				break;
			}

			default:
				BOutlineListView::MessageReceived(message);
		}
	}

private:
	// The mounted volumes other than the boot volume (a USB stick, a second disk), after the favourites. Nothing at
	// all, not even a heading, when there are none.
	void _AddVolumes()
	{
		BVolume boot;
		BVolumeRoster().GetBootVolume(&boot);
		BVolumeRoster roster;
		BVolume volume;
		while (roster.GetNextVolume(&volume) == B_OK) {
			if (!volume.IsPersistent() || volume == boot)
				continue;
			// the package volumes show the installed packages: not disks
			fs_info info;
			if (fs_stat_dev(volume.Device(), &info) == B_OK && strcmp(info.fsh_name, "packagefs") == 0)
				continue;
			BDirectory root;
			BEntry entry;
			entry_ref ref;
			BPath path;
			char name[B_FILE_NAME_LENGTH];
			if (volume.GetRootDirectory(&root) != B_OK || root.GetEntry(&entry) != B_OK
				|| entry.GetRef(&ref) != B_OK || entry.GetPath(&path) != B_OK || volume.GetName(name) != B_OK) {
				continue;
			}
			SideItem* item = new SideItem(&ref, name, 0);
			item->SetVolume(volume.Device());
			item->SetPath(path.Path());
			_AddFolder(item, -1);
		}
	}

	// The Trash, if it is to be shown: after the favourites, before the volumes.
	void _AddTrash()
	{
		if (!TrackerSettings().SnakeSidePanelTrash())
			return;
		BPath path;
		BEntry entry;
		entry_ref ref;
		if (find_directory(B_TRASH_DIRECTORY, &path) != B_OK || entry.SetTo(path.Path()) != B_OK
			|| entry.GetRef(&ref) != B_OK) {
			return;
		}
		SideItem* item = new SideItem(&ref, "Trash", 0);
		item->SetTrash();
		AddItem(item, _FavoriteEnd());
	}

	// where the lines after the favourites start
	int32 _FavoriteEnd() const
	{
		int32 i = 1;
		while (i < FullListCountItems()) {
			SideItem* item = dynamic_cast<SideItem*>(FullListItemAt(i));
			if (item == NULL || !(item->IsFavorite() || item->IsHint() || item->OutlineLevel() > 0))
				break;
			i++;
		}
		return i;
	}

	// ---- the favourites: one folder to a line, in ~/config/settings/Tracker/SnakeFavorites

	static bool _FavoritesFile(BPath* path)
	{
		if (find_directory(B_USER_SETTINGS_DIRECTORY, path, true) != B_OK || path->Append("Tracker") != B_OK)
			return false;
		create_directory(path->Path(), 0755);
		return path->Append("SnakeFavorites") == B_OK;
	}

	void _LoadFavorites()
	{
		fFavorites.clear();
		BPath file;
		BFile in;
		if (_FavoritesFile(&file) && in.SetTo(file.Path(), B_READ_ONLY) == B_OK) {
			off_t size = 0;
			in.GetSize(&size);
			BString text;
			char* buffer = text.LockBuffer(size + 1);
			ssize_t read = buffer != NULL ? in.Read(buffer, size) : 0;
			if (buffer != NULL)
				buffer[read > 0 ? read : 0] = '\0';
			text.UnlockBuffer(read > 0 ? read : 0);
			int32 start = 0;
			while (start < text.Length()) {
				int32 end = text.FindFirst('\n', start);
				if (end < 0)
					end = text.Length();
				BString line;
				text.CopyInto(line, start, end - start);
				line.Trim();
				if (line.Length() > 0)
					fFavorites.push_back(line);
				start = end + 1;
			}
			return;
		}

		// the first time: the user's own places
		static const directory_which kStart[] = { B_USER_DIRECTORY, B_DESKTOP_DIRECTORY };
		for (size_t i = 0; i < sizeof(kStart) / sizeof(kStart[0]); i++) {
			BPath path;
			if (find_directory(kStart[i], &path) == B_OK)
				fFavorites.push_back(BString(path.Path()));
		}
		BPath downloads;
		if (find_directory(B_USER_DIRECTORY, &downloads) == B_OK && downloads.Append("Downloads") == B_OK
			&& BEntry(downloads.Path()).IsDirectory()) {
			fFavorites.push_back(BString(downloads.Path()));
		}
		_SaveFavorites();
	}

	void _SaveFavorites()
	{
		BPath file;
		BFile out;
		if (!_FavoritesFile(&file) || out.SetTo(file.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE) != B_OK)
			return;
		for (size_t i = 0; i < fFavorites.size(); i++) {
			out.Write(fFavorites[i].String(), fFavorites[i].Length());
			out.Write("\n", 1);
		}
	}

	bool _AddFavorite(const entry_ref& ref)
	{
		BEntry entry(&ref, true);
		BPath path;
		if (entry.InitCheck() != B_OK || !entry.IsDirectory() || entry.GetPath(&path) != B_OK)
			return false;
		for (size_t i = 0; i < fFavorites.size(); i++) {
			if (fFavorites[i] == path.Path())
				return false;
		}
		fFavorites.push_back(BString(path.Path()));
		return true;
	}

	// the favourites' lines go in at the given place in the list
	void _AddFavoriteItems(int32 at)
	{
		const BPath home = _HomePath();
		for (size_t i = 0; i < fFavorites.size(); i++) {
			BEntry entry(fFavorites[i].String());
			entry_ref ref;
			if (entry.GetRef(&ref) != B_OK)
				continue;
			BString label(ref.name);
			if (fFavorites[i] == home.Path())
				label = "Home";
			SideItem* item = new SideItem(&ref, label.String(), 0);
			item->SetFavorite();
			item->SetPath(fFavorites[i].String());
			item->SetFavoriteIndex((int32)i);
			AddItem(item, at++);
			AddItem(new SideItem(NULL, "", 1, false, true), at++);
			Collapse(item);
		}
		if (fFavorites.empty()) {
			SideItem* hint = new SideItem(NULL, "Drop folders here", 0);
			hint->SetHint();
			AddItem(hint, at);
		}
	}

	static BPath _HomePath()
	{
		BPath path;
		find_directory(B_USER_DIRECTORY, &path);
		return path;
	}

	void _SaveAndRebuildFavorites()
	{
		_SaveFavorites();
		// out with the old lines (up to the heading of the volumes), in with the new
		std::vector<BListItem*> old;
		for (int32 i = 1; i < FullListCountItems(); i++) {
			SideItem* item = dynamic_cast<SideItem*>(FullListItemAt(i));
			// (taking out a folder's line also takes out and deletes the lines under it: only these are ours)
			if (item != NULL && item->OutlineLevel() == 0 && (item->IsFavorite() || item->IsHint()))
				old.push_back(item);
		}
		for (size_t i = 0; i < old.size(); i++)
			RemoveItem(old[i]);
		for (size_t i = 0; i < old.size(); i++)
			delete old[i];
		_AddFavoriteItems(1);
		Invalidate();
	}

	// A volume that can be unmounted from here: a disk with a real file system that is not the boot volume. The
	// package volumes (/boot/system and the like), the boot volume and the virtual file systems are the system's own.
	static bool _CanUnmount(dev_t device)
	{
		BVolume volume(device);
		BVolume boot;
		BVolumeRoster().GetBootVolume(&boot);
		if (volume.InitCheck() != B_OK || !volume.IsPersistent() || volume == boot)
			return false;
		fs_info info;
		if (fs_stat_dev(device, &info) != B_OK)
			return false;
		static const char* kSystem[] = { "packagefs", "rootfs", "devfs", "pipefs", "tmpfs", "ramfs", "bindfs", NULL };
		for (int i = 0; kSystem[i] != NULL; i++) {
			if (strcmp(info.fsh_name, kSystem[i]) == 0)
				return false;
		}
		return true;
	}

	static bool _IsVolumeRoot(const entry_ref& ref, dev_t* device)
	{
		BNode node(&ref);
		node_ref nodeRef;
		if (node.GetNodeRef(&nodeRef) != B_OK)
			return false;
		BVolume volume(nodeRef.device);
		BDirectory root;
		node_ref rootRef;
		if (volume.InitCheck() != B_OK || volume.GetRootDirectory(&root) != B_OK || root.GetNodeRef(&rootRef) != B_OK)
			return false;
		*device = nodeRef.device;
		return nodeRef == rootRef;
	}

	void _ShowContextMenu(int32 index, BPoint where)
	{
		// (index is -1 below the last line)
		SideItem* item = index >= 0 ? dynamic_cast<SideItem*>(ItemAt(index)) : NULL;
		const bool usable = item != NULL && !item->IsHeading() && !item->IsPlaceholder() && !item->IsHint();
		if (item != NULL && !usable)
			return;

		SnakePopUpMenu* menu = new SnakePopUpMenu("sidemenu", false, false);
		BMessage* message;
		BMenuItem* menuItem;
		if (usable && item->IsTrash()) {
			menuItem = new SnakeMenuItem("Empty Trash", new BMessage(kEmptyTrash));
			menuItem->SetTarget(this);
			TTracker* tracker = dynamic_cast<TTracker*>(be_app);
			menuItem->SetEnabled(tracker != NULL && tracker->TrashFull());
			menu->AddItem(menuItem);
		} else if (usable) {
		if (item->IsFavorite()) {
			message = new BMessage(kRemoveFavorite);
			message->AddInt32("index", item->FavoriteIndex());
			menuItem = new SnakeMenuItem("Remove from favorites", message);
			menuItem->SetTarget(this);
			menu->AddItem(menuItem);

			message = new BMessage(kMoveFavorite);
			message->AddInt32("index", item->FavoriteIndex());
			message->AddInt32("delta", -1);
			menuItem = new SnakeMenuItem("Move up", message);
			menuItem->SetTarget(this);
			menuItem->SetEnabled(item->FavoriteIndex() > 0);
			menu->AddItem(menuItem);

			message = new BMessage(kMoveFavorite);
			message->AddInt32("index", item->FavoriteIndex());
			message->AddInt32("delta", 1);
			menuItem = new SnakeMenuItem("Move down", message);
			menuItem->SetTarget(this);
			menuItem->SetEnabled(item->FavoriteIndex() < (int32)fFavorites.size() - 1);
			menu->AddItem(menuItem);
			menu->AddItem(new SnakeSeparatorItem());
		} else {
			message = new BMessage(kAddFavorite);
			message->AddRef("refs", &item->Ref());
			menuItem = new SnakeMenuItem("Add to favorites", message);
			menuItem->SetTarget(this);
			menu->AddItem(menuItem);
			menu->AddItem(new SnakeSeparatorItem());
		}
		message = new BMessage(kOpenInNewWindow);
		message->AddRef("refs", &item->Ref());
		menuItem = new SnakeMenuItem("Open in new window", message);
		menuItem->SetTarget(this);
		menu->AddItem(menuItem);

		dev_t device;
		if (_IsVolumeRoot(item->Ref(), &device) && _CanUnmount(device)) {
			// only for disks that were connected: never the boot disk or anything that belongs to the system
			menu->AddItem(new SnakeSeparatorItem());
			message = new BMessage(kUnmountVolume);
			message->AddInt32("device_id", device);
			menuItem = new SnakeMenuItem("Unmount", message);
			menuItem->SetTarget(be_app);
			menu->AddItem(menuItem);
		}

		}

		// the Trash line can be shown or hidden from anywhere in the panel
		if (menu->CountItems() > 0)
			menu->AddItem(new SnakeSeparatorItem());
		menuItem = new SnakeMenuItem(TrackerSettings().SnakeSidePanelTrash() ? "Hide Trash" : "Show Trash",
			new BMessage(kToggleTrash));
		menuItem->SetTarget(this);
		menu->AddItem(menuItem);

		menu->Go(ConvertToScreen(where), true, false, true);
	}

	std::vector<BString>	fFavorites;

	void _AddFolder(SideItem* item, int32 fullListIndex)
	{
		if (fullListIndex < 0)
			AddItem(item);
		else
			AddItem(item, fullListIndex);
		// something below it, so that it can be opened (new lines start open: it starts closed)
		_AddPlaceholder(item);
		Collapse(item);
	}

	void _AddPlaceholder(SideItem* folder)
	{
		AddItem(new SideItem(NULL, "", folder->OutlineLevel() + 1, false, true), FullListIndexOf(folder) + 1);
	}

	void _Load(SideItem* folder)
	{
		folder->SetLoaded();
		// the line that stood for what is below
		for (int32 i = FullListIndexOf(folder) + 1; i < FullListCountItems(); i++) {
			SideItem* child = dynamic_cast<SideItem*>(FullListItemAt(i));
			if (child == NULL || child->OutlineLevel() <= folder->OutlineLevel())
				break;
			if (child->IsPlaceholder()) {
				RemoveItem(child);
				delete child;
				break;
			}
		}

		std::vector<SideFolder> folders;
		BDirectory directory(&folder->Ref());
		BEntry entry;
		while (directory.GetNextEntry(&entry, true) == B_OK) {
			if (!entry.IsDirectory())
				continue;
			char name[B_FILE_NAME_LENGTH];
			if (entry.GetName(name) != B_OK || name[0] == '.')
				continue;
			SideFolder f;
			f.name = name;
			if (entry.GetRef(&f.ref) != B_OK)
				continue;
			folders.push_back(f);
		}
		std::sort(folders.begin(), folders.end(), SideFolderBefore);

		int32 index = FullListIndexOf(folder) + 1;
		for (size_t i = 0; i < folders.size(); i++) {
			SideItem* item = new SideItem(&folders[i].ref, folders[i].name.String(), folder->OutlineLevel() + 1);
			AddItem(item, index++);
			AddItem(new SideItem(NULL, "", item->OutlineLevel() + 1, false, true), index++);
			Collapse(item);
		}
		// nothing below: no sign for opening
		if (folders.empty())
			Invalidate();
	}

	BHandler*	fTarget;
	bool		fProgrammatic;
};


TSidePanel::TSidePanel(BContainerWindow* window)
	:
	BView("sidepanel", B_WILL_DRAW),
	fWindow(window),
	fTree(NULL),
	fScrollView(NULL),
	fFollowRunner(NULL),
	fVolumeRoster(NULL)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	BGroupLayout* layout = new BGroupLayout(B_VERTICAL, 0);
	layout->SetInsets(0);
	SetLayout(layout);

	fTree = new SideTree(this);
	fScrollView = new BScrollView("sidescroll", fTree, 0, false, true, B_PLAIN_BORDER);
	layout->AddView(fScrollView);

	SetExplicitMinSize(BSize(110, B_SIZE_UNSET));
	SetExplicitPreferredSize(BSize(210, B_SIZE_UNSET));
}


TSidePanel::~TSidePanel()
{
	delete fFollowRunner;
	if (fVolumeRoster != NULL) {
		fVolumeRoster->StopWatching();
		delete fVolumeRoster;
	}
}


void
TSidePanel::AttachedToWindow()
{
	BView::AttachedToWindow();
	fTree->Populate();
	ShowCurrentFolder();

	BMessage message(kFollowFolder);
	fFollowRunner = new BMessageRunner(this, &message, 500000);

	// volumes that are mounted or unmounted show up (or go) here
	fVolumeRoster = new BVolumeRoster;
	fVolumeRoster->StartWatching(BMessenger(this));

}


void
TSidePanel::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kFollowFolder:
		{
			fTree->RefreshTrashIcon();
			// the window shows another folder: the tree follows
			const Model* model = fWindow->TargetModel();
			if (model != NULL && *model->NodeRef() != fShown)
				ShowCurrentFolder();
			break;
		}

		case B_NODE_MONITOR:
		{
			int32 opcode;
			if (message->FindInt32("opcode", &opcode) == B_OK
				&& (opcode == B_DEVICE_MOUNTED || opcode == B_DEVICE_UNMOUNTED)) {
				fTree->RebuildVolumes();
				ShowCurrentFolder();
			}
			break;
		}

		case kNavigateTo:
		{
			entry_ref ref;
			if (message->FindRef("refs", &ref) == B_OK) {
				BNode node(&ref);
				node_ref nodeRef;
				if (node.GetNodeRef(&nodeRef) == B_OK)
					fShown = nodeRef;
				fWindow->NavigateTo(&ref);
			}
			break;
		}

		default:
			BView::MessageReceived(message);
	}
}


void
TSidePanel::ShowCurrentFolder()
{
	const Model* model = fWindow->TargetModel();
	if (model == NULL)
		return;
	fShown = *model->NodeRef();
	fTree->ShowFolder(*model->EntryRef());
}

}	// namespace BPrivate
