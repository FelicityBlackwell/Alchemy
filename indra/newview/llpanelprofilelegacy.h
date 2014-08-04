/*
 * @file llpanelprofilelegacy.h
 * @brief Legacy protocol avatar profile panel
 *
 * Copyright (C) 2014, Cinder Roxley <cinder.roxley@phoenixviewer.com>
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef LL_PANELPROFILELEGACY_H
#define LL_PANELPROFILELEGACY_H

#include "llgroupmgr.h"
#include "llpanelavatar.h"

class LLAccordianCtrlTab;
class LLAvatarName;
class LLClassifiedItem;
class LLFlatListView;
class LLPanel;
class LLPanelPickInfo;
class LLPanelProfileTab;
class LLPanelClassifiedInfo;
class LLTextBase;
class LLToggleableMenu;

class LLPanelProfileLegacy : public LLPanelProfileTab
{
public:
	LLPanelProfileLegacy();
	virtual BOOL postBuild();
	/* virtual */ void onOpen(const LLSD& key);
	
private:
	~LLPanelProfileLegacy();
	/* virtual */ void updateData();
	/* virtual */ void processProperties(void* data, EAvatarProcessorType type);
	/* virtual */ void resetControls();
	void showAccordion(const std::string& name, bool show);
	void onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name);
	void onCommitAvatarProperties();
	void onCommitInterest();
	void onCommitNotes();
	void onCommitRights();
	void onBackBtnClick();
	void onCommitAction(const LLSD& userdata);
	bool isActionEnabled(const LLSD& userdata);
	
	LLToggleableMenu* mActionMenu;
	boost::signals2::connection mAvatarNameCacheConnection;
	
public:
	class LLPanelProfilePicks : public LLPanelProfileTab
	{
		friend class LLPanelProfileLegacy;
	public:
		LLPanelProfilePicks();
		/* virtual */ BOOL postBuild();
	protected:
		/* virtual */ void onOpen(const LLSD& key);
	private:
		/* virtual */ void updateData();
		/* virtual */ void processProperties(void* data, EAvatarProcessorType type);
		/* virtual */ void resetControls() {};
		void showAccordion(const std::string& name, bool show);
		void openPickInfo();
		LLClassifiedItem* findClassifiedById(const LLUUID& classified_id);
		
		LLFlatListView* mClassifiedsList;
		LLFlatListView* mPicksList;
	};
	
	class LLPanelProfileGroups : public LLPanelProfileTab
	{
		friend class LLPanelProfileLegacy;
	public:
		LLPanelProfileGroups();
		/* virtual */ BOOL postBuild();
	protected:
		/* virtual */ void onOpen(const LLSD& key);
	private:
		/* virtual */ void updateData();
		/* virtual */ void processProperties(void* data, EAvatarProcessorType type);
		/* virtual */ void resetControls() {};
		void showGroup(const LLUUID& id);
		
		LLTextBase* mGroupsText;
		LLFlatListView* mGroupsList;
	};
	
private:
	LLPanelProfilePicks* mPanelPicks;
	LLPanelProfileGroups* mPanelGroups;
};

class LLProfileGroupItem : public LLPanel, public LLGroupMgrObserver
{
public:
	LLProfileGroupItem();
	~LLProfileGroupItem();
	static LLProfileGroupItem* create();
	void init(const LLAvatarGroups::LLGroupData& data);
	/* virtual */ BOOL postBuild();
	
	void setValue(const LLSD& value);
	void setId(const LLUUID& id);
	void setInsignia(const LLUUID& id);
	void setName(const std::string& name);
	void setCharter(const std::string& charter);
protected:
	/* virtual */ void changed(LLGroupChange gc);
private:
	LLUUID	mInsignia;
	std::string mGroupName;
	std::string mCharter;	
};

#endif //LL_PANELPROFILELEGACY_H
