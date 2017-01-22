#ifndef WBOSSMGR_H_
#define WBOSSMGR_H_

#include "Config.h"
#include "GData/NpcGroup.h"

namespace GObject
{
class Player;
class WBossMgr;

struct AttackInfo
{
	AttackInfo(Player* player, UInt32 score) : player(player), score(score) {}
	Player* player;
	UInt32 score;

	const AttackInfo& operator += (const AttackInfo& atk)
	{
		score += atk.score;
		return *this;
	}
};

struct lt
{
	bool operator()(AttackInfo a, AttackInfo b) const
	{
		return a.score <= b.score;
	}
};

typedef std:;set<AttackInfo, lt> AtkInfoType;

class WBoss
{
public:
	WBoss(WBossMgr* mgr, UInt32 id, UInt8 cnt, UInt8 max, UInt16 loc, UInt8 idx)
		: m_id(id), m_count(cnt), m_maxcnt(max), m_loc(loc),
		m_idx(idx), m_disappered(false), m_extra(false), m_last(0), m_appearTime(0), _mgr(mgr),
		_percent(100), _ng(NULL), m_final(false), m_lastHP(0), m_lastAtk(0), m_lastMAtk(0) {}
	~WBoss() {}

	inline void setFinal(bool f) { m_final = f; }
	inline bool isFinal() { return m_final; }
	inline bool isDisappered() const { return m_disappered; }

	inline void setId(UInt32 id) { m_id = id; }
	inline UInt32 getId() const { return m_id;}
	inline UInt32 getApperTime() const { return m_appearTime; }
	inline void setAppearTime(UInt32 t) { m_appearTime = t; }
	bool attack(Player* pl, UInt16 loc, UInt32 id);
	void appear(UInt32 npcid, UInt32 oldid = 0);
	void disapper();
	bool attackWorldBoss(Player * pl, UInt32 npcId, UInt8 expfactor, bool final = false);
	void updateLastDB(UInt32 end);
	const std::string& getName() const { return m_name;}

	inline void setIdx(UInt8 idx) { m_idx = idx;}
	inline UInt8 getIdx() const { return m_idx; }
	inline void setLoc(UInt16 loc) { m_loc = loc; }
	inline UInt16 getLoc() { return m_loc;}
	inline UInt32 getHP()
	{
		if(m_final)
		{
			return _hp[0];
		}

		return 0;
	}

	inline void setHP(UInt32 hp)
	{
		if(!m_final)
			return;
		if(!hp)
			hp = 100;
		_hp[0] = hp;
	}
	inline void setLast(UInt16 last) { m_last = last;}

	void reward(Player* player);
	void getRandList(UInt32 sz, UInt32 num, std::set<UInt32>& ret);
	void flee();

	void sendHpMax(Player* player = NULL);
	void sendHp(Player* player = NULL);
	void sendDmg(UInt32 damage);
	void sendLoc(Player* player = NULL);
	void sendId(Player* player = NULL);
	void sendCount(Player* player = NULL);

	void sendFighteCD(Player* player);
	void sendFighterNum(Player* player = NULL);
	void sendLastTime(Player* player = NULL);
	void sendInspireInfo(Player* player);
	void sendAtkInfo(Player* player = NULL);
	void sendSkipFlag(Player* player);
	void sendSkipBattleReport(Player* player, UInt32 damage, UInt32 exp, bool res);
	void SetDirty(Player* player, bool _isInspire);
	void ReqBossId(Player* player);

private:
	UInt32 m_id;
	UInt8 m_count;
	UInt8 m_maxcnt;
	UInt16 m_loc;
	UInt8 m_idx;
	bool m_disappered;
	bool m_extra;
	UInt16 m_last;
	UInt32 m_appearTime;

	WBosMgr* _mgr;
	UInt8 _percent;
	GData::NpcGroup* _ng;
	std::vector<UInt32> _hp;
	bool m_final;
	AtkInfoType m_atkinfo;
	std::string m_name;

	UInt32 m_lastHP;
	Int32 m_lastAtk;
	Int32 m_lastMAtk;
};

struct Last
{
	UInt16 time;
	UInt32 hp;
	Int32 atk;
	Int32 matk;
};

class WBossMgr
{
public:
	static bool isWorldBoss(UInt32 npcid);

public:
	WBossMgr();
	~WBossMgr()
	{
		if(m_boss)
		{
			delete m_boss;
			m_boss = NULL;
		}
	}

	void initWBoss();
	void process(UInt32 now);
	void appear(UInt32 now);
	void disapper(UInt32 now);
	void attack(Player* pl, UInt16 loc, UInt32 npcid);
	void broadcastTV(UInt32 now);
	void calcNext(UInt32 now);
	void nextDay(UInt32 now);
	void sendDaily(Player* player);
	void setHP(UInt32 hp);
	void sendBossInfo(Player* pl);
	UInt16 fixBossId(UInt16 id, UInt8 idx);
	void fixBossName(UInt16 id, Fighter* fighter, UInt8 idx);

};

}
#endif
