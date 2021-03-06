#pragma once
#include "../lib/GameConstants.h"
#include "../lib/int3.h"
#include "../lib/NetPacks.h"

class CGObjectInstance;
class CGHeroInstance;
class CArmedInstance;
struct CPack;
class CGameHandler;
class CObjectVisitQuery;
struct TryMoveHero;
class CQuery;

typedef si32 TQueryID;
typedef shared_ptr<CQuery> QueryPtr;

// This class represents any kind of prolonged interaction that may need to do something special after it is over.
// It does not necessarily has to be "query" requiring player action, it can be also used internally within server.
// Examples:
// - all kinds of blocking dialog windows 
// - battle 
// - object visit
// - hero movement
// Queries can cause another queries, forming a stack of queries for each player. Eg: hero movement -> object visit -> dialog.
class CQuery
{
protected:
	void addPlayer(PlayerColor color);
public:
	std::vector<PlayerColor> players; //players that are affected (often "blocked") by query
	TQueryID queryID;

	CQuery(void);


	virtual bool blocksPack(const CPack *pack) const; //query can block attempting actions by player. Eg. he can't move hero during the battle.

	virtual bool endsByPlayerAnswer() const; //query is removed after player gives answer (like dialogs)
	virtual void onRemoval(CGameHandler *gh, PlayerColor color); //called after query is removed from stack
	virtual void onExposure(CGameHandler *gh, QueryPtr topQuery);//called when query immediately above is removed and this is exposed (becomes top)
	virtual std::string toString() const;

	virtual void notifyObjectAboutRemoval(const CObjectVisitQuery &objectVisit) const;

	virtual ~CQuery(void);


	template <typename Handler> void serialize(Handler &h, const int version)
	{
		h & players & queryID;
	}
};

std::ostream &operator<<(std::ostream &out, const CQuery &query);
std::ostream &operator<<(std::ostream &out, QueryPtr query);

//Created when hero visits object.
//Removed when query above is resolved (or immediately after visit if no queries were created)
class CObjectVisitQuery : public CQuery
{
public:
	const CGObjectInstance *visitedObject;
	const CGHeroInstance *visitingHero;
	int3 tile; //may be different than hero pos -> eg. visit via teleport
	bool removeObjectAfterVisit;

	CObjectVisitQuery(const CGObjectInstance *Obj, const CGHeroInstance *Hero, int3 Tile);

	virtual bool blocksPack(const CPack *pack) const OVERRIDE;
	virtual void onRemoval(CGameHandler *gh, PlayerColor color) OVERRIDE;
	virtual void onExposure(CGameHandler *gh, QueryPtr topQuery) OVERRIDE;
};

class CBattleQuery : public CQuery
{
public:
	std::array<const CArmedInstance *,2> belligerents;

	const BattleInfo *bi;
	boost::optional<BattleResult> result;

	CBattleQuery();
	CBattleQuery(const BattleInfo *Bi); //TODO
	virtual void notifyObjectAboutRemoval(const CObjectVisitQuery &objectVisit) const OVERRIDE;
	virtual bool blocksPack(const CPack *pack) const OVERRIDE;
	virtual void onRemoval(CGameHandler *gh, PlayerColor color) OVERRIDE;
};

//Created when hero attempts move and something happens
//(not necessarily position change, could be just an object interaction).
class CHeroMovementQuery : public CQuery
{
public:
	TryMoveHero tmh;
	bool visitDestAfterVictory; //if hero moved to guarded tile and it should be visited once guard is defeated
	const CGHeroInstance *hero;

	virtual void onExposure(CGameHandler *gh, QueryPtr topQuery);

	CHeroMovementQuery(const TryMoveHero &Tmh, const CGHeroInstance *Hero, bool VisitDestAfterVictory = false);
};

class CDialogQuery : public CQuery
{
public:
	boost::optional<ui32> answer;
	virtual bool endsByPlayerAnswer() const OVERRIDE;
	virtual bool blocksPack(const CPack *pack) const OVERRIDE;
};

class CGarrisonDialogQuery : public CDialogQuery
{
public:
	std::array<const CArmedInstance *,2> exchangingArmies;

	CGarrisonDialogQuery(const CArmedInstance *up, const CArmedInstance *down);
	virtual void notifyObjectAboutRemoval(const CObjectVisitQuery &objectVisit) const OVERRIDE;
	virtual bool blocksPack(const CPack *pack) const OVERRIDE;
};

//yes/no and component selection dialogs
class CBlockingDialogQuery : public CDialogQuery
{
public:
	BlockingDialog bd; //copy of pack... debug purposes

	CBlockingDialogQuery(const BlockingDialog &bd);

	virtual void notifyObjectAboutRemoval(const CObjectVisitQuery &objectVisit) const OVERRIDE;
};

class CHeroLevelUpDialogQuery : public CDialogQuery
{
public:
	CHeroLevelUpDialogQuery(const HeroLevelUp &Hlu);

	virtual void onRemoval(CGameHandler *gh, PlayerColor color) OVERRIDE;

	HeroLevelUp hlu;
};


class CCommanderLevelUpDialogQuery : public CDialogQuery
{
public:
	CCommanderLevelUpDialogQuery(const CommanderLevelUp &Clu);

	virtual void onRemoval(CGameHandler *gh, PlayerColor color) OVERRIDE;

	CommanderLevelUp clu;
};

struct Queries
{
private:
	void addQuery(PlayerColor player, QueryPtr query);
	void popQuery(PlayerColor player, QueryPtr query);

	std::map<PlayerColor, std::vector<QueryPtr>> queries; //player => stack of queries

public:
	CGameHandler *gh;
	static boost::mutex mx;

	void addQuery(QueryPtr query);
	void popQuery(const CQuery &query);
	void popQuery(QueryPtr query);
	void popIfTop(const CQuery &query); //removes this query if it is at the top (otherwise, do nothing)
	void popIfTop(QueryPtr query); //removes this query if it is at the top (otherwise, do nothing)

	QueryPtr topQuery(PlayerColor player);

	std::vector<shared_ptr<const CQuery>> allQueries() const;
	std::vector<shared_ptr<CQuery>> allQueries();
	//void removeQuery

};
