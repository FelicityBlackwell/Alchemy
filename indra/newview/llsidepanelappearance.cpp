/**
 * @file llsidepanelappearance.cpp
 * @brief Side Bar "Appearance" panel
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "llsidepanelappearance.h"

#include "llaccordionctrltab.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llagentwearables.h"
#include "llappearancemgr.h"
#include "llfloatersidepanelcontainer.h"
#include "llfolderview.h"
#include "llinventorypanel.h"
#include "llfiltereditor.h"
#include "llfloaterreg.h"
#include "llfloaterworldmap.h"
#include "llfolderviewmodel.h"
#include "llloadingindicator.h"
#include "lloutfitobserver.h"
#include "llpaneleditwearable.h"
#include "llpaneloutfitsinventory.h"
#include "lltextbox.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"
#include "llviewerwearable.h"

static LLPanelInjector<LLSidepanelAppearance> t_appearance("sidepanel_appearance");

class LLCurrentlyWornFetchObserver final : public LLInventoryFetchItemsObserver
{
public:
	LLCurrentlyWornFetchObserver(const uuid_vec_t &ids,
								 LLSidepanelAppearance *panel) :
		LLInventoryFetchItemsObserver(ids),
		mPanel(panel)
	{}
	~LLCurrentlyWornFetchObserver() = default;

	void done() override
	{
		mPanel->inventoryFetched();
		gInventory.removeObserver(this);
		delete this;
	}
private:
	LLSidepanelAppearance *mPanel;
};

LLSidepanelAppearance::LLSidepanelAppearance() :
	LLPanel(),
	mOutfitLoadingIndicator(nullptr),
	mFilterEditor(nullptr),
	mOutfitEdit(nullptr),
	mEditOutfitBtn(nullptr),
	mCurrOutfitPanel(nullptr),
	mFilterSubString(LLStringUtil::null),
	mOpened(false)
{
	LLOutfitObserver& outfit_observer =  LLOutfitObserver::instance();
	outfit_observer.addBOFReplacedCallback(boost::bind(&LLSidepanelAppearance::refreshCurrentOutfitName, this, ""));
	outfit_observer.addBOFChangedCallback(boost::bind(&LLSidepanelAppearance::refreshCurrentOutfitName, this, ""));
	outfit_observer.addCOFChangedCallback(boost::bind(&LLSidepanelAppearance::refreshCurrentOutfitName, this, ""));

	gAgentWearables.addLoadingStartedCallback(boost::bind(&LLSidepanelAppearance::setWearablesLoading, this, true));
	gAgentWearables.addLoadedCallback(boost::bind(&LLSidepanelAppearance::setWearablesLoading, this, false));
}

// virtual
BOOL LLSidepanelAppearance::postBuild()
{
	mOutfitLoadingIndicator = getChild<LLLoadingIndicator>("wearables_loading_indicator");

	mOpenOutfitBtn = getChild<LLButton>("openoutfit_btn");
	mOpenOutfitBtn->setClickedCallback(boost::bind(&LLSidepanelAppearance::onOpenOutfitButtonClicked, this));

	mEditAppearanceBtn = getChild<LLButton>("editappearance_btn");
	mEditAppearanceBtn->setClickedCallback(boost::bind(&LLSidepanelAppearance::onEditAppearanceButtonClicked, this));

	mEditOutfitBtn = getChild<LLButton>("edit_outfit_btn");
	mEditOutfitBtn->setClickedCallback(boost::bind(&LLSidepanelAppearance::showOutfitEditPanel, this));

	mFilterEditor = getChild<LLFilterEditor>("Filter");
	if (mFilterEditor)
	{
		mFilterEditor->setCommitCallback(boost::bind(&LLSidepanelAppearance::onFilterEdit, this, _2));
	}

	mPanelOutfitsInventory = dynamic_cast<LLPanelOutfitsInventory *>(getChild<LLPanel>("panel_outfits_inventory"));

	mOutfitEdit = dynamic_cast<LLPanelOutfitEdit*>(getChild<LLPanel>("panel_outfit_edit"));
	if (mOutfitEdit)
	{
		LLButton* back_btn = mOutfitEdit->getChild<LLButton>("back_btn");
		if (back_btn)
		{
			back_btn->setClickedCallback(boost::bind(&LLSidepanelAppearance::showOutfitsInventoryPanel, this));
		}

	}

	mEditWearable = dynamic_cast<LLPanelEditWearable*>(getChild<LLPanel>("panel_edit_wearable"));
	if (mEditWearable)
	{
		LLButton* edit_wearable_back_btn = mEditWearable->getChild<LLButton>("back_btn");
		if (edit_wearable_back_btn)
		{
			edit_wearable_back_btn->setClickedCallback(boost::bind(&LLSidepanelAppearance::showOutfitEditPanel, this));
		}
	}

	mCurrentLookName = getChild<LLTextBox>("currentlook_name");

	mOutfitStatus = getChild<LLTextBox>("currentlook_status");
	
	mCurrOutfitPanel = getChild<LLPanel>("panel_currentlook");


	setVisibleCallback(boost::bind(&LLSidepanelAppearance::onVisibilityChanged,this,_2));

	return TRUE;
}

// virtual
void LLSidepanelAppearance::onOpen(const LLSD& key)
{
	if (!key.has("type"))
	{
		// No specific panel requested.
		// If we're opened for the first time then show My Outfits.
		// Else do nothing.
		if (!mOpened)
		{
			showOutfitsInventoryPanel();
		}
	}
	else
	{
		// Switch to the requested panel.
		std::string type = key["type"].asString();
		if (type == "my_outfits")
		{
			showOutfitsInventoryPanel();
		}
		else if (type == "edit_outfit")
		{
			showOutfitEditPanel();
		}
		else if (type == "edit_shape")
		{
			showWearableEditPanel();
		}
	}

	mOpened = true;
}

void LLSidepanelAppearance::onVisibilityChanged(const LLSD &new_visibility)
{
	LLSD visibility;
	visibility["visible"] = new_visibility.asBoolean();
	visibility["reset_accordion"] = false;
	updateToVisibility(visibility);
}

void LLSidepanelAppearance::updateToVisibility(const LLSD &new_visibility)
{
	if (new_visibility["visible"].asBoolean())
	{
		const BOOL is_outfit_edit_visible = mOutfitEdit && mOutfitEdit->getVisible();
		const BOOL is_wearable_edit_visible = mEditWearable && mEditWearable->getVisible();

		if (is_outfit_edit_visible || is_wearable_edit_visible)
		{
			const LLViewerWearable *wearable_ptr = mEditWearable ? mEditWearable->getWearable() : NULL;
			if (!wearable_ptr)
			{
				LL_WARNS() << "Visibility change to invalid wearable" << LL_ENDL;
				return;
			}
			// Disable camera switch is currently just for WT_PHYSICS type since we don't want to freeze the avatar
			// when editing its physics.
			if (!gAgentCamera.cameraCustomizeAvatar())
			{
				LLVOAvatarSelf::onCustomizeStart(LLWearableType::getDisableCameraSwitch(wearable_ptr->getType()));
			}
			if (is_wearable_edit_visible)
			{
				U32 index;
				if (!gAgentWearables.getWearableIndex(wearable_ptr,index))
				{
					// we're no longer wearing the wearable we were last editing, switch back to outfit editor
					showOutfitEditPanel();
				}
			}

			if (is_outfit_edit_visible && new_visibility["reset_accordion"].asBoolean())
			{
				mOutfitEdit->resetAccordionState();
			}
		}
	}
	else
	{
		if (gAgentCamera.cameraCustomizeAvatar() && gSavedSettings.getBOOL("AppearanceCameraMovement"))
		{
			gAgentCamera.changeCameraToDefault();
			gAgentCamera.resetView();
		}	
	}
}

void LLSidepanelAppearance::onFilterEdit(const std::string& search_string)
{
	if (mFilterSubString != search_string)
	{
		mFilterSubString = search_string;

		// Searches are case-insensitive
		// but we don't convert the typed string to upper-case so that it can be fed to the web search as-is.

		mPanelOutfitsInventory->onSearchEdit(mFilterSubString);
	}
}

void LLSidepanelAppearance::onOpenOutfitButtonClicked()
{
	const LLViewerInventoryItem *outfit_link = LLAppearanceMgr::getInstance()->getBaseOutfitLink();
	if (!outfit_link)
		return;
	if (!outfit_link->getIsLinkType())
		return;

	LLAccordionCtrlTab* tab_outfits = mPanelOutfitsInventory->findChild<LLAccordionCtrlTab>("tab_outfits");
	if (tab_outfits)
	{
		tab_outfits->changeOpenClose(FALSE);
		LLInventoryPanel *inventory_panel = tab_outfits->findChild<LLInventoryPanel>("outfitslist_tab");
		if (inventory_panel)
		{
			LLFolderView* root = inventory_panel->getRootFolder();
			LLFolderViewItem *outfit_folder =    inventory_panel->getItemByID(outfit_link->getLinkedUUID());
			if (outfit_folder)
			{
				outfit_folder->setOpen(!outfit_folder->isOpen());
				root->setSelection(outfit_folder,TRUE);
				root->scrollToShowSelection();
			}
		}
	}
}

// *TODO: obsolete?
void LLSidepanelAppearance::onEditAppearanceButtonClicked()
{
	if (gAgentWearables.areWearablesLoaded())
	{
		LLVOAvatarSelf::onCustomizeStart();
	}
}

void LLSidepanelAppearance::showOutfitsInventoryPanel()
{
	toggleWearableEditPanel(FALSE);
	toggleOutfitEditPanel(FALSE);
	toggleMyOutfitsPanel(TRUE);
}

void LLSidepanelAppearance::showOutfitEditPanel()
{
	if (mOutfitEdit && mOutfitEdit->getVisible()) return;

	// Accordion's state must be reset in all cases except the one when user
	// is returning back to the mOutfitEdit panel from the mEditWearable panel.
	// The simplest way to control this is to check the visibility state of the mEditWearable
	// BEFORE it is changed by the call to the toggleWearableEditPanel(FALSE, NULL, TRUE).
	if (mEditWearable != nullptr && !mEditWearable->getVisible() && mOutfitEdit != nullptr)
	{
		mOutfitEdit->resetAccordionState();
	}

	// If we're exiting the edit wearable view, and the camera was not focused on the avatar
	// (e.g. such as if we were editing a physics param), then skip the outfits edit mode since
	// otherwise this would trigger the camera focus mode.
	if (mEditWearable != nullptr && mEditWearable->getVisible() && !gAgentCamera.cameraCustomizeAvatar())
	{
		showOutfitsInventoryPanel();
		return;
	}

	toggleMyOutfitsPanel(FALSE);
	toggleWearableEditPanel(FALSE, nullptr, TRUE); // don't switch out of edit appearance mode
	toggleOutfitEditPanel(TRUE);
}

void LLSidepanelAppearance::showWearableEditPanel(LLViewerWearable *wearable /* = NULL*/, BOOL disable_camera_switch)
{
	toggleMyOutfitsPanel(FALSE);
	toggleOutfitEditPanel(FALSE, TRUE); // don't switch out of edit appearance mode
	toggleWearableEditPanel(TRUE, wearable, disable_camera_switch);
}

void LLSidepanelAppearance::toggleMyOutfitsPanel(BOOL visible)
{
	if (!mPanelOutfitsInventory || mPanelOutfitsInventory->getVisible() == visible)
	{
		// visibility isn't changing, hence nothing to do
		return;
	}

	mPanelOutfitsInventory->setVisible(visible);

	// *TODO: Move these controls to panel_outfits_inventory.xml
	// so that we don't need to toggle them explicitly.
	mFilterEditor->setVisible(visible);
	mCurrOutfitPanel->setVisible(visible);

	if (visible)
	{
		mPanelOutfitsInventory->onOpen(LLSD());
	}
}

void LLSidepanelAppearance::toggleOutfitEditPanel(BOOL visible, BOOL disable_camera_switch)
{
	if (!mOutfitEdit || mOutfitEdit->getVisible() == visible)
	{
		// visibility isn't changing, hence nothing to do
		return;
	}

	mOutfitEdit->setVisible(visible);

	if (visible)
	{
		mOutfitEdit->onOpen(LLSD());
		LLVOAvatarSelf::onCustomizeStart(disable_camera_switch);
	}
	else 
	{
		if (!disable_camera_switch)   // if we're just switching between outfit and wearable editing, don't end customization.
		{
			LLVOAvatarSelf::onCustomizeEnd(disable_camera_switch);
			LLAppearanceMgr::getInstance()->updateIsDirty();
		}
	}
}

void LLSidepanelAppearance::toggleWearableEditPanel(BOOL visible, LLViewerWearable *wearable, BOOL disable_camera_switch)
{
	if (!mEditWearable)
	{
		return;
	}

	if (mEditWearable->getVisible() == visible && (!visible || mEditWearable->getWearable() == wearable))
	{
		// visibility isn't changing and panel doesn't need an update, hence nothing to do
		return;
	}

	// If we're just switching between outfit and wearable editing or updating item,
	// don't end customization and don't switch camera
	// Don't end customization and don't switch camera without visibility change
	BOOL change_state = !disable_camera_switch && mEditWearable->getVisible() != visible;

	if (!wearable)
	{
		wearable = gAgentWearables.getViewerWearable(LLWearableType::WT_SHAPE, 0);
	}
	if (!wearable)
	{
		return;
	}

	// Toggle panel visibility.
	mEditWearable->setVisible(visible);

	if (visible)
	{
		LLVOAvatarSelf::onCustomizeStart(!change_state);
		mEditWearable->setWearable(wearable, !change_state);
		mEditWearable->onOpen(LLSD()); // currently no-op, just for consistency
	}
	else
	{
		// Save changes if closing.
		mEditWearable->saveChanges();
		mEditWearable->setWearable(nullptr);
		LLAppearanceMgr::getInstance()->updateIsDirty();
		if (change_state)
		{
			LLVOAvatarSelf::onCustomizeEnd(!change_state);
		}
	}
}

void LLSidepanelAppearance::refreshCurrentOutfitName(const std::string& name)
{
	// Set current outfit status (wearing/unsaved).
	bool dirty = LLAppearanceMgr::getInstance()->isOutfitDirty();
	std::string cof_status_str = getString(dirty ? "Unsaved Changes" : "Now Wearing");
	mOutfitStatus->setText(cof_status_str);

	if (name.empty())
	{
		std::string outfit_name;
		if (LLAppearanceMgr::getInstance()->getBaseOutfitName(outfit_name))
		{
				mCurrentLookName->setText(outfit_name);
				return;
		}

		std::string string_name = gAgentWearables.isCOFChangeInProgress() ? "Changing outfits" : "No Outfit";
		mCurrentLookName->setText(getString(string_name));
		mOpenOutfitBtn->setEnabled(FALSE);
	}
	else
	{
		mCurrentLookName->setText(name);
		// Can't just call update verbs since the folder link may not have been created yet.
		mOpenOutfitBtn->setEnabled(TRUE);
	}
}

//static
void LLSidepanelAppearance::editWearable(LLViewerWearable *wearable, LLView *data, BOOL disable_camera_switch)
{
	LLFloaterSidePanelContainer::showPanel("appearance", LLSD());
	LLSidepanelAppearance *panel = dynamic_cast<LLSidepanelAppearance*>(data);
	if (panel)
	{	
		panel->showWearableEditPanel(wearable, disable_camera_switch);
	}
}

// Fetch currently worn items and only enable the New Look button after everything's been
// fetched.  Alternatively, we could stuff this logic into llagentwearables::makeNewOutfitLinks.
void LLSidepanelAppearance::fetchInventory()
{
	uuid_vec_t ids;
	LLUUID item_id;
	for(S32 type = (S32)LLWearableType::WT_SHAPE; type < (S32)LLWearableType::WT_COUNT; ++type)
	{
		for (U32 index = 0; index < gAgentWearables.getWearableCount((LLWearableType::EType)type); ++index)
		{
			item_id = gAgentWearables.getWearableItemID((LLWearableType::EType)type, index);
			if(item_id.notNull())
			{
				ids.push_back(item_id);
			}
		}
	}

	if (isAgentAvatarValid())
	{
		for (LLVOAvatar::attachment_map_t::const_iterator iter = gAgentAvatarp->mAttachmentPoints.begin(); 
			 iter != gAgentAvatarp->mAttachmentPoints.end(); ++iter)
		{
			LLViewerJointAttachment* attachment = iter->second;
			if (!attachment) continue;
			for (auto attached_object : attachment->mAttachedObjects)
            {
                if (!attached_object) continue;
				const LLUUID& item_id = attached_object->getAttachmentItemID();
				if (item_id.isNull()) continue;
				ids.push_back(item_id);
			}
		}
	}

	LLCurrentlyWornFetchObserver *fetch_worn = new LLCurrentlyWornFetchObserver(ids, this);
	fetch_worn->startFetch();
	// If no items to be fetched, done will never be triggered.
	// TODO: Change LLInventoryFetchItemsObserver::fetchItems to trigger done() on this condition.
	if (fetch_worn->isFinished())
	{
		fetch_worn->done();
	}
	else
	{
		gInventory.addObserver(fetch_worn);
	}
}

void LLSidepanelAppearance::inventoryFetched()
{
}

void LLSidepanelAppearance::setWearablesLoading(bool val)
{
	mOutfitLoadingIndicator->setVisible( val);
	mEditOutfitBtn->setVisible( !val);

	if (!val)
	{
		// refresh outfit name when COF is already changed.
		refreshCurrentOutfitName();
	}
}

void LLSidepanelAppearance::showDefaultSubpart()
{
	if (mEditWearable->getVisible())
	{
		mEditWearable->showDefaultSubpart();
	}
}

void LLSidepanelAppearance::updateScrollingPanelList()
{
	if (mEditWearable->getVisible())
	{
		mEditWearable->updateScrollingPanelList();
	}
}
