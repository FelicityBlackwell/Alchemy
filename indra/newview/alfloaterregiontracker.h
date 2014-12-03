/**
* @file alfloaterregiontracker.h
* @brief Region tracking floater
*
* $LicenseInfo:firstyear=2013&license=viewerlgpl$
* Alchemy Viewer Source Code
* Copyright (C) 2014, Alchemy Viewer Project.
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
* $/LicenseInfo$
*/

#pragma once

#include "lleventtimer.h"
#include "llfloater.h"

class LLButton;
class LLSD;
class LLScrollListCtrl;

class ALFloaterRegionTracker : public LLFloater, public LLEventTimer
{
	friend class LLFloaterReg;
private:
	ALFloaterRegionTracker(const LLSD& key);
	virtual ~ALFloaterRegionTracker();
public:
	/*virtual*/ BOOL postBuild();
	/*virtual*/ void onOpen(const LLSD& key);
	/*virtual*/ void onClose(bool app_quitting);
	/*virtual*/ void refresh();
	/*virtual*/ BOOL tick();

private:
	void updateHeader();
	void requestRegionData();
	void removeRegions();
	bool saveToJSON();
	bool loadFromJSON();
	void openMap();

public:
	std::string getRegionLabelIfExists(const std::string& name);
	void onRegionAddedCallback(const LLSD& notification, const LLSD& response);

private:
	LLSD mRegionMap;
	LLButton* mRefreshRegionListBtn = nullptr;
	LLButton* mRemoveRegionBtn = nullptr;
	LLButton* mOpenMapBtn = nullptr;
	LLScrollListCtrl* mRegionScrollList = nullptr;
};
