// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/*
 * @file llpanelsearchplaces.cpp
 * @brief Groups search panel
 *
 * Copyright (c) 2014, Cinder Roxley <cinder@sdf.org>
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
#include "llpanelsearchplaces.h"
#include "llfloaterdirectory.h"

#include "llfloaterreg.h"
#include "llqueryflags.h"

#include "llagent.h"
#include "llcombobox.h"
#include "llnotificationsutil.h"
#include "llparcel.h"
#include "llsearcheditor.h"
#include "llsearchhistory.h"

static LLPanelInjector<LLPanelSearchPlaces> t_panel_search_places("panel_search_places");

LLPanelSearchPlaces::LLPanelSearchPlaces()
:	LLPanelSearch()
{
	mCommitCallbackRegistrar.add("Search.query", boost::bind(&LLPanelSearchPlaces::onCommitSearch, this, _1));
}

BOOL LLPanelSearchPlaces::postBuild()
{
	mPlacesCategory = getChild<LLComboBox>("places_category");
	mPlacesCategory->add("All categories", LLSD("any"));
	mPlacesCategory->addSeparator();
	for (size_t category = LLParcel::C_LINDEN; category < LLParcel::C_COUNT; ++category)
	{
		LLParcel::ECategory eCategory = static_cast<LLParcel::ECategory>(category);
		mPlacesCategory->add(LLParcel::getCategoryUIString(eCategory), LLParcel::getCategoryString(eCategory));
	}
	
	mSearchEditor = getChild<LLSearchEditor>("search_bar");
	//mSearchEditor->setKeystrokeCallback(boost::bind(&LLPanelSearchPlaces::onCommitSearch, this, _1));
	
	return TRUE;
}

void LLPanelSearchPlaces::onCommitSearch(LLUICtrl* ctrl)
{
	LLSearchEditor* pSearchEditor = dynamic_cast<LLSearchEditor*>(ctrl);
	if (pSearchEditor)
	{
		std::string text = pSearchEditor->getText();
		LLStringUtil::trim(text);
		if (text.length() <= MIN_SEARCH_STRING_SIZE)
			LLSearchHistory::getInstance()->addEntry(text);
	}
	search();
}

void LLPanelSearchPlaces::search()
{
	LLDirQuery query;
	query.type = SE_PLACES;
	query.results_per_page = 100;
	query.text = mSearchEditor->getText();
	LLStringUtil::trim(query.text);
	
	const std::string& category_string = mPlacesCategory->getSelectedValue();
	query.category_char = category_string == "any" ? LLParcel::C_ANY : LLParcel::getCategoryFromString(category_string);
	
	static LLUICachedControl<bool> inc_pg("ShowPGSims", 1);
	static LLUICachedControl<bool> inc_mature("ShowMatureSims", 0);
	static LLUICachedControl<bool> inc_adult("ShowAdultSims", 0);
	if (!(inc_pg || inc_mature || inc_adult))
	{
		LLNotificationsUtil::add("NoContentToSearch");
		return;
	}
	if (gAgent.wantsPGOnly())
		query.scope |= DFQ_PG_SIMS_ONLY;
	if (inc_pg)
		query.scope |= DFQ_INC_PG;
	if (inc_mature && gAgent.canAccessMature())
		query.scope |= DFQ_INC_MATURE;
	if (inc_adult && gAgent.canAccessAdult())
		query.scope |= DFQ_INC_ADULT;
	query.scope |= DFQ_DWELL_SORT;
	
	mFloater->queryDirectory(query, true);
}
