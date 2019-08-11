/** 
 * @file llkeyframemotion.h
 * @brief Implementation of LLKeframeMotion class.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#ifndef LL_LLKEYFRAMEMOTION_H
#define LL_LLKEYFRAMEMOTION_H

//-----------------------------------------------------------------------------
// Header files
//-----------------------------------------------------------------------------

#include "llassetstorage.h"
#include "llbboxlocal.h"
#include "llhandmotion.h"
#include "lljointstate.h"
#include "llmotion.h"
#include "llquaternion.h"
#include "v3dmath.h"
#include "v3math.h"
#include "llbvhconsts.h"

#include "absl/container/flat_hash_map.h"

class LLKeyframeDataCache;
class LLVFS;
class LLDataPacker;

#define MIN_REQUIRED_PIXEL_AREA_KEYFRAME (40.f)
#define MAX_CHAIN_LENGTH (4)

const S32 KEYFRAME_MOTION_VERSION = 1;
const S32 KEYFRAME_MOTION_SUBVERSION = 0;

//-----------------------------------------------------------------------------
// class LLKeyframeMotion
//-----------------------------------------------------------------------------
class LLKeyframeMotion :
	public LLMotion
{
	friend class LLKeyframeDataCache;
public:
	// Constructor
	LLKeyframeMotion(const LLUUID &id);

	// Destructor
	virtual ~LLKeyframeMotion();

private:
	// private helper functions to wrap some asserts
	LLPointer<LLJointState>& getJointState(U32 index);
	LLJoint* getJoint(U32 index );
	
public:
	//-------------------------------------------------------------------------
	// functions to support MotionController and MotionRegistry
	//-------------------------------------------------------------------------

	// static constructor
	// all subclasses must implement such a function and register it
	static LLMotion *create(const LLUUID& id);

public:
	//-------------------------------------------------------------------------
	// animation callbacks to be implemented by subclasses
	//-------------------------------------------------------------------------

	// motions must specify whether or not they loop
	BOOL getLoop() override
	{ 
		if (mJointMotionList) return mJointMotionList->mLoop; 
		else return FALSE;
	}

	// motions must report their total duration
	F32 getDuration() override
	{ 
		if (mJointMotionList) return mJointMotionList->mDuration; 
		else return 0.f;
	}

	// motions must report their "ease in" duration
	F32 getEaseInDuration() override
	{ 
		if (mJointMotionList) return mJointMotionList->mEaseInDuration; 
		else return 0.f;
	}

	// motions must report their "ease out" duration.
	F32 getEaseOutDuration() override
	{ 
		if (mJointMotionList) return mJointMotionList->mEaseOutDuration; 
		else return 0.f;
	}

	// motions must report their priority
	LLJoint::JointPriority getPriority() override
	{ 
		if (mJointMotionList) return mJointMotionList->mBasePriority; 
		else return LLJoint::LOW_PRIORITY;
	}

	LLMotionBlendType getBlendType() override { return NORMAL_BLEND; }

	// called to determine when a motion should be activated/deactivated based on avatar pixel coverage
	F32 getMinPixelArea() override { return MIN_REQUIRED_PIXEL_AREA_KEYFRAME; }

	// run-time (post constructor) initialization,
	// called after parameters have been set
	// must return true to indicate success and be available for activation
	LLMotionInitStatus onInitialize(LLCharacter *character) override;

	// called when a motion is activated
	// must return TRUE to indicate success, or else
	// it will be deactivated
	BOOL onActivate() override;

	// called per time step
	// must return TRUE while it is active, and
	// must return FALSE when the motion is completed.
	BOOL onUpdate(F32 time, U8* joint_mask) override;

	// called when a motion is deactivated
	void onDeactivate() override;

	void setStopTime(F32 time) override;

	static void setVFS(LLVFS* vfs) { sVFS = vfs; }

	static void onLoadComplete(LLVFS *vfs,
							   const LLUUID& asset_uuid,
							   LLAssetType::EType type,
							   void* user_data, S32 status, LLExtStat ext_status);

public:
	U32		getFileSize();
	BOOL	serialize(LLDataPacker& dp) const;
	BOOL	deserialize(LLDataPacker& dp, const LLUUID& asset_id);
	BOOL	isLoaded() { return mJointMotionList != nullptr; }
    void	dumpToFile(const std::string& name);


	// setters for modifying a keyframe animation
	void setLoop(BOOL loop);

	F32 getLoopIn() {
		return (mJointMotionList) ? mJointMotionList->mLoopInPoint : 0.f;
	}

	F32 getLoopOut() {
		return (mJointMotionList) ? mJointMotionList->mLoopOutPoint : 0.f;
	}
	
	void setLoopIn(F32 in_point);

	void setLoopOut(F32 out_point);

	void setHandPose(LLHandMotion::eHandPose pose) {
		if (mJointMotionList) mJointMotionList->mHandPose = pose;
	}

	LLHandMotion::eHandPose getHandPose() { 
		return (mJointMotionList) ? mJointMotionList->mHandPose : LLHandMotion::HAND_POSE_RELAXED;
	}

	void setPriority(S32 priority);

	void setEmote(const LLUUID& emote_id);

	void setEaseIn(F32 ease_in);

	void setEaseOut(F32 ease_in);

	F32 getLastUpdateTime() { return mLastLoopedTime; }

	const LLBBoxLocal& getPelvisBBox();

	static void flushKeyframeCache();

protected:
	//-------------------------------------------------------------------------
	// JointConstraintSharedData
	//-------------------------------------------------------------------------
	class JointConstraintSharedData
	{
	public:
		JointConstraintSharedData() :
			mSourceConstraintVolume(0),
			mTargetConstraintVolume(0), 
			mChainLength(0),
			mJointStateIndices(nullptr),
			mEaseInStartTime(0.f), 
			mEaseInStopTime(0.f),
			mEaseOutStartTime(0.f),
			mEaseOutStopTime(0.f),
			mUseTargetOffset(FALSE),
			mConstraintType(CONSTRAINT_TYPE_POINT),
			mConstraintTargetType(CONSTRAINT_TARGET_TYPE_BODY)
		{ };
		~JointConstraintSharedData() { delete [] mJointStateIndices; }

		S32						mSourceConstraintVolume;
		LLVector3				mSourceConstraintOffset;
		S32						mTargetConstraintVolume;
		LLVector3				mTargetConstraintOffset;
		LLVector3				mTargetConstraintDir;
		S32						mChainLength;
		S32*					mJointStateIndices;
		F32						mEaseInStartTime;
		F32						mEaseInStopTime;
		F32						mEaseOutStartTime;
		F32						mEaseOutStopTime;
		BOOL					mUseTargetOffset;
		EConstraintType			mConstraintType;
		EConstraintTargetType	mConstraintTargetType;
	};

	//-----------------------------------------------------------------------------
	// JointConstraint()
	//-----------------------------------------------------------------------------
	class JointConstraint
	{
	public:
		JointConstraint(JointConstraintSharedData* shared_data);
		~JointConstraint();

		JointConstraintSharedData*	mSharedData;
		F32							mWeight;
		F32							mTotalLength;
		LLVector3					mPositions[MAX_CHAIN_LENGTH];
		F32							mJointLengths[MAX_CHAIN_LENGTH];
		F32							mJointLengthFractions[MAX_CHAIN_LENGTH];
		BOOL						mActive;
		LLVector3d					mGroundPos;
		LLVector3					mGroundNorm;
		LLJoint*					mSourceVolume;
		LLJoint*					mTargetVolume;
		F32							mFixupDistanceRMS;
	};

	void applyKeyframes(F32 time);

	void applyConstraints(F32 time, U8* joint_mask);

	void activateConstraint(JointConstraint* constraintp);

	void initializeConstraint(JointConstraint* constraint);

	void deactivateConstraint(JointConstraint *constraintp);

	void applyConstraint(JointConstraint* constraintp, F32 time, U8* joint_mask);

	BOOL	setupPose();

public:
	enum AssetStatus { ASSET_LOADED, ASSET_FETCHED, ASSET_NEEDS_FETCH, ASSET_FETCH_FAILED, ASSET_UNDEFINED };

	enum InterpolationType { IT_STEP, IT_LINEAR, IT_SPLINE };

	//-------------------------------------------------------------------------
	// ScaleKey
	//-------------------------------------------------------------------------
	struct ScaleKey
	{
		F32			mTime = 0.0f;
		LLVector3	mScale;
	};

	//-------------------------------------------------------------------------
	// RotationKey
	//-------------------------------------------------------------------------
	struct RotationKey
	{
		F32				mTime = 0.0f;
		LLQuaternion	mRotation;
	};

	//-------------------------------------------------------------------------
	// PositionKey
	//-------------------------------------------------------------------------
	struct PositionKey
	{
		F32			mTime = 0.0f;
		LLVector3	mPosition;
	};

	//-------------------------------------------------------------------------
	// ScaleCurve
	//-------------------------------------------------------------------------
	struct ScaleCurve
	{
		ScaleCurve() = default;
		~ScaleCurve() = default;
		LLVector3 getValue(F32 time, F32 duration);
		LLVector3 interp(F32 u, ScaleKey& before, ScaleKey& after);

		InterpolationType	mInterpolationType = LLKeyframeMotion::IT_LINEAR;
		S32					mNumKeys = 0;
		typedef std::vector<std::pair<F32, ScaleKey>> key_map_t;
		key_map_t 			mKeys;
		ScaleKey			mLoopInKey;
		ScaleKey			mLoopOutKey;
	};

	//-------------------------------------------------------------------------
	// RotationCurve
	//-------------------------------------------------------------------------
	struct RotationCurve
	{
		RotationCurve() = default;
		~RotationCurve() = default;
		LLQuaternion getValue(F32 time, F32 duration);
		LLQuaternion interp(F32 u, RotationKey& before, RotationKey& after);

		InterpolationType	mInterpolationType = LLKeyframeMotion::IT_LINEAR;
		S32					mNumKeys = 0;
		typedef std::vector<std::pair<F32, RotationKey>> key_map_t;
		key_map_t		mKeys;
		RotationKey		mLoopInKey;
		RotationKey		mLoopOutKey;
	};

	//-------------------------------------------------------------------------
	// PositionCurve
	//-------------------------------------------------------------------------
	struct PositionCurve
	{
		PositionCurve() = default;
		~PositionCurve() = default;
		LLVector3 getValue(F32 time, F32 duration);
		LLVector3 interp(F32 u, PositionKey& before, PositionKey& after);

		InterpolationType	mInterpolationType = LLKeyframeMotion::IT_LINEAR;
		S32					mNumKeys = 0;
		typedef std::vector<std::pair<F32, PositionKey>> key_map_t;
		key_map_t		mKeys;
		PositionKey		mLoopInKey;
		PositionKey		mLoopOutKey;
	};

	//-------------------------------------------------------------------------
	// JointMotion
	//-------------------------------------------------------------------------
	class JointMotion
	{
	public:
		PositionCurve	mPositionCurve;
		RotationCurve	mRotationCurve;
		ScaleCurve		mScaleCurve;
		std::string		mJointName;
		U32				mUsage;
		LLJoint::JointPriority	mPriority;

		void update(LLJointState* joint_state, F32 time, F32 duration);
	};
	
	//-------------------------------------------------------------------------
	// JointMotionList
	//-------------------------------------------------------------------------
	class JointMotionList
	{
	public:
		std::vector<JointMotion*> mJointMotionArray;
		F32						mDuration;
		BOOL					mLoop;
		F32						mLoopInPoint;
		F32						mLoopOutPoint;
		F32						mEaseInDuration;
		F32						mEaseOutDuration;
		LLJoint::JointPriority	mBasePriority;
		LLHandMotion::eHandPose mHandPose;
		LLJoint::JointPriority  mMaxPriority;
		typedef std::list<JointConstraintSharedData*> constraint_list_t;
		constraint_list_t		mConstraints;
		LLBBoxLocal				mPelvisBBox;
		// mEmoteName is a facial motion, but it's necessary to appear here so that it's cached.
		// TODO: LLKeyframeDataCache::getKeyframeData should probably return a class containing 
		// JointMotionList and mEmoteName, see LLKeyframeMotion::onInitialize.
		std::string				mEmoteName; 
	public:
		JointMotionList();
		~JointMotionList();
		U32 dumpDiagInfo();
		JointMotion* getJointMotion(U32 index) const { llassert(index < mJointMotionArray.size()); return mJointMotionArray[index]; }
		U32 getNumJointMotions() const { return mJointMotionArray.size(); }
	};


protected:
	static LLVFS*				sVFS;

	//-------------------------------------------------------------------------
	// Member Data
	//-------------------------------------------------------------------------
	JointMotionList*				mJointMotionList;
	std::vector<LLPointer<LLJointState> > mJointStates;
	LLJoint*						mPelvisp;
	LLCharacter*					mCharacter;
	typedef std::list<JointConstraint*>	constraint_list_t;
	constraint_list_t				mConstraints;
	U32								mLastSkeletonSerialNum;
	F32								mLastUpdateTime;
	F32								mLastLoopedTime;
	AssetStatus						mAssetStatus;
};

class LLKeyframeDataCache
{
public:
	// *FIX: implement this as an actual singleton member of LLKeyframeMotion
	LLKeyframeDataCache(){};
	~LLKeyframeDataCache();

	typedef absl::flat_hash_map<LLUUID, class LLKeyframeMotion::JointMotionList*> keyframe_data_map_t; 
	static keyframe_data_map_t sKeyframeDataMap;

	static void addKeyframeData(const LLUUID& id, LLKeyframeMotion::JointMotionList*);
	static LLKeyframeMotion::JointMotionList* getKeyframeData(const LLUUID& id);

	static void removeKeyframeData(const LLUUID& id);

	//print out diagnostic info
	static void dumpDiagInfo();
	static void clear();
};

#endif // LL_LLKEYFRAMEMOTION_H


