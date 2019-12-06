#include "MyStrategy.hpp"
#include <math.h>

MyStrategy::MyStrategy() {}

double distanceSqr(Vec2Double a, Vec2Double b) {
	return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y);
}

bool checkforobstacles(const Vec2Double &a, const Vec2Double &b, const Game &game);

UnitAction MyStrategy::getAction(const Unit &unit, const Game &game,
								 Debug &debug) {
	bool jump = true;
	bool reload = false;
	bool shoot = true;
	bool swapWeapon = false;
	bool plantMine = false;
	double velocity; 
	int health_limit = 70;

	// Locate the nearest Pistol, Launcher, AR.
	const LootBox *nearestWeapon = nullptr;
	const LootBox *nearestPistol = nullptr;
	const LootBox *nearestLauncher = nullptr;
	const LootBox *nearestAR = nullptr;
	for (const LootBox &lootBox : game.lootBoxes) {
		if (std::dynamic_pointer_cast<Item::Weapon>(lootBox.item)) {
			if(lootBox.Weapon.item->weaponType == WeaponType::PISTOL){
				if (nearestPistol == nullptr ||distanceSqr(unit.position, lootBox.position) <
					distanceSqr(unit.position, nearestPistol->position)) {
					nearestPistol = &lootBox;
				}
			}
			else if(lootBox.Weapon.item->weaponType == WeaponType::ROCKET_LAUNCHER){
				if (nearestLauncher == nullptr ||distanceSqr(unit.position, lootBox.position) <
					distanceSqr(unit.position, nearestLauncher->position)) {
					nearestLauncher = &lootBox;
				}
			}
			else{
				if (nearestAR == nullptr ||distanceSqr(unit.position, lootBox.position) <
					distanceSqr(unit.position, nearestAR->position)) {
					nearestAR = &lootBox;
				}
			}
		}
	}

	// Locate the nearest Weapon.
	nearestWeapon = nearestAR;
	if (distanceSqr(unit.position, nearestLauncher->position) <
		distanceSqr(unit.position, nearestWeapon->position)) {
		nearestWeapon = nearestLauncher;
	}
	if (distanceSqr(unit.position, nearestPistol->position) <
		distanceSqr(unit.position, nearestWeapon->position)) {
		nearestWeapon = nearestPistol;
	}

	// Locate the nearest enemy.
	const Unit *nearestEnemy = nullptr;
	for (const Unit &other : game.units) {
		if (other.playerId != unit.playerId) {
			if (nearestEnemy == nullptr || distanceSqr(unit.position, other.position) <
			 	distanceSqr(unit.position, nearestEnemy->position)) {
				nearestEnemy = &other;
			}	
		}
	}

	// Locate nearest HealthPack.
	const LootBox *nearestHealthPack = nullptr;
	for (const LootBox &lootBox : game.lootBoxes) {
		if (std::dynamic_pointer_cast<Item::HealthPack>(lootBox.item)) {
			if (nearestHealthPack == nullptr ||distanceSqr(unit.position, lootBox.position) <
				distanceSqr(unit.position, nearestHealthPack->position)) {
				nearestHealthPack = &lootBox;
			}
		}
	}

	// Fix target position.
	Vec2Double targetPos = nearestWeapon->position;
	// No weapon, then go to nearest the weapon.
	if (unit.weapon == nullptr && nearestWeapon != nullptr) {
		targetPos = nearestWeapon->position;
	} 
	// Have a weapon, but not an AR, go to the nearest AR if there is one.
	else if(unit.weapon != nullptr && nearestAR != nullptr && unit.weapon->typ != WeaponType::ASSAULT_RIFLE){
		targetPos = nearestAR->position;
		swapWeapon = true;
	}
	// Else go to the nearest enemy.
	else if (nearestEnemy != nullptr) {
		targetPos = nearestEnemy->position;
	}
	
	// If health is low, go towards a HealthPack. 
	if(unit.health < health_limit){
		if(nearestHealthPack != nullptr){
			targetPos = nearestHealthPack->position;
			swapWeapon = false;
		}
	}

	debug.draw(CustomData::Log(std::string("Target pos: ") + targetPos.toString()));
	
	Vec2Double aim = Vec2Double(0, 0);
	if (nearestEnemy != nullptr) {
		aim = Vec2Double(nearestEnemy->position.x - unit.position.x, nearestEnemy->position.y - unit.position.y);
	}
	
	jump = targetPos.y > unit.position.y;
	if (targetPos.x > unit.position.x &&
		game.level.tiles[size_t(unit.position.x + 1)][size_t(unit.position.y)] ==
			Tile::WALL) {
		jump = true;
		reload = true;
	}
	if (targetPos.x < unit.position.x &&
		game.level.tiles[size_t(unit.position.x - 1)][size_t(unit.position.y)] ==
			Tile::WALL) {
		jump = true;
		reload = true;
	}
	
	// If enemy is adjacent then fallback.
	if (abs(nearestEnemy->position.x-unit.position.x) == 1 && 
			nearestEnemy->position.y == unit.position.y){
		reload = false;
		plantMine = true;
		//TODO:: Decide where to go after planting the mine.
	}

	velocity = targetPos.x - unit.position.x;
	// Check for obstacles in your aim.
	shoot = checkforobstacles(unit.position, nearestEnemy->position, game);

	// Reload if you are not shooting or magazine is empty.
	if(unit.weapon != nullptr){
		if(!shoot || unit.weapon->magazine == 0){
			reload = true;
		}
	}

	UnitAction action;
	action.velocity = velocity;
	action.jump = jump;
	action.jumpDown = !action.jump;
	action.aim = aim;
	action.shoot = shoot;
	action.reload = reload;
	action.swapWeapon = swapWeapon;
	action.plantMine = plantMine;
	return action;
}

bool checkforobstacles(const Vec2Double &a, const Vec2Double &b, const Game &game){
	//vector<vector<Tile> > v = game.level.tiles;
	
	if(a.x == b.x){
		return true;
	}

	double slope = (b.y-a.y)/(b.x-a.x);

	double curr_x = a.x, curr_y = a.y;
	double change_x = 1/sqrt(1+slope*slope), change_y = abs(slope)/sqrt(1+slope*slope);
	double distance = sqrt((a.x-b.x)*(a.x-b.x) + (a.y-b.y)*(a.y-b.y));

	while(distance>=1){
		if(slope >= 0 && a.x > b.x){
			curr_x -= change_x;
			curr_y -= change_y;
		}
		else if(slope >= 0 && a.x < b.x){
			curr_x += change_x;
			curr_y += change_y;
		}
		else if(slope < 0 && a.x > b.x){
			curr_x -= change_x;
			curr_y += change_y;
		}
		else if(slope < 0 && a.x < b.x){
			curr_x += change_x;
			curr_y -= change_y;
		}
		if(game.level.tiles[size_t(curr_x)][size_t(curr_y)] == Tile::WALL 
		   || game.level.tiles[size_t(curr_x)][size_t(curr_y)] == Tile::PLATFORM)
			return false;
		distance -= 1;
	}
	return true;
}
