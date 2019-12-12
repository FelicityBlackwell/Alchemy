/** 
 * @file llgroupmgr.h
 * @brief Manager for aggregating all client knowledge for specific groups
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
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

#ifndef LL_LLGROUPMGR_H
#define LL_LLGROUPMGR_H

#include "lluuid.h"
#include "lleventcoro.h"
#include "llcoros.h"

// Forward Declarations
class LLMessageSystem;
class LLGroupRoleData;
class LLGroupMgr;
enum class LLRoleChangeType;
enum class LLRoleMemberChangeType;

enum LLGroupChange
{
	GC_PROPERTIES,
	GC_MEMBER_DATA,
	GC_ROLE_DATA,
	GC_ROLE_MEMBER_DATA,
	GC_TITLES,
	GC_BANLIST,
	GC_ALL
};

constexpr U32 GB_MAX_BANNED_AGENTS = 500;

class LLGroupMgrObserver
{
public:
	LLGroupMgrObserver(const LLUUID& id) : mID(id){};
	LLGroupMgrObserver() : mID(LLUUID::null){};
	virtual ~LLGroupMgrObserver() = default;
	virtual void changed(LLGroupChange gc) = 0;
	const LLUUID& getID() const { return mID; }

protected:
	LLUUID	mID;
};

class LLParticularGroupObserver
{
public:
	virtual ~LLParticularGroupObserver() = default;
	virtual void changed(const LLUUID& group_id, LLGroupChange gc) = 0;
};

class LLGroupMemberData
{
friend class LLGroupMgrGroupData;

public:
	typedef std::map<LLUUID,LLGroupRoleData*> role_list_t;
	
	LLGroupMemberData(const LLUUID& id, 
						S32 contribution,
						U64 agent_powers,
                        std::string title,
                        std::string online_status,
						BOOL is_owner);

	~LLGroupMemberData();

	const LLUUID& getID() const { return mID; }
	S32 getContribution() const { return mContribution; }
	U64	getAgentPowers() const { return mAgentPowers; }
	BOOL isOwner() const { return mIsOwner; }
	const std::string& getTitle() const { return mTitle; }
	const std::string& getOnlineStatus() const { return mOnlineStatus; }
	void addRole(const LLUUID& role, LLGroupRoleData* rd);
	bool removeRole(const LLUUID& role);
	void clearRoles() { mRolesList.clear(); };
	role_list_t::iterator roleBegin() { return mRolesList.begin(); }
	role_list_t::iterator roleEnd() { return mRolesList.end(); }

	BOOL isInRole(const LLUUID& role_id) { return (mRolesList.find(role_id) != mRolesList.end()); }

private:
	LLUUID	mID;
	S32		mContribution;
	U64		mAgentPowers;
	std::string	mTitle;
	std::string	mOnlineStatus;
	BOOL	mIsOwner;
	role_list_t mRolesList;
};

struct LLRoleData
{
    LLRoleData();
	LLRoleData(const LLRoleData& rd)
	: 	mRoleName(rd.mRoleName),
		mRoleTitle(rd.mRoleTitle),
		mRoleDescription(rd.mRoleDescription),
		mRolePowers(rd.mRolePowers),
		mChangeType(rd.mChangeType) { }

	std::string mRoleName;
	std::string mRoleTitle;
	std::string mRoleDescription;
	U64	mRolePowers;
	LLRoleChangeType mChangeType;
};

class LLGroupRoleData
{
friend class LLGroupMgrGroupData;

public:
	LLGroupRoleData(const LLUUID& role_id, 
					const std::string& role_name,
					const std::string& role_title,
					const std::string& role_desc,
					const U64 role_powers,
					const S32 member_count);
	
	LLGroupRoleData(const LLUUID& role_id, 
					LLRoleData role_data,
					const S32 member_count);

	~LLGroupRoleData() = default;

	const LLUUID& getID() const { return mRoleID; }

	const uuid_vec_t& getRoleMembers() const { return mMemberIDs; }
	S32 getMembersInRole(uuid_vec_t members, BOOL needs_sort = TRUE);
	S32 getTotalMembersInRole() const { return mMemberCount ? mMemberCount : mMemberIDs.size(); } //FIXME: Returns 0 for Everyone role when Member list isn't yet loaded, see MAINT-5225

	LLRoleData getRoleData() const { return mRoleData; }
	void setRoleData(LLRoleData data) { mRoleData = data; }
	
	void addMember(const LLUUID& member);
	bool removeMember(const LLUUID& member);
	void clearMembers();

	const uuid_vec_t::const_iterator getMembersBegin() const
	{ return mMemberIDs.begin(); }

	const uuid_vec_t::const_iterator getMembersEnd() const
	{ return mMemberIDs.end(); }


protected:
	LLGroupRoleData()
	: mMemberCount(0), mMembersNeedsSort(FALSE) {}

	LLUUID mRoleID;
	LLRoleData	mRoleData;

	uuid_vec_t mMemberIDs;
	S32	mMemberCount;

private:
	BOOL mMembersNeedsSort;
};

struct LLRoleMemberChange
{
    LLRoleMemberChange();
	LLRoleMemberChange(const LLUUID& role, const LLUUID& member, LLRoleMemberChangeType change)
		: mRole(role), mMember(member), mChange(change) { }
	LLRoleMemberChange(const LLRoleMemberChange& rc)
		: mRole(rc.mRole), mMember(rc.mMember), mChange(rc.mChange) { }
	LLUUID mRole;
	LLUUID mMember;
	LLRoleMemberChangeType mChange;
};

typedef std::pair<LLUUID,LLUUID> lluuid_pair;

struct lluuid_pair_less
{
	bool operator()(const lluuid_pair& lhs, const lluuid_pair& rhs) const
	{
		return lhs.first == rhs.first ? lhs.second < rhs.second : lhs.first < rhs.first;
	}
};


struct LLGroupBanData
{
	LLDate mBanDate;
	// TODO: std:string ban_reason;
};


struct LLGroupTitle
{
	std::string mTitle;
	LLUUID		mRoleID;
	BOOL		mSelected;
};

class LLGroupMgrGroupData
{
friend class LLGroupMgr;

public:
	LLGroupMgrGroupData(const LLUUID& id);
	~LLGroupMgrGroupData();

	const LLUUID& getID() const { return mID; }

	BOOL getRoleData(const LLUUID& role_id, LLRoleData& role_data);
	void setRoleData(const LLUUID& role_id, LLRoleData role_data);
	void createRole(const LLUUID& role_id, LLRoleData role_data);
	void deleteRole(const LLUUID& role_id);
	BOOL pendingRoleChanges();

	void addRolePower(const LLUUID& role_id, U64 power);
	void removeRolePower(const LLUUID& role_id, U64 power);
	U64  getRolePowers(const LLUUID& role_id);

	void removeData();
	void removeRoleData();
	void removeMemberData();
	void removeRoleMemberData();

	bool changeRoleMember(const LLUUID& role_id, const LLUUID& member_id, LLRoleMemberChangeType rmc);
	void recalcAllAgentPowers();
	void recalcAgentPowers(const LLUUID& agent_id);

	bool isMemberDataComplete() const { return mMemberDataComplete; }
	bool isRoleDataComplete() const { return mRoleDataComplete; }
	bool isRoleMemberDataComplete() const { return mRoleMemberDataComplete; }
	bool isGroupPropertiesDataComplete() const { return mGroupPropertiesDataComplete; }

	bool isMemberDataPending() { return mMemberRequestID.notNull(); }
	bool isRoleDataPending() { return mRoleDataRequestID.notNull(); }
	bool isRoleMemberDataPending() { return (mRoleMembersRequestID.notNull() || mPendingRoleMemberRequest); }
	bool isGroupTitlePending() { return mTitlesRequestID.notNull(); }

	bool isSingleMemberNotOwner();

	F32 getAccessTime() const { return mAccessTime; }
	void setAccessed();

	const LLUUID& getMemberVersion() const { return mMemberVersion; }

	void clearBanList() { mBanList.clear(); }
	const LLGroupBanData& getBanEntry(const LLUUID& ban_id) { return mBanList[ban_id]; }
	
	void createBanEntry(const LLUUID& ban_id, const LLGroupBanData& ban_data = LLGroupBanData());
	void removeBanEntry(const LLUUID& ban_id);
	void banMemberById(const LLUUID& participant_uuid);

	typedef	std::map<LLUUID,LLGroupMemberData*> member_list_t;
	typedef	std::map<LLUUID,LLGroupRoleData*> role_list_t;
	typedef std::map<lluuid_pair,LLRoleMemberChange,lluuid_pair_less> change_map_t;
	typedef std::map<LLUUID,LLRoleData> role_data_map_t;
	typedef std::map<LLUUID,LLGroupBanData> ban_list_t;

	member_list_t		mMembers;
	role_list_t			mRoles;
	change_map_t		mRoleMemberChanges;
	role_data_map_t		mRoleChanges;
	ban_list_t			mBanList;

	std::vector<LLGroupTitle> mTitles;

	LLUUID				mID;
	LLUUID				mOwnerRole;
	std::string			mName;
	std::string			mCharter;
	BOOL				mShowInList;
	LLUUID				mInsigniaID;
	LLUUID				mFounderID;
	BOOL				mOpenEnrollment;
	S32					mMembershipFee;
	BOOL				mAllowPublish;
	BOOL				mListInProfile;
	BOOL				mMaturePublish;
	BOOL				mChanged;
	S32					mMemberCount;
	S32					mRoleCount;

	bool				mPendingBanRequest;
	LLUUID				mPendingBanMemberID;

protected:
	void sendRoleChanges();
	void cancelRoleChanges();

private:
	LLUUID				mMemberRequestID;
	LLUUID				mRoleDataRequestID;
	LLUUID				mRoleMembersRequestID;
	LLUUID				mTitlesRequestID;
	U32					mReceivedRoleMemberPairs;

	bool				mMemberDataComplete;
	bool				mRoleDataComplete;
	bool				mRoleMemberDataComplete;
	bool				mGroupPropertiesDataComplete;

	bool				mPendingRoleMemberRequest;
	F32					mAccessTime;

	// Generate a new ID every time mMembers
	LLUUID				mMemberVersion;
};

struct LLRoleAction
{
	std::string mName;
	std::string mDescription;
	std::string mLongDescription;
	U64 mPowerBit;
};

struct LLRoleActionSet
{
	LLRoleActionSet();
	~LLRoleActionSet();
	LLRoleAction* mActionSetData;
	std::vector<LLRoleAction*> mActions;
};

class LLGroupMgr final : public LLSingleton<LLGroupMgr>
{
	LLSINGLETON(LLGroupMgr);
	~LLGroupMgr();
	LOG_CLASS(LLGroupMgr);
	
public:
	enum EBanRequestType
	{
		REQUEST_GET = 0,
		REQUEST_POST,
		REQUEST_PUT,
		REQUEST_DEL
	};

	enum EBanRequestAction
	{
		BAN_NO_ACTION	= 0,
		BAN_CREATE		= 1,
		BAN_DELETE		= 2,
		BAN_UPDATE		= 4
	};

	void addObserver(LLGroupMgrObserver* observer);
	void addObserver(const LLUUID& group_id, LLParticularGroupObserver* observer);
	void removeObserver(LLGroupMgrObserver* observer);
	void removeObserver(const LLUUID& group_id, LLParticularGroupObserver* observer);
	LLGroupMgrGroupData* getGroupData(const LLUUID& id);

	void sendGroupPropertiesRequest(const LLUUID& group_id);
	void sendGroupRoleDataRequest(const LLUUID& group_id);
	void sendGroupRoleMembersRequest(const LLUUID& group_id);
	void sendGroupMembersRequest(const LLUUID& group_id);
	void sendGroupTitlesRequest(const LLUUID& group_id);
	void sendGroupTitleUpdate(const LLUUID& group_id, const LLUUID& title_role_id);
	void sendUpdateGroupInfo(const LLUUID& group_id);
	void sendGroupRoleMemberChanges(const LLUUID& group_id);
	void sendGroupRoleChanges(const LLUUID& group_id);

	static void sendCreateGroupRequest(const std::string& name,
									   const std::string& charter,
									   U8 show_in_list,
									   const LLUUID& insignia,
									   S32 membership_fee,
									   BOOL open_enrollment,
									   BOOL allow_publish,
									   BOOL mature_publish);

	static void sendGroupMemberJoin(const LLUUID& group_id);
	static void sendGroupMemberInvites(const LLUUID& group_id, std::map<LLUUID,LLUUID>& role_member_pairs);
	static void sendGroupMemberEjects(const LLUUID& group_id,
									  uuid_vec_t& member_ids);
	
	void sendGroupBanRequest(EBanRequestType request_type, 
									const LLUUID& group_id,	
									U32 ban_action = BAN_NO_ACTION,
									const uuid_vec_t &ban_list = uuid_vec_t());


	void sendCapGroupMembersRequest(const LLUUID& group_id);

	void cancelGroupRoleChanges(const LLUUID& group_id);

	static void processGroupPropertiesReply(LLMessageSystem* msg, void** data);
	static void processGroupMembersReply(LLMessageSystem* msg, void** data);
	static void processGroupRoleDataReply(LLMessageSystem* msg, void** data);
	static void processGroupRoleMembersReply(LLMessageSystem* msg, void** data);
	static void processGroupTitlesReply(LLMessageSystem* msg, void** data);
	static void processCreateGroupReply(LLMessageSystem* msg, void** data);
	static void processJoinGroupReply(LLMessageSystem* msg, void ** data);
	static void processEjectGroupMemberReply(LLMessageSystem* msg, void ** data);
	static void processLeaveGroupReply(LLMessageSystem* msg, void ** data);

	static bool parseRoleActions(const std::string& xml_filename);

	std::vector<LLRoleActionSet*> mRoleActionSets;

	static void debugClearAllGroups(void*);
	void clearGroups();
	void clearGroupData(const LLUUID& group_id);

private:
    void groupMembersRequestCoro(std::string url, LLUUID groupId);
    void processCapGroupMembersRequest(const LLSD& content);

    void getGroupBanRequestCoro(std::string url, LLUUID groupId);
    void postGroupBanRequestCoro(std::string url, LLUUID groupId, U32 action, uuid_vec_t banList, bool update);

    static void processGroupBanRequest(const LLSD& content);

	void notifyObservers(LLGroupChange gc);
	void addGroup(LLGroupMgrGroupData* group_datap);
	LLGroupMgrGroupData* createGroupData(const LLUUID &id);

	typedef std::multimap<LLUUID,LLGroupMgrObserver*> observer_multimap_t;
	observer_multimap_t mObservers;

	typedef std::map<LLUUID, LLGroupMgrGroupData*> group_map_t;
	group_map_t mGroups;

	typedef std::set<LLParticularGroupObserver*> observer_set_t;
	typedef std::map<LLUUID,observer_set_t> observer_map_t;
	observer_map_t mParticularObservers;

    bool mMemberRequestInFlight;
};


#endif
