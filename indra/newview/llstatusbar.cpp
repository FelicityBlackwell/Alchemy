/** 
* @file llstatusbar.cpp
* @brief LLStatusBar class implementation
*
* $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

#include "llstatusbar.h"

// viewer includes
#include "llagent.h"
#include "llbutton.h"
#include "llcommandhandler.h"
#include "llfirstuse.h"
#include "llviewercontrol.h"
#include "llbuycurrencyhtml.h"
#include "llpanelaopulldown.h"
#include "llpanelnearbymedia.h"
#include "alpanelquicksettingspulldown.h"
#include "llpanelvolumepulldown.h"
#include "llpanelavcomplexitypulldown.h"
#include "llfloaterscriptdebug.h"
#include "llhudicon.h"
#include "llkeyboard.h"
#include "llmenugl.h"
#include "llrootview.h"
#include "llsd.h"
#include "lltextbox.h"
#include "llui.h"
#include "llviewerstats.h"
#include "llviewerwindow.h"
#include "llframetimer.h"
#include "llvoavatarself.h"
#include "llresmgr.h"
#include "llworld.h"
#include "llstatgraph.h"
#include "llviewerjoystick.h"
#include "llviewermedia.h"
#include "llviewermenu.h"	// for gMenuBarView
#include "llviewerthrottle.h"

#include "llappviewer.h"

// library includes
#include "llfloaterreg.h"
#include "llrect.h"
#include "llstring.h"
#include "message.h"
#include "llsearchableui.h"
#include "llsearcheditor.h"

//
// Globals
//
LLStatusBar *gStatusBar = nullptr;
S32 STATUS_BAR_HEIGHT = 26;


// TODO: these values ought to be in the XML too
const S32 SIM_STAT_WIDTH = 8;
const LLColor4 SIM_OK_COLOR(0.f, 1.f, 0.f, 1.f);
const LLColor4 SIM_WARN_COLOR(1.f, 1.f, 0.f, 1.f);
const LLColor4 SIM_FULL_COLOR(1.f, 0.f, 0.f, 1.f);
const F32 ICON_TIMER_EXPIRY		= 3.f; // How long the balance and health icons should flash after a change.

LLStatusBar::LLStatusBar(const LLRect& rect)
:	LLPanel(),
	mFilterEdit(nullptr),			// Edit for filtering
	mSearchPanel(nullptr),			// Panel for filtering
	mTextTime(nullptr),
	mTextFPS(nullptr),
	mSGBandwidth(nullptr),
	mSGPacketLoss(nullptr),
	mSGSpinLock(nullptr),
	mPanelPopupHolder(nullptr),
	mBtnQuickSettings(nullptr),
	mBtnAO(nullptr),
	mBtnVolume(nullptr),
	mBoxBalance(nullptr),
	mBtnBuyL(nullptr),
	mAvComplexity(nullptr),
	mPanelFlycam(nullptr),
	mBalance(0),
	mHealth(100),
	mSquareMetersCredit(0),
	mSquareMetersCommitted(0),
	mImgAvComplex(nullptr),
	mImgAvComplexWarn(nullptr),
	mImgAvComplexHeavy(nullptr)
{
	LLView::setRect(rect);
	
	// status bar can possible overlay menus?
	setMouseOpaque(FALSE);

	mBalanceTimer = new LLFrameTimer();
	mHealthTimer = new LLFrameTimer();

	mImgAvComplex = LLUI::getUIImage("50_Ton_Weight");
	mImgAvComplexWarn = LLUI::getUIImage("50_Ton_Weight_Warn");
	mImgAvComplexHeavy = LLUI::getUIImage("50_Ton_Weight_Heavy");

	buildFromFile("panel_status_bar.xml");
}

LLStatusBar::~LLStatusBar()
{
    if (mCurrencyChangedSlot.connected())
	{
        mCurrencyChangedSlot.disconnect();
	}

	delete mBalanceTimer;
	mBalanceTimer = nullptr;

	delete mHealthTimer;
	mHealthTimer = nullptr;

	// LLView destructor cleans up children
}

//-----------------------------------------------------------------------
// Overrides
//-----------------------------------------------------------------------

// virtual
void LLStatusBar::draw()
{
	refresh();
	LLPanel::draw();
}

BOOL LLStatusBar::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	show_navbar_context_menu(this,x,y);
	return TRUE;
}

BOOL LLStatusBar::postBuild()
{
	gMenuBarView->setRightMouseDownCallback(boost::bind(&show_navbar_context_menu, _1, _2, _3));

	mPanelPopupHolder = gViewerWindow->getRootView()->getChildView("popup_holder");

	mTextTime = getChild<LLTextBox>("TimeText" );
	
	mTextFPS = getChild<LLTextBox>("FPSText");

	mBtnBuyL = getChild<LLButton>("buyL");
	mBtnBuyL->setCommitCallback(boost::bind(&LLStatusBar::onClickBuyCurrency, this));

    //getChild<LLUICtrl>("goShop")->setCommitCallback(boost::bind(&LLWeb::loadURL, gSavedSettings.getString("MarketplaceURL"), LLStringUtil::null, LLStringUtil::null));

	mBoxBalance = getChild<LLTextBox>("balance");
	mBoxBalance->setClickedCallback( &LLStatusBar::onClickBalance, this );
	
	mBtnQuickSettings = getChild<LLButton>("quick_settings_btn");
	mBtnQuickSettings->setMouseEnterCallback(boost::bind(&LLStatusBar::onMouseEnterQuickSettings, this));

	mBtnAO = getChild<LLButton>("ao_btn");
	mBtnAO->setClickedCallback(&LLStatusBar::onClickAOBtn, this);
	mBtnAO->setMouseEnterCallback(boost::bind(&LLStatusBar::onMouseEnterAO, this));
	mBtnAO->setToggleState(gSavedPerAccountSettings.getBOOL("UseAO")); // shunt it into correct state - ALCH-368

	mBtnVolume = getChild<LLButton>( "volume_btn" );
	mBtnVolume->setClickedCallback(&LLStatusBar::onClickVolume, this );
	mBtnVolume->setMouseEnterCallback(boost::bind(&LLStatusBar::onMouseEnterVolume, this));

	mMediaToggle = getChild<LLButton>("media_toggle_btn");
	mMediaToggle->setClickedCallback( &LLStatusBar::onClickMediaToggle, this );
	mMediaToggle->setMouseEnterCallback(boost::bind(&LLStatusBar::onMouseEnterNearbyMedia, this));
	
	mAvComplexity = getChild<LLIconCtrl>("av_complexity");
	mAvComplexity->setMouseEnterCallback(boost::bind(&LLStatusBar::onMouseEnterAvatarComplexity, this));

	mPanelFlycam = getChild<LLUICtrl>("flycam_lp");

	gSavedSettings.getControl("MuteAudio")->getSignal()->connect(boost::bind(&LLStatusBar::onVolumeChanged, this, _2));
	gSavedPerAccountSettings.getControl("UseAO")->getCommitSignal()->connect(boost::bind(&LLStatusBar::onAOStateChanged, this));

	// Adding Net Stat Graph
	S32 x = getRect().getWidth() - 2;
	S32 y = 0;
	LLRect r;
	r.set( x-SIM_STAT_WIDTH, y+MENU_BAR_HEIGHT-1, x, y+1);
	LLStatGraph::Params sgp;
	sgp.name("BandwidthGraph");
	sgp.rect(r);
	sgp.follows.flags(FOLLOWS_BOTTOM | FOLLOWS_RIGHT);
	sgp.mouse_opaque(false);
	sgp.stat("activemessagedatareceived");
	sgp.unit_label("Kbps");
	sgp.decimal_digits(0);
	sgp.label("UDP Data Received");
	mSGBandwidth = LLUICtrlFactory::create<LLStatGraph>(sgp);
	addChild(mSGBandwidth);
	x -= SIM_STAT_WIDTH + 2;

	r.set( x-SIM_STAT_WIDTH, y+MENU_BAR_HEIGHT-1, x, y+1);
	//these don't seem to like being reused
	LLStatGraph::Params pgp;
	pgp.name("PacketLossPercent");
	pgp.rect(r);
	pgp.follows.flags(FOLLOWS_BOTTOM | FOLLOWS_RIGHT);
	pgp.mouse_opaque(false);
	pgp.stat("packetslostpercentstat");
	pgp.min(0.f);
	pgp.max(5.f);
	pgp.decimal_digits(1);
	pgp.label("Packet Loss");
	LLStatGraph::Thresholds thresholds;
	thresholds.threshold.add(LLStatGraph::ThresholdParams().value(0.1f).color(LLColor4::green))
						.add(LLStatGraph::ThresholdParams().value(0.25f).color(LLColor4::yellow))
						.add(LLStatGraph::ThresholdParams().value(0.6f).color(LLColor4::red));

	pgp.thresholds(thresholds);

	mSGPacketLoss = LLUICtrlFactory::create<LLStatGraph>(pgp);
	addChild(mSGPacketLoss);

	LLRect slpRect;
	slpRect.set(1, MENU_BAR_HEIGHT - 2, SIM_STAT_WIDTH + 1, 0);
	LLStatGraph::Params slp;
	slp.name("SpinLockPercent");
	slp.rect(slpRect);
	slp.follows.flags(FOLLOWS_TOP | FOLLOWS_RIGHT);
	slp.layout("topleft");
	slp.mouse_opaque(false);
	slp.stat("fpslimitspinlockpercentstat");
	slp.min(0.0f);
	slp.max(100.0f);
	slp.decimal_digits(1);
	slp.label("Spin Lock");
	LLStatGraph::Thresholds slpThresholds;
	slpThresholds.threshold	.add(LLStatGraph::ThresholdParams().value(0.0f).color(LLColor4::green))
							.add(LLStatGraph::ThresholdParams().value(0.125f).color(LLColor4::yellow))
							.add(LLStatGraph::ThresholdParams().value(0.25f).color(LLColor4::red))
							.add(LLStatGraph::ThresholdParams().value(0.5f).color(LLColor4::red));
	slp.thresholds(slpThresholds);

	mSGSpinLock = LLUICtrlFactory::create<LLStatGraph>(slp);
	mFpsSpinlockPanel = getChild<LLUICtrl>("fps_spinlock_lp");
	mFpsSpinlockPanel->addChild(mSGSpinLock);
	mSGSpinLock->setVisible(TRUE);

	mPanelQuickSettingsPulldown = new ALPanelQuickSettingsPulldown();
	addChild(mPanelQuickSettingsPulldown);
	mPanelQuickSettingsPulldown->setFollows(FOLLOWS_TOP | FOLLOWS_RIGHT);
	mPanelQuickSettingsPulldown->setVisible(FALSE);

	mPanelAOPulldown = new LLPanelAOPulldown();
	addChild(mPanelAOPulldown);
	mPanelAOPulldown->setFollows(FOLLOWS_TOP | FOLLOWS_RIGHT);
	mPanelAOPulldown->setVisible(FALSE);

	mPanelVolumePulldown = new LLPanelVolumePulldown();
	addChild(mPanelVolumePulldown);
	mPanelVolumePulldown->setFollows(FOLLOWS_TOP|FOLLOWS_RIGHT);
	mPanelVolumePulldown->setVisible(FALSE);

	mPanelAvatarComplexityPulldown = new LLPanelAvatarComplexityPulldown();
	addChild(mPanelAvatarComplexityPulldown);
	mPanelAvatarComplexityPulldown->setFollows(FOLLOWS_TOP | FOLLOWS_RIGHT);
	mPanelAvatarComplexityPulldown->setVisible(FALSE);

	mPanelNearByMedia = new LLPanelNearByMedia();
	addChild(mPanelNearByMedia);
	mPanelNearByMedia->setFollows(FOLLOWS_TOP|FOLLOWS_RIGHT);
	mPanelNearByMedia->setVisible(FALSE);

	updateBalancePanelPosition();

	// Hook up and init for filtering
	mFilterEdit = getChild<LLSearchEditor>( "search_menu_edit" );
	mSearchPanel = getChild<LLPanel>( "menu_search_panel" );

	BOOL search_panel_visible = gSavedSettings.getBOOL("MenuSearch");
	mSearchPanel->setVisible(search_panel_visible);
	mFilterEdit->setKeystrokeCallback(boost::bind(&LLStatusBar::onUpdateFilterTerm, this));
	mFilterEdit->setCommitCallback(boost::bind(&LLStatusBar::onUpdateFilterTerm, this));
	collectSearchableItems();
	gSavedSettings.getControl("MenuSearch")->getCommitSignal()->connect(boost::bind(&LLStatusBar::updateMenuSearchVisibility, this, _2));

	if (search_panel_visible)
	{
		updateMenuSearchPosition();
	}

    mCurrencyChangedSlot = LLCurrencyWrapper::getInstance()->addCurrencyChangedCb(
        [&] { mBtnBuyL->updateCurrencySymbols(); sendMoneyBalanceRequest(); });

	return TRUE;
}

// Per-frame updates of visibility
void LLStatusBar::refresh()
{
	static LLCachedControl<bool> show_net_stats(gSavedSettings, "ShowNetStats", false);
	static LLCachedControl<bool> show_fps(gSavedSettings, "ShowStatusBarFPS", true);

	if (show_net_stats)
	{
		// Adding Net Stat Meter back in
		F32 bwtotal = gViewerThrottle.getMaxBandwidth() / 1024.f;
		mSGBandwidth->setMin(0.f);
		mSGBandwidth->setMax(bwtotal*1.25f);
		//mSGBandwidth->setThreshold(0, bwtotal*0.75f);
		//mSGBandwidth->setThreshold(1, bwtotal);
		//mSGBandwidth->setThreshold(2, bwtotal);
	}
	
	if (show_fps && mFPSUpdateTimer.getElapsedTimeF32() > 0.25f)
	{
		mFPSUpdateTimer.reset();
		F32 fps = (F32)LLTrace::get_frame_recording().getLastRecording().getMean(LLStatViewer::FPS_SAMPLE);
		mTextFPS->setText(fmt::format(FMT_STRING("{:.1f}"), fps));
	}

	// update clock every second
	if(mClockUpdateTimer.getElapsedTimeF32() > 1.f) // <alchemy/>
	{
		mClockUpdateTimer.reset();

		time_t utc_time = time_corrected();

		// <alchemy> Allow user to control whether the clock shows seconds or not.
		static LLCachedControl<bool> want_precise_clock(gSavedSettings, "AlchemyPreciseClock", true);

		// Show seconds if so desired
		
		std::string timeStr = getString(want_precise_clock() ? "timePrecise" : "time");
		// </alchemy>
		LLSD substitution;
		substitution["datetime"] = static_cast<S32>(utc_time);
		LLStringUtil::format (timeStr, substitution);
		mTextTime->setText(timeStr);

		// set the tooltip to have the date
		std::string dtStr = getString("timeTooltip");
		LLStringUtil::format (dtStr, substitution);
		mTextTime->setToolTip (dtStr);
	}

	LLRect r;
	const S32 MENU_RIGHT = gMenuBarView->getRightmostMenuEdge();

	// reshape menu bar to its content's width
	if (MENU_RIGHT != gMenuBarView->getRect().getWidth())
	{
		gMenuBarView->reshape(MENU_RIGHT, gMenuBarView->getRect().getHeight());
	}

	mSGBandwidth->setVisible(show_net_stats);
	mSGPacketLoss->setVisible(show_net_stats);
	mPanelFlycam->setVisible(LLViewerJoystick::instance().getOverrideCamera());

	// update the master volume button state
	bool mute_audio = LLAppViewer::instance()->getMasterSystemAudioMute();
	mBtnVolume->setToggleState(mute_audio);

	LLViewerMedia* media_inst = LLViewerMedia::getInstance();

	// Disable media toggle if there's no media, parcel media, and no parcel audio
	// (or if media is disabled)
	static const LLCachedControl<bool> audio_streaming_enabled(gSavedSettings, "AudioStreamingMusic");
	static const LLCachedControl<bool> media_streaming_enabled(gSavedSettings, "AudioStreamingMedia");
	bool button_enabled = (audio_streaming_enabled || media_streaming_enabled) &&
						  (media_inst->hasInWorldMedia() || media_inst->hasParcelMedia() || media_inst->hasParcelAudio());
	mMediaToggle->setEnabled(button_enabled);
	// Note the "sense" of the toggle is opposite whether media is playing or not
	bool any_media_playing = (media_inst->isAnyMediaPlaying() || 
							  media_inst->isParcelMediaPlaying() ||
							  media_inst->isParcelAudioPlaying());
	mMediaToggle->setValue(!any_media_playing);
}

void LLStatusBar::setVisibleForMouselook(bool visible)
{
	mTextTime->setVisible(visible);
	mBoxBalance->setVisible(visible);
	mBtnBuyL->setVisible(visible);
	mBtnQuickSettings->setVisible(visible);
	mBtnAO->setVisible(visible);
	mBtnVolume->setVisible(visible);
	mMediaToggle->setVisible(visible);
	mAvComplexity->setVisible(visible);
	mTextFPS->setVisible(visible);
	mSGBandwidth->setVisible(visible);
	mSGPacketLoss->setVisible(visible);
	mSGSpinLock->setVisible(visible);
	mSearchPanel->setVisible(visible && gSavedSettings.getBOOL("MenuSearch"));
	setBackgroundVisible(visible);
}

void LLStatusBar::debitBalance(S32 debit)
{
	setBalance(getBalance() - debit);
}

void LLStatusBar::creditBalance(S32 credit)
{
	setBalance(getBalance() + credit);
}

void LLStatusBar::setBalance(S32 balance)
{
	if (balance > getBalance() && getBalance() != 0)
	{
		LLFirstUse::receiveLindens();
	}

	std::string money_str = LLResMgr::getInstance()->getMonetaryString( balance );

	LLStringUtil::format_map_t string_args;
	string_args["[AMT]"] = llformat("%s", money_str.c_str());
	std::string label_str = getString("buycurrencylabel", string_args);
	mBoxBalance->setValue(label_str);

	updateBalancePanelPosition();
	// If the search panel is shown, move this according to the new balance width. Parcel text will reshape itself in setParcelInfoText
	if (mSearchPanel && mSearchPanel->getVisible())
	{
		updateMenuSearchPosition();
	}

	if (mBalance && (fabs((F32)(mBalance - balance)) > gSavedSettings.getF32("UISndMoneyChangeThreshold")))
	{
		make_ui_sound(mBalance > balance ? "UISndMoneyChangeDown" : "UISndMoneyChangeUp");
	}

	if( balance != mBalance )
	{
		mBalanceTimer->reset();
		mBalanceTimer->setTimerExpirySec( ICON_TIMER_EXPIRY );
		mBalance = balance;
	}
}


// static
void LLStatusBar::sendMoneyBalanceRequest()
{
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_MoneyBalanceRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_MoneyData);
	msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null );
	gAgent.sendReliableMessage();
}


void LLStatusBar::setHealth(S32 health)
{
	//LL_INFOS() << "Setting health to: " << buffer << LL_ENDL;
	if( mHealth > health )
	{
		if (mHealth > (health + gSavedSettings.getF32("UISndHealthReductionThreshold")))
		{
			if (isAgentAvatarValid())
			{
				if (gAgentAvatarp->getSex() == SEX_FEMALE)
				{
					make_ui_sound("UISndHealthReductionF");
				}
				else
				{
					make_ui_sound("UISndHealthReductionM");
				}
			}
		}

		mHealthTimer->reset();
		mHealthTimer->setTimerExpirySec( ICON_TIMER_EXPIRY );
	}

	mHealth = health;
}

void LLStatusBar::setAvComplexity(S32 complexity, F32 muted_pct, U32 agents)
{
	if (muted_pct >= 25.f)
		mAvComplexity->setImage(mImgAvComplexWarn);
	else if (muted_pct >= 75.f)
		mAvComplexity->setImage(mImgAvComplexHeavy);
	else
		mAvComplexity->setImage(mImgAvComplex);

	mPanelAvatarComplexityPulldown->setAvComplexity(complexity, muted_pct, agents);
}

S32 LLStatusBar::getBalance() const
{
	return mBalance;
}


S32 LLStatusBar::getHealth() const
{
	return mHealth;
}

void LLStatusBar::setLandCredit(S32 credit)
{
	mSquareMetersCredit = credit;
}
void LLStatusBar::setLandCommitted(S32 committed)
{
	mSquareMetersCommitted = committed;
}

BOOL LLStatusBar::isUserTiered() const
{
	return (mSquareMetersCredit > 0);
}

S32 LLStatusBar::getSquareMetersCredit() const
{
	return mSquareMetersCredit;
}

S32 LLStatusBar::getSquareMetersCommitted() const
{
	return mSquareMetersCommitted;
}

S32 LLStatusBar::getSquareMetersLeft() const
{
	return mSquareMetersCredit - mSquareMetersCommitted;
}

void LLStatusBar::onClickBuyCurrency() const
{
	// open a currency floater - actual one open depends on 
	// value specified in settings.xml
	LLBuyCurrencyHTML::openCurrencyFloater();
	LLFirstUse::receiveLindens(false);
}

void LLStatusBar::onMouseEnterQuickSettings()
{
	LLRect qs_rect = mPanelQuickSettingsPulldown->getRect();
	LLRect qs_btn_rect = mBtnQuickSettings->getRect();
	qs_rect.setLeftTopAndSize(qs_btn_rect.mLeft -
		(qs_rect.getWidth() - qs_btn_rect.getWidth()) / 2,
		qs_btn_rect.mBottom,
		qs_rect.getWidth(),
		qs_rect.getHeight());
	// force onscreen
	qs_rect.translate(mPanelPopupHolder->getRect().getWidth() - qs_rect.mRight, 0);

	// show the master volume pull-down
	mPanelQuickSettingsPulldown->setShape(qs_rect);
	LLUI::getInstance()->clearPopups();
	LLUI::getInstance()->addPopup(mPanelQuickSettingsPulldown);

	mPanelNearByMedia->setVisible(FALSE);
	mPanelVolumePulldown->setVisible(FALSE);
	mPanelAOPulldown->setVisible(FALSE);
	mPanelAvatarComplexityPulldown->setVisible(FALSE);
	mPanelQuickSettingsPulldown->setVisible(TRUE);
}

void LLStatusBar::onMouseEnterAO()
{
	LLRect qs_rect = mPanelAOPulldown->getRect();
	LLRect qs_btn_rect = mBtnAO->getRect();
	qs_rect.setLeftTopAndSize(qs_btn_rect.mLeft -
							  (qs_rect.getWidth() - qs_btn_rect.getWidth()) / 2,
							  qs_btn_rect.mBottom,
							  qs_rect.getWidth(),
							  qs_rect.getHeight());
	// force onscreen
	qs_rect.translate(mPanelPopupHolder->getRect().getWidth() - qs_rect.mRight, 0);
	
	mPanelAOPulldown->setShape(qs_rect);
	LLUI::getInstance()->clearPopups();
	LLUI::getInstance()->addPopup(mPanelAOPulldown);
	
	mPanelNearByMedia->setVisible(FALSE);
	mPanelVolumePulldown->setVisible(FALSE);
	mPanelQuickSettingsPulldown->setVisible(FALSE);
	mPanelAOPulldown->setVisible(TRUE);
	mPanelAvatarComplexityPulldown->setVisible(FALSE);
}

void LLStatusBar::onMouseEnterVolume()
{
	LLRect vol_btn_rect = mBtnVolume->getRect();
	LLRect volume_pulldown_rect = mPanelVolumePulldown->getRect();
	volume_pulldown_rect.setLeftTopAndSize(vol_btn_rect.mLeft -
	     (volume_pulldown_rect.getWidth() - vol_btn_rect.getWidth()),
			       vol_btn_rect.mBottom,
			       volume_pulldown_rect.getWidth(),
			       volume_pulldown_rect.getHeight());

	volume_pulldown_rect.translate(mPanelPopupHolder->getRect().getWidth() - volume_pulldown_rect.mRight, 0);
	mPanelVolumePulldown->setShape(volume_pulldown_rect);


	// show the master volume pull-down
	LLUI::getInstance()->clearPopups();
	LLUI::getInstance()->addPopup(mPanelVolumePulldown);
	mPanelNearByMedia->setVisible(FALSE);
	mPanelQuickSettingsPulldown->setVisible(FALSE);
	mPanelAOPulldown->setVisible(FALSE);
	mPanelAvatarComplexityPulldown->setVisible(FALSE);
	mPanelVolumePulldown->setVisible(TRUE);
}

void LLStatusBar::onMouseEnterNearbyMedia()
{
	LLRect nearby_media_rect = mPanelNearByMedia->getRect();
	LLRect nearby_media_btn_rect = mMediaToggle->getRect();
	nearby_media_rect.setLeftTopAndSize(nearby_media_btn_rect.mLeft - 
										(nearby_media_rect.getWidth() - nearby_media_btn_rect.getWidth())/2,
										nearby_media_btn_rect.mBottom,
										nearby_media_rect.getWidth(),
										nearby_media_rect.getHeight());
	// force onscreen
	nearby_media_rect.translate(mPanelPopupHolder->getRect().getWidth() - nearby_media_rect.mRight, 0);
	
	// show the master volume pull-down
	mPanelNearByMedia->setShape(nearby_media_rect);
	LLUI::getInstance()->clearPopups();
	LLUI::getInstance()->addPopup(mPanelNearByMedia);

	mPanelQuickSettingsPulldown->setVisible(FALSE);
	mPanelVolumePulldown->setVisible(FALSE);
	mPanelAOPulldown->setVisible(FALSE);
	mPanelAvatarComplexityPulldown->setVisible(FALSE);
	mPanelNearByMedia->setVisible(TRUE);
}

void LLStatusBar::onMouseEnterAvatarComplexity()
{
	LLRect complexity_rect = mPanelAvatarComplexityPulldown->getRect();
	LLRect complexity_btn_rect = mAvComplexity->getRect();
	complexity_rect.setLeftTopAndSize(complexity_btn_rect.mLeft -
		(complexity_rect.getWidth() - complexity_btn_rect.getWidth()) / 2,
		complexity_btn_rect.mBottom,
		complexity_rect.getWidth(),
		complexity_rect.getHeight());
	// force onscreen
	complexity_rect.translate(mPanelPopupHolder->getRect().getWidth() - complexity_rect.mRight, 0);

	mPanelAvatarComplexityPulldown->setShape(complexity_rect);
	LLUI::getInstance()->clearPopups();
	LLUI::getInstance()->addPopup(mPanelAvatarComplexityPulldown);

	mPanelQuickSettingsPulldown->setVisible(FALSE);
	mPanelVolumePulldown->setVisible(FALSE);
	mPanelAOPulldown->setVisible(FALSE);
	mPanelAvatarComplexityPulldown->setVisible(TRUE);
	mPanelNearByMedia->setVisible(FALSE);
}

// static
void LLStatusBar::onClickAOBtn(void* data)
{
	gSavedPerAccountSettings.set("UseAO", !gSavedPerAccountSettings.getBOOL("UseAO"));
}

// static
void LLStatusBar::onClickVolume(void* data)
{
	// toggle the master mute setting
	bool mute_audio = LLAppViewer::instance()->getMasterSystemAudioMute();
	LLAppViewer::instance()->setMasterSystemAudioMute(!mute_audio);	
}

//static 
void LLStatusBar::onClickBalance(void* )
{
	// Force a balance request message:
	LLStatusBar::sendMoneyBalanceRequest();
	// The refresh of the display (call to setBalance()) will be done by process_money_balance_reply()
}

//static 
void LLStatusBar::onClickMediaToggle(void* data)
{
	LLStatusBar *status_bar = static_cast<LLStatusBar*>(data);
	// "Selected" means it was showing the "play" icon (so media was playing), and now it shows "pause", so turn off media
	bool pause = status_bar->mMediaToggle->getValue();
	LLViewerMedia::getInstance()->setAllMediaPaused(pause);
}

void LLStatusBar::onAOStateChanged()
{
	mBtnAO->setToggleState(gSavedPerAccountSettings.getBOOL("UseAO"));
}

BOOL can_afford_transaction(S32 cost)
{
	return((cost <= 0)||((gStatusBar) && (gStatusBar->getBalance() >=cost)));
}

void LLStatusBar::onVolumeChanged(const LLSD& newvalue)
{
	refresh();
}

void LLStatusBar::onUpdateFilterTerm()
{
	LLWString searchValue = utf8str_to_wstring( mFilterEdit->getValue() );
	LLWStringUtil::toLower( searchValue );

	if( !mSearchData || mSearchData->mLastFilter == searchValue )
		return;

	mSearchData->mLastFilter = searchValue;

	mSearchData->mRootMenu->highlightAndHide( searchValue );
	gMenuBarView->needsArrange();
}

void collectChildren( LLMenuGL *aMenu, ll::statusbar::SearchableItemPtr aParentMenu )
{
	for( U32 i = 0; i < aMenu->getItemCount(); ++i )
	{
		LLMenuItemGL *pMenu = aMenu->getItem( i );

		ll::statusbar::SearchableItemPtr pItem( new ll::statusbar::SearchableItem );
		pItem->mCtrl = pMenu;
		pItem->mMenu = pMenu;
		pItem->mLabel = utf8str_to_wstring( pMenu->ll::ui::SearchableControl::getSearchText() );
		LLWStringUtil::toLower( pItem->mLabel );
		aParentMenu->mChildren.push_back( pItem );

		LLMenuItemBranchGL *pBranch = dynamic_cast< LLMenuItemBranchGL* >( pMenu );
		if( pBranch )
			collectChildren( pBranch->getBranch(), pItem );
	}

}

void LLStatusBar::collectSearchableItems()
{
	mSearchData.reset( new ll::statusbar::SearchData );
	ll::statusbar::SearchableItemPtr pItem( new ll::statusbar::SearchableItem );
	mSearchData->mRootMenu = pItem;
	collectChildren( gMenuBarView, pItem );
}

void LLStatusBar::updateMenuSearchVisibility(const LLSD& data)
{
	bool visible = data.asBoolean();
	mSearchPanel->setVisible(visible);
	if (!visible)
	{
		mFilterEdit->setText(LLStringUtil::null);
		onUpdateFilterTerm();
	}
	else
	{
		updateMenuSearchPosition();
	}
}

void LLStatusBar::updateMenuSearchPosition()
{
	const S32 HPAD = 12;
	LLRect balanceRect = mBoxBalance->getRect();
	LLRect searchRect = mSearchPanel->getRect();
	S32 w = searchRect.getWidth();
	searchRect.mLeft = balanceRect.mLeft - w - HPAD;
	searchRect.mRight = searchRect.mLeft + w;
	mSearchPanel->setShape( searchRect );
}

void LLStatusBar::updateBalancePanelPosition()
{
    // Resize the L$ balance background to be wide enough for your balance plus the buy button
    const S32 HPAD = 24;
    LLRect balance_rect = mBoxBalance->getTextBoundingRect();
    LLRect buy_rect = getChildView("buyL")->getRect();
    LLRect shop_rect = getChildView("goShop")->getRect();
    LLView* balance_bg_view = getChildView("balance_bg");
    LLRect balance_bg_rect = balance_bg_view->getRect();
    balance_bg_rect.mLeft = balance_bg_rect.mRight - (buy_rect.getWidth() + shop_rect.getWidth() + balance_rect.getWidth() + HPAD);
    balance_bg_view->setShape(balance_bg_rect);
}


// Implements secondlife:///app/balance/request to request a L$ balance
// update via UDP message system. JC
class LLBalanceHandler : public LLCommandHandler
{
public:
	// Requires "trusted" browser/URL source
	LLBalanceHandler() : LLCommandHandler("balance", UNTRUSTED_BLOCK) { }
	bool handle(const LLSD& tokens, const LLSD& query_map, LLMediaCtrl* web) override
	{
		if (tokens.size() == 1
			&& tokens[0].asString() == "request")
		{
			LLStatusBar::sendMoneyBalanceRequest();
			return true;
		}
		return false;
	}
};
// register with command dispatch system
LLBalanceHandler gBalanceHandler;
