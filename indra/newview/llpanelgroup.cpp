// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llpanelgroup.cpp
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
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

#include "llpanelgroup.h"

// Library includes
#include "llaccordionctrltab.h"
#include "llaccordionctrl.h"
#include "llbutton.h"
#include "llfloaterreg.h"
#include "lltrans.h"

// Viewer includes
#include "llagent.h" 
#include "llfloatersidepanelcontainer.h"
#include "llviewermessage.h"
#include "llviewerwindow.h"
#include "llappviewer.h"
#include "llnotificationsutil.h"
#include "llgroupactions.h"

#include "llsidetraypanelcontainer.h"

#include "llpanelgroupnotices.h"
#include "llpanelgroupgeneral.h"
#include "llpanelgrouproles.h"

static LLPanelInjector<LLPanelGroup> t_panel_group("panel_group_info_sidetray");

LLPanelGroup* get_group_panel(LLUUID const& group_id)
{
	auto floater = LLFloaterReg::findInstance("group_profile", LLSD(group_id));
	if (floater)
	{
		LLPanelGroup* panel = floater->findChild<LLPanelGroup>("panel_group_info_sidetray");
		if (panel) return panel;
	}
	return nullptr;
}

LLPanelGroupTab::LLPanelGroupTab()
	: LLPanel(),
	  mAllowEdit(TRUE),
	  mHasModal(FALSE)
{
	mGroupID = LLUUID::null;
}

LLPanelGroupTab::~LLPanelGroupTab()
{
}

BOOL LLPanelGroupTab::isVisibleByAgent(LLAgent* agentp)
{
	//default to being visible
	return TRUE;
}

BOOL LLPanelGroupTab::postBuild()
{
	return TRUE;
}

LLPanelGroup::LLPanelGroup()
:	LLPanel(),
	LLGroupMgrObserver(),
	mSkipRefresh(FALSE),
	mAccordianGroup(nullptr),
	mButtonApply(nullptr),
	mButtonCall(nullptr),
	mButtonChat(nullptr),
	mButtonCreate(nullptr),
	mButtonJoin(nullptr),
	mButtonRefresh(nullptr),
	mJoinText(nullptr)
{
	// Set up the factory callbacks.
	// Roles sub tabs
	LLGroupMgr::getInstance()->addObserver(this);
	mCommitCallbackRegistrar.add("Group.CopyData", boost::bind(&LLPanelGroup::copyData, this, _2));
}

LLPanelGroup::~LLPanelGroup()
{
	for (auto itr = sGroupPanelInstances.cbegin(); itr != sGroupPanelInstances.cend(); ++itr)
	{
		if (itr->second == this)
		{
			sGroupPanelInstances.erase(itr);
			break;
		}
	}
	LLGroupMgr::getInstance()->removeObserver(this);
	if(LLVoiceClient::instanceExists())
	{
		LLVoiceClient::getInstance()->removeObserver(this);
	}
}

void LLPanelGroup::onOpen(const LLSD& key)
{
	if(!key.has("group_id"))
		return;
	
	LLUUID group_id = key["group_id"];
	if(!key.has("action"))
	{
		setGroupID(group_id);
		getChild<LLAccordionCtrl>("groups_accordion")->expandDefaultTab();
		return;
	}

	std::string str_action = key["action"];

	if(str_action == "refresh")
	{
		if(mID == group_id || group_id == LLUUID::null)
			refreshData();
	}
	else if(str_action == "close")
	{
		onBackBtnClick();
	}
	else if(str_action == "create")
	{
		setGroupID(LLUUID::null);
	}
	else if(str_action == "refresh_notices")
	{
		refreshNotices();
	}

}

BOOL LLPanelGroup::postBuild()
{
	mDefaultNeedsApplyMesg = getString("default_needs_apply_text");
	mWantApplyMesg = getString("want_apply_text");

	mAccordianGroup = getChild<LLAccordionCtrl>("groups_accordion");

	mButtonApply = getChild<LLButton>("btn_apply");
	mButtonApply->setCommitCallback(boost::bind(&LLPanelGroup::onBtnApply, this));
	mButtonApply->setVisible(true);
	mButtonApply->setEnabled(false);

	mButtonCall = getChild<LLButton>("btn_call");
	mButtonCall->setCommitCallback(boost::bind(&LLPanelGroup::callGroup, this));

	mButtonChat = getChild<LLButton>("btn_chat");
	mButtonChat->setCommitCallback(boost::bind(&LLPanelGroup::chatGroup, this));

	mButtonRefresh = getChild<LLButton>("btn_refresh");
	mButtonRefresh->setCommitCallback(boost::bind(&LLPanelGroup::refreshData, this));

	mButtonCreate = getChild<LLButton>("btn_create");
	mButtonCreate->setCommitCallback(boost::bind(&LLPanelGroup::onBtnCreate, this));
	mButtonCreate->setVisible(false);

	if (dynamic_cast<LLSideTrayPanelContainer*>(getParent()) != nullptr)
		getChild<LLUICtrl>("back")->setCommitCallback(boost::bind(&LLPanelGroup::onBackBtnClick, this));
	else if (dynamic_cast<LLFloater*>(getParent()) != nullptr)
		getChild<LLUICtrl>("back")->setCommitCallback(boost::bind(&LLPanelGroup::closeParentFloater, this));
	else
		getChild<LLUICtrl>("back")->setEnabled(FALSE);
	
	LLPanelGroupTab* panel_general = findChild<LLPanelGroupTab>("group_general_tab_panel");
	LLPanelGroupTab* panel_roles = findChild<LLPanelGroupTab>("group_roles_tab_panel");
	LLPanelGroupTab* panel_banlist = findChild<LLPanelGroupTab>("group_banlist_tab_panel");
	LLPanelGroupTab* panel_notices = findChild<LLPanelGroupTab>("group_notices_tab_panel");
	LLPanelGroupTab* panel_land = findChild<LLPanelGroupTab>("group_land_tab_panel");
	LLPanelGroupTab* panel_experiences = findChild<LLPanelGroupTab>("group_experiences_tab_panel");

	if(panel_general)	mTabs.push_back(panel_general);
	if(panel_roles)		mTabs.push_back(panel_roles);
	if(panel_banlist)	mTabs.push_back(panel_banlist);
	if(panel_notices)	mTabs.push_back(panel_notices);
	if(panel_land)		mTabs.push_back(panel_land);
	if(panel_experiences)		mTabs.push_back(panel_experiences);

	if(panel_general)
	{
		panel_general->setupCtrls(this);
		mButtonJoin = panel_general->getChild<LLButton>("btn_join");
		mButtonJoin->setVisible(false);
		mButtonJoin->setEnabled(true);
		mButtonJoin->setCommitCallback(boost::bind(&LLPanelGroup::onBtnJoin,this));

		mJoinText = panel_general->getChild<LLUICtrl>("join_cost_text");
	}

	LLVoiceClient::getInstance()->addObserver(this);
	
	return TRUE;
}

void LLPanelGroup::reposButton(LLButton* button)
{
	if(!button)
		return;
	LLRect btn_rect = button->getRect();
	btn_rect.setLeftTopAndSize( btn_rect.mLeft, btn_rect.getHeight() + 2, btn_rect.getWidth(), btn_rect.getHeight());
	button->setRect(btn_rect);
}

void LLPanelGroup::reposButtons()
{
	reposButton(mButtonApply);
	reposButton(mButtonCreate);
	reposButton(mButtonRefresh);
	reposButton(mButtonChat);
	reposButton(mButtonCall);
}

void LLPanelGroup::reshape(S32 width, S32 height, BOOL called_from_parent )
{
	LLPanel::reshape(width, height, called_from_parent );

	reposButtons();
}

void LLPanelGroup::onBackBtnClick()
{
	LLSideTrayPanelContainer* parent = dynamic_cast<LLSideTrayPanelContainer*>(getParent());
	if(parent)
	{
		parent->openPreviousPanel();
	}
}


void LLPanelGroup::onBtnCreate()
{
	LLPanelGroupGeneral* panel_general = findChild<LLPanelGroupGeneral>("group_general_tab_panel");
	if(!panel_general)
		return;
	std::string apply_mesg;
	if(panel_general->apply(apply_mesg))//yes yes you need to call apply to create...
		return;
	if ( !apply_mesg.empty() )
	{
		LLSD args;
		args["MESSAGE"] = apply_mesg;
		LLNotificationsUtil::add("GenericAlert", args);
	}
}

void LLPanelGroup::onBtnApply()
{
	apply();
	refreshData();
}


void LLPanelGroup::onBtnJoin() const
{
	LL_DEBUGS() << "joining group: " << mID << LL_ENDL;
	LLGroupActions::join(mID);
}

void LLPanelGroup::changed(LLGroupChange gc)
{
	for(std::vector<LLPanelGroupTab* >::iterator it = mTabs.begin();it!=mTabs.end();++it)
		(*it)->update(gc);
	update(gc);
}

// virtual
void LLPanelGroup::onChange(EStatusType status, const std::string &channelURI, bool proximal)
{
	if(status == STATUS_JOINING || status == STATUS_LEFT_CHANNEL)
	{
		return;
	}

	mButtonCall->setEnabled(LLVoiceClient::getInstance()->voiceEnabled() && LLVoiceClient::getInstance()->isVoiceWorking());
}

void LLPanelGroup::notifyObservers()
{
	changed(GC_ALL);
}

void LLPanelGroup::update(LLGroupChange gc)
{
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mID);
	if(gdatap)
	{
		std::string group_name =  gdatap->mName.empty() ? LLTrans::getString("LoadingData") : gdatap->mName;
		LLUICtrl* group_name_ctrl = getChild<LLUICtrl>("group_name");
		group_name_ctrl->setValue(group_name);
		group_name_ctrl->setToolTip(group_name);
		
		LLGroupData agent_gdatap;
		bool is_member = gAgent.getGroupData(mID,agent_gdatap) || gAgent.isGodlikeWithoutAdminMenuFakery();
		bool join_btn_visible = !is_member && gdatap->mOpenEnrollment;
		
		mButtonJoin->setVisible(join_btn_visible);
		mJoinText->setVisible(join_btn_visible);

		if(join_btn_visible)
		{
			LLStringUtil::format_map_t string_args;
			std::string fee_buff;
			if(gdatap->mMembershipFee)
			{
				string_args["[AMOUNT]"] = llformat("%d", gdatap->mMembershipFee);
				fee_buff = getString("group_join_btn", string_args);
				
			}
			else
			{
				fee_buff = getString("group_join_free", string_args);
			}
			mJoinText->setValue(fee_buff);
		}
	}
}

void LLPanelGroup::setGroupID(const LLUUID& group_id)
{
	std::string str_group_id;
	group_id.toString(str_group_id);

	bool is_same_id = group_id == mID;
	
	for (auto itr = sGroupPanelInstances.cbegin(); itr != sGroupPanelInstances.cend(); ++itr)
	{
		if (itr->second == this)
		{
			sGroupPanelInstances.erase(itr);
			break;
		}
	}
	LLGroupMgr::getInstance()->removeObserver(this);
	mID = group_id;

	sGroupPanelInstances.emplace(std::make_pair(LLUUID(mID), static_cast<LLPanelGroup*>(this)));
	LLGroupMgr::getInstance()->addObserver(this);

	for(std::vector<LLPanelGroupTab* >::iterator it = mTabs.begin();it!=mTabs.end();++it)
		(*it)->setGroupID(group_id);

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mID);
	if(gdatap)
	{
		std::string group_name =  gdatap->mName.empty() ? LLTrans::getString("LoadingData") : gdatap->mName;
		LLUICtrl* group_name_ctrl = getChild<LLUICtrl>("group_name");
		group_name_ctrl->setValue(group_name);
		group_name_ctrl->setToolTip(group_name);
	}

	bool is_null_group_id = group_id == LLUUID::null;
	mButtonApply->setVisible(!is_null_group_id);
	mButtonRefresh->setVisible(!is_null_group_id);

	mButtonCreate->setVisible(is_null_group_id);

	mButtonCall->setVisible(!is_null_group_id);
	mButtonChat->setVisible(!is_null_group_id);

	getChild<LLUICtrl>("prepend_founded_by")->setVisible(!is_null_group_id);

	mAccordianGroup->reset();

	LLAccordionCtrlTab* tab_general = getChild<LLAccordionCtrlTab>("group_general_tab");
	LLAccordionCtrlTab* tab_roles = getChild<LLAccordionCtrlTab>("group_roles_tab");
	LLAccordionCtrlTab* tab_banlist = getChild<LLAccordionCtrlTab>("group_banlist_tab");
	LLAccordionCtrlTab* tab_notices = getChild<LLAccordionCtrlTab>("group_notices_tab");
	LLAccordionCtrlTab* tab_land = getChild<LLAccordionCtrlTab>("group_land_tab");
	LLAccordionCtrlTab* tab_experiences = getChild<LLAccordionCtrlTab>("group_experiences_tab");

	if(mButtonJoin)
		mButtonJoin->setVisible(false);


	if(is_null_group_id)//creating new group
	{
		if(!tab_general->getDisplayChildren())
			tab_general->changeOpenClose(tab_general->getDisplayChildren());
		
		if(tab_roles->getDisplayChildren())
			tab_roles->changeOpenClose(tab_roles->getDisplayChildren());
		if(tab_banlist->getDisplayChildren())
			tab_banlist->changeOpenClose(tab_banlist->getDisplayChildren());
		if(tab_notices->getDisplayChildren())
			tab_notices->changeOpenClose(tab_notices->getDisplayChildren());
		if(tab_land->getDisplayChildren())
			tab_land->changeOpenClose(tab_land->getDisplayChildren());
		if(tab_experiences->getDisplayChildren())
			tab_experiences->changeOpenClose(tab_experiences->getDisplayChildren());

		tab_roles->setVisible(false);
		tab_banlist->setVisible(false);
		tab_notices->setVisible(false);
		tab_land->setVisible(false);
		tab_experiences->setVisible(false);

		getChild<LLUICtrl>("group_name")->setVisible(false);
		getChild<LLUICtrl>("group_name_editor")->setVisible(true);

		mButtonCall->setVisible(false);
		mButtonChat->setVisible(false);
	}
	else 
	{
		if(!is_same_id)
		{
			if(!tab_general->getDisplayChildren())
				tab_general->changeOpenClose(tab_general->getDisplayChildren());
			if(tab_roles->getDisplayChildren())
				tab_roles->changeOpenClose(tab_roles->getDisplayChildren());
			if(tab_banlist->getDisplayChildren())
				tab_banlist->changeOpenClose(tab_banlist->getDisplayChildren());
			if(tab_notices->getDisplayChildren())
				tab_notices->changeOpenClose(tab_notices->getDisplayChildren());
			if(tab_land->getDisplayChildren())
				tab_land->changeOpenClose(tab_land->getDisplayChildren());
			if(tab_experiences->getDisplayChildren())
				tab_experiences->changeOpenClose(tab_experiences->getDisplayChildren());
		}

		LLGroupData agent_gdatap;
		bool is_member = gAgent.getGroupData(mID,agent_gdatap) || gAgent.isGodlikeWithoutAdminMenuFakery();
		
		tab_roles->setVisible(is_member);
		tab_banlist->setVisible(is_member);
		tab_notices->setVisible(is_member);
		tab_land->setVisible(is_member);
		tab_experiences->setVisible(is_member);

		getChild<LLUICtrl>("group_name")->setVisible(true);
		getChild<LLUICtrl>("group_name_editor")->setVisible(false);

		mButtonApply->setVisible(is_member);
		mButtonCall->setVisible(is_member);
		mButtonChat->setVisible(is_member);
	}

	mAccordianGroup->arrange();

	reposButtons();
	update(GC_ALL);//show/hide "join" button if data is already ready
}

bool LLPanelGroup::apply(LLPanelGroupTab* tab)
{
	if(!tab)
		return false;

	std::string mesg;
	if ( !tab->needsApply(mesg) )
		return true;
	
	std::string apply_mesg;
	if(tab->apply( apply_mesg ) )
	{
		//we skip refreshing group after ew manually apply changes since its very annoying
		//for those who are editing group

		LLPanelGroupRoles * roles_tab = dynamic_cast<LLPanelGroupRoles*>(tab);
		if (roles_tab)
		{
			LLGroupMgr* gmgrp = LLGroupMgr::getInstance();
			LLGroupMgrGroupData* gdatap = gmgrp->getGroupData(roles_tab->getGroupID());

			// allow refresh only for one specific case:
			// there is only one member in group and it is not owner
			// it's a wrong situation and need refresh panels from server
			if (gdatap && gdatap->isSingleMemberNotOwner())
			{
				return true;
			}
		}

		mSkipRefresh = TRUE;
		return true;
	}
		
	if ( !apply_mesg.empty() )
	{
		LLSD args;
		args["MESSAGE"] = apply_mesg;
		LLNotificationsUtil::add("GenericAlert", args);
	}
	return false;
}

void LLPanelGroup::refreshNotices()
{
	LLPanelGroupNotices* panel_notices = findChild<LLPanelGroupNotices>("group_notices_tab_panel");
	if (panel_notices)
		panel_notices->refreshNotices();
}

bool LLPanelGroup::apply()
{
	bool applied = apply(findChild<LLPanelGroupTab>("group_general_tab_panel"));
	LL_DEBUGS() << "Applied changes to group_general_tab_panel? " << (applied ? "yes" : "no") << LL_ENDL;
	applied = apply(findChild<LLPanelGroupTab>("group_roles_tab_panel"));
	LL_DEBUGS() << "Applied changes to group_roles_tab_panel? " << (applied ? "yes" : "no") << LL_ENDL;
	applied = apply(findChild<LLPanelGroupTab>("panel_banlist_tab_panel"));
	LL_DEBUGS() << "Applied changes to panel_banlist_tab_panel? " << (applied ? "yes" : "no") << LL_ENDL;
	applied = apply(findChild<LLPanelGroupTab>("group_notices_tab_panel"));
	LL_DEBUGS() << "Applied changes to group_notices_tab_panel? " << (applied ? "yes" : "no") << LL_ENDL;
	applied = apply(findChild<LLPanelGroupTab>("group_land_tab_panel"));
	LL_DEBUGS() << "Applied changes to group_land_tab_panel? " << (applied ? "yes" : "no") << LL_ENDL;
	applied = apply(findChild<LLPanelGroupTab>("group_experiences_tab_panel"));
	LL_DEBUGS() << "Applied changes to group_experiences_tab_panel? " << (applied ? "yes" : "no") << LL_ENDL;
	return applied;
}


// virtual
void LLPanelGroup::draw()
{
	LLPanel::draw();

	if (mRefreshTimer.hasExpired())
	{
		mRefreshTimer.stop();
		mButtonRefresh->setEnabled(TRUE);
		mAccordianGroup->setEnabled(TRUE);
	}

	if(mButtonApply->getVisible())
	{
		bool enable = false;
		std::string mesg;
		for(std::vector<LLPanelGroupTab* >::iterator it = mTabs.begin();it!=mTabs.end();++it)
			enable = enable || (*it)->needsApply(mesg);

		mButtonApply->setEnabled(enable);
	}
}

void LLPanelGroup::refreshData()
{
	if(mSkipRefresh)
	{
		mSkipRefresh = FALSE;
		return;
	}
	LLGroupMgr::getInstance()->clearGroupData(getID());

	setGroupID(getID());
	
	// 5 second timeout
	mButtonRefresh->setEnabled(FALSE);
	mAccordianGroup->setEnabled(FALSE);

	mRefreshTimer.start();
	mRefreshTimer.setTimerExpirySec(5);
}

void LLPanelGroup::callGroup()
{
	LLGroupActions::startCall(getID());
}

void LLPanelGroup::chatGroup()
{
	LLGroupActions::startIM(getID());
}

void LLPanelGroup::showNotice(const std::string& subject,
			      const std::string& message,
			      const bool& has_inventory,
			      const std::string& inventory_name,
			      LLOfferInfo* inventory_offer)
{
	LLPanelGroupNotices* panel_notices = findChild<LLPanelGroupNotices>("group_notices_tab_panel");
	if(!panel_notices)
	{
		// We need to clean up that inventory offer.
		if (inventory_offer)
		{
			inventory_offer->forceResponse(IOR_DECLINE);
		}
		return;
	}
	panel_notices->showNotice(subject,message,has_inventory,inventory_name,inventory_offer);
}




//static
void LLPanelGroup::refreshCreatedGroup(const LLUUID& group_id)
{
	LLPanelGroup* panel = get_group_panel(group_id);
	if(!panel)
		return;
	panel->setGroupID(group_id);
}

//static
void LLPanelGroup::showNotice(const std::string& subject,
					   const std::string& message,
					   const LLUUID& group_id,
					   const bool& has_inventory,
					   const std::string& inventory_name,
					   LLOfferInfo* inventory_offer)
{
	LLPanelGroup* panel = get_group_panel(group_id);
	if(!panel || panel->getID() != group_id) return;
	panel->showNotice(subject,message,has_inventory,inventory_name,inventory_offer);
}


void LLPanelGroup::copyData(const LLSD& userdata)
{
	const std::string& param = userdata.asString();
	if (param == "copy_name")
		LLGroupActions::copyData(mID, LLGroupActions::E_DATA_NAME);
	else if (param == "copy_slurl")
		LLGroupActions::copyData(mID, LLGroupActions::E_DATA_SLURL);
	else if (param == "copy_key")
		LLGroupActions::copyData(mID, LLGroupActions::E_DATA_UUID);
	else
		LL_WARNS() << "Unhandled action: " << param << LL_ENDL;
}

void LLPanelGroup::closeParentFloater()
{
	LLFloater* floater = dynamic_cast<LLFloater*>(getParent());
	if (floater) floater->closeFloater();
}
