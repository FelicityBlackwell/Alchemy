/*
 * @file llpanelaomini.cpp
 * @brief Animation overrides mini control panel
 *
 * Copyright (c) 2016, Cinder Roxley <cinder@sdf.org>
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

#include "llviewerprecompiledheaders.h"
#include "llpanelaomini.h"

#include "llaoengine.h"
#include "llfloaterao.h"
#include "llfloaterreg.h"
#include "llcombobox.h"

static LLPanelInjector<LLPanelAOMini> t_ao_mini("ao_mini");

LLPanelAOMini::LLPanelAOMini() 
	: LLPanel()
	, mSetList(nullptr)
{
	mCommitCallbackRegistrar.add("AO.SitOverride", boost::bind(&LLPanelAOMini::onClickSit, this, _2));
	mCommitCallbackRegistrar.add("AO.NextAnim", boost::bind(&LLPanelAOMini::onClickNext, this));
	mCommitCallbackRegistrar.add("AO.PrevAnim", boost::bind(&LLPanelAOMini::onClickPrevious, this));
	mCommitCallbackRegistrar.add("AO.OpenFloater", boost::bind(&LLPanelAOMini::openAOFloater, this));
	
	//mEnableCallbackRegistrar.add("AO.EnableSet", boost::bind());
	//mEnableCallbackRegistrar.add("AO.EnableState", boost::bind());
}

LLPanelAOMini::~LLPanelAOMini()
{
	if (mReloadCallback.connected())
		mReloadCallback.disconnect();
	if (mSetChangedCallback.connected())
		mSetChangedCallback.disconnect();
}

BOOL LLPanelAOMini::postBuild()
{
	mSetList = getChild<LLComboBox>("set_list");
	mSetList->setCommitCallback(boost::bind(&LLPanelAOMini::onSelectSet, this, _2));
	mReloadCallback = LLAOEngine::instance().setReloadCallback(boost::bind(&LLPanelAOMini::updateSetList, this));
	mSetChangedCallback = LLAOEngine::instance().setSetChangedCallback(boost::bind(&LLPanelAOMini::onSetChanged, this, _1));
	
	return TRUE;
}

/////////////////////////////////////
// Callback receivers

void LLPanelAOMini::updateSetList()
{
	std::vector<LLAOSet*> list = LLAOEngine::getInstance()->getSetList();
	if (list.empty())
	{
		return;
	}
	mSetList->removeall();
	for (LLAOSet* set : list)
	{
		mSetList->add(set->getName(), &set, ADD_BOTTOM, true);
	}
	const std::string& current_set = LLAOEngine::instance().getCurrentSetName();
	mSetList->selectByValue(LLSD(current_set));
}

void LLPanelAOMini::onSetChanged(const std::string& set_name)
{
	mSetList->selectByValue(LLSD(set_name));
}

////////////////////////////////////
// Control actions

void LLPanelAOMini::onSelectSet(const LLSD& userdata)
{
	LLAOSet* selected_set = LLAOEngine::instance().getSetByName(userdata.asString());
	if (selected_set)
	{
		LLAOEngine::instance().selectSet(selected_set);
	}
}

void LLPanelAOMini::onClickSit(const LLSD& userdata)
{
	const std::string& current_set = LLAOEngine::instance().getCurrentSetName();
	LLAOSet* selected_set = LLAOEngine::instance().getSetByName(current_set);
	if (selected_set)
	{
		LLAOEngine::instance().setOverrideSits(selected_set, userdata.asBoolean());
	}
}

void LLPanelAOMini::onClickNext()
{
	LLAOEngine::instance().cycle(LLAOEngine::CycleNext);
}

void LLPanelAOMini::onClickPrevious()
{
	LLAOEngine::instance().cycle(LLAOEngine::CyclePrevious);
}

void LLPanelAOMini::openAOFloater()
{
	LLFloaterReg::showInstance("ao");
}
