/*
 * @file llfloaterprogressview.cpp
 * @brief Progress floater
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
#include "llfloaterprogressview.h"

#include "llfloaterreg.h"
#include "llprogressbar.h"
#include "lltextbase.h"

#include "llagent.h"

LLFloaterProgressView::LLFloaterProgressView(const LLSD& key)
:	LLFloater(key)
,	mProgressBar(nullptr)
,	mProgressText(nullptr)
,	mLocationText(nullptr)
,	mCancelBtn(nullptr)
{
	mCommitCallbackRegistrar.add("cancel", boost::bind(&LLFloaterProgressView::onCommitCancel, this, _1));
}

LLFloaterProgressView::~LLFloaterProgressView()
{
}

BOOL LLFloaterProgressView::postBuild()
{
	mProgressBar = getChild<LLProgressBar>("progress_bar");
	mProgressText = getChild<LLTextBase>("progress_text");
	mLocationText = getChild<LLTextBase>("location");
	mCancelBtn = getChild<LLButton>("cancel_btn");
	return TRUE;
}

void LLFloaterProgressView::setRegion(const std::string& region, bool haz_region)
{
	// yeah, this bool is pretty fucking stupid. so what? wanna fight about it?
	if (haz_region)
	{
		LLStringUtil::format_map_t arg;
		arg["REGION"] = region;
		mLocationText->setText(getString("loc_fmt", arg));
	}
	else
		mLocationText->setText(region);
}

void LLFloaterProgressView::setProgressText(const std::string& text)
{
	mProgressText->setValue(text);
}

void LLFloaterProgressView::setProgressPercent(const F32 percent)
{
	mProgressBar->setValue(percent);
}

void LLFloaterProgressView::setProgressCancelButtonVisible(BOOL visible, const std::string& label)
{
	mCancelBtn->setVisible(visible);
	mCancelBtn->setEnabled(visible);
	mCancelBtn->setLabelSelected(label);
	mCancelBtn->setLabelUnselected(label);
}

void LLFloaterProgressView::onCommitCancel(LLUICtrl* ctrl)
{
	gAgent.teleportCancel();
	ctrl->setEnabled(FALSE);
}
