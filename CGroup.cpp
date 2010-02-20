#include "CGroup.h"

#include <sstream>
#include <iostream>
#include <string>

#include "CAI.h"
#include "CUnit.h"
#include "CUnitTable.h"
#include "CTaskHandler.h"

int CGroup::counter = 0;

void CGroup::addUnit(CUnit &unit) {
	LOG_II("CGroup::add " << unit)

	if (unit.group == this)
		return; // already registered

	recalcProperties(&unit);

	units[unit.key] = &unit;
	unit.reg(*this);
	// NOTE: unit can only exist in one and only group
	if (unit.group)
		unit.group->remove(unit);
	assert(unit.group == NULL);
	unit.group = this;

	// TODO: if group is busy invoke new unit to community process?
}

void CGroup::remove() {
	LOG_II("CGroup::remove " << (*this))

	// NOTE: removal order below is important
		
	std::list<ARegistrar*>::iterator j = records.begin();
	while(j != records.end()) {
		ARegistrar *regobj = *j; j++;
		// remove from CEconomy, CPathfinder, ATask
		regobj->remove(*this);
	}
	
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++) {
		i->second->unreg(*this);
		i->second->group = NULL;
	}
	units.clear();
	
	//assert(records.empty());

	// TODO: we can remove the following line when we're sure CMilitary,
	// CEconomy and CPathfinder removes their links from CGroup.records
	records.clear();
}

void CGroup::remove(ARegistrar &unit) {
	LOG_II("CGroup::remove unit(" << unit.key << ")")

	assert(units.find(unit.key) != units.end());
	
	CUnit *unit2 = units[unit.key];
	unit2->group = NULL;
	unit.unreg(*this);
	units.erase(unit.key);

	/* If no more units remain in this group, remove the group */
	if (units.empty()) {
		remove();
	}
	else {
		/* Recalculate properties of the current group */
		recalcProperties(NULL, true);
		std::map<int, CUnit*>::iterator i;
		for (i = units.begin(); i != units.end(); i++) {
			recalcProperties(i->second);
		}
	}
}

void CGroup::reclaim(int entity) {
	float3 pos = ERRORVECTOR;
	const UnitDef* ud = ai->cbc->GetUnitDef(entity);
	if (ud) {
		if (ud->isFeature)
			pos = ai->cb->GetFeaturePos(entity);
	}
	else
		pos = ai->cb->GetFeaturePos(entity);

	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++) {
		if (i->second->def->canReclaim) {
			if (pos.x < 0)
				i->second->reclaim(entity);
			else			
				i->second->reclaim(pos, 16.0f);
		}
	}
}

void CGroup::abilities(bool on) {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++) {
		if (i->second->def->canCloak)
			i->second->cloak(on);
	}
}

void CGroup::micro(bool on) {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++)
		i->second->micro(on);
}

bool CGroup::isMicroing() {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++) {
		if (i->second->isMicroing())
			return true;
	}
	return false;
}

bool CGroup::isIdle() {
	bool idle = true;
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++) {
		if (!ai->unittable->idle[i->second->key]) {
			idle = false;
			break;
		}
	}
	return idle;
}

void CGroup::reset() {
	recalcProperties(NULL, true);
	busy = false;
	micro(false);
	abilities(false);
	units.clear();
	records.clear();
}

void CGroup::recalcProperties(CUnit *unit, bool reset)
{
	if(reset) {
		strength   = 0.0f;
		speed      = MAX_FLOAT;
		size       = 0;
		buildSpeed = 0.0f;
		range      = 0.0f;
		buildRange = 0.0f;
		los        = 0.0f;
		busy       = false;
		maxSlope   = 1.0f;
		moveType   = -1; // emulate NONE
		techlvl    = 1;
    }

	if(unit == NULL)
		return;

    if (unit->builtBy > 0) {
		techlvl = std::max<int>(techlvl, unit->techlvl);
	}

	// NOTE: aircraft & static units do not have movedata
	MoveData *md = ai->cb->GetUnitDef(unit->key)->movedata;
    if (md) {
    	if (md->maxSlope <= maxSlope) {
			// TODO: rename moveType into pathType because this is not the same
			moveType = md->pathType;
			maxSlope = md->maxSlope;
		}
	}
		
	strength += ai->cb->GetUnitPower(unit->key);
	buildSpeed += unit->def->buildSpeed;
	size += FOOTPRINT2REAL * std::max<int>(unit->def->xsize, unit->def->zsize);
	// FIXME: why 0.7 and 1.5?
	range = std::max<float>(ai->cb->GetUnitMaxRange(unit->key)*0.7f, range);
	buildRange = std::max<float>(unit->def->buildDistance*1.5f, buildRange);
	speed = std::min<float>(ai->cb->GetUnitSpeed(unit->key), speed);
	los = std::max<float>(unit->def->losRadius, los);
}

void CGroup::merge(CGroup &group) {
	std::map<int, CUnit*>::iterator i = group.units.begin();
	// NOTE: "group" will automatically be removed when last unit is transferred
	while(i != group.units.end()) {
		CUnit *unit = i->second; i++;
		assert(unit->group == &group);
		addUnit(*unit);
	}	
}

float3 CGroup::pos() {
	std::map<int, CUnit*>::iterator i;
	float3 pos(0.0f, 0.0f, 0.0f);

	for (i = units.begin(); i != units.end(); i++)
		pos += ai->cb->GetUnitPos(i->first);

	pos /= units.size();

	return pos;
}

int CGroup::maxLength() {
	return size + 200;
}

void CGroup::assist(ATask &t) {
	switch(t.t) {
		case BUILD: {
			CTaskHandler::BuildTask *task = dynamic_cast<CTaskHandler::BuildTask*>(&t);
			CUnit *unit  = task->group->units.begin()->second;
			guard(unit->key);
			break;
		}

		case ATTACK: {
			// TODO: Calculate the flanking pos and attack from there
			CTaskHandler::AttackTask *task = dynamic_cast<CTaskHandler::AttackTask*>(&t);
			attack(task->target);
			break;
		}

		case FACTORY_BUILD: {
			CTaskHandler::FactoryTask *task = dynamic_cast<CTaskHandler::FactoryTask*>(&t);
			CUnit *unit  = task->group->units.begin()->second;
			guard(unit->key);
			break;
		}

		default: return;
	}
}

void CGroup::move(float3 &pos, bool enqueue) {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++)
		i->second->move(pos, enqueue);
}

void CGroup::wait() {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++)
		i->second->wait();
}

void CGroup::unwait() {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++)
		i->second->unwait();
}

void CGroup::attack(int target, bool enqueue) {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++)
		i->second->attack(target, enqueue);
}

void CGroup::build(float3 &pos, UnitType *ut) {
	std::map<int, CUnit*>::iterator alpha, i;
	alpha = units.begin();
	if (alpha->second->build(ut, pos)) {
		for (i = ++alpha; i != units.end(); i++)
			i->second->guard(alpha->first);
	}
}

void CGroup::stop() {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++)
		i->second->stop();
}

void CGroup::guard(int target, bool enqueue) {
	std::map<int, CUnit*>::iterator i;
	for (i = units.begin(); i != units.end(); i++)
		i->second->guard(target, enqueue);
}

bool CGroup::canReach(float3 &pos) {
	// TODO: what movetype should we use?
	return true;
}

bool CGroup::canAttack(int uid) {
	// TODO: if at least one unit can shoot target then return true
	return true;
}

bool CGroup::canAdd(CUnit *unit) {
	// TODO:
	return true;
}
		
bool CGroup::canMerge(CGroup *group) {
	/* TODO: can't merge: 
	- static vs mobile
	- water with non-water
	- underwater with hovers?
	- builders with non-builders?
	- nukes with non-nukes
	- lrpc with non-lrpc?
	*/
	return true;
}

CUnit* CGroup::firstUnit() {
	if (units.empty())
		return NULL;
	return units.begin()->second;
}


std::ostream& operator<<(std::ostream &out, const CGroup &group) {
	std::stringstream ss;
	ss << "Group(" << group.key << "):" << " amount(" << group.units.size() << ") [";
	std::map<int, CUnit*>::const_iterator i = group.units.begin();
	for (i = group.units.begin(); i != group.units.end(); i++) {
		ss << (*i->second) << ", ";
	}
	std::string s = ss.str();
	s = s.substr(0, s.size()-2);
	s += "]";
	out << s;
	return out;
}
