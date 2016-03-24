//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//////////////////////////////////////////////////////////////////////
#include "otpch.h"

#include <string>
#include <sstream>
#include <utility>

#include <boost/tokenizer.hpp>
typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

#include "commands.h"
#include "player.h"
#include "npc.h"
#include "monsters.h"
#include "game.h"
#include "actions.h"
#include "house.h"
#include "ioplayer.h"
#include "tools.h"
#include "ban.h"
#include "configmanager.h"
#include "town.h"
#include "spells.h"
#include "talkaction.h"
#include "movement.h"
#include "spells.h"
#include "weapons.h"
#include "raids.h"

#ifdef __ENABLE_SERVER_DIAGNOSTIC__
#include "outputmessage.h"
#include "connection.h"
#include "status.h"
#include "protocollogin.h"
#endif

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

extern ConfigManager g_config;
extern Actions* g_actions;
extern Monsters g_monsters;
extern BanManager g_bans;
extern Npcs g_npcs;
extern TalkActions* g_talkactions;
extern MoveEvents* g_moveEvents;
extern Spells* g_spells;
extern Weapons* g_weapons;
extern CreatureEvents* g_creatureEvents;
extern Game g_game;

extern bool readXMLInteger(xmlNodePtr p, const char *tag, int &value);

#define ipText(a) (unsigned int)a[0] << "." << (unsigned int)a[1] << "." << (unsigned int)a[2] << "." << (unsigned int)a[3]

//table of commands
s_defcommands Commands::defined_commands[] = {
	{"/s",&Commands::placeNpc},
	{"/m",&Commands::placeMonster},
	{"/summon",&Commands::placeSummon},
	{"/B",&Commands::broadcastMessage},
	{"/t",&Commands::teleportMasterPos},
	{"/c",&Commands::teleportHere},
	{"/i",&Commands::createItemById},
	{"/n",&Commands::createItemByName},
	{"/q",&Commands::subtractMoney},
	{"/reload",&Commands::reloadInfo},
	{"/z",&Commands::testCommand},
	{"/goto",&Commands::teleportTo},
	{"/info",&Commands::getInfo},
	{"/save",&Commands::saveServer},
	{"/closeserver",&Commands::closeServer},
	{"/openserver",&Commands::openServer},
	{"/getonline",&Commands::onlineList},
	{"/a",&Commands::teleportNTiles},
	{"/kick",&Commands::kickPlayer},
	{"/owner",&Commands::setHouseOwner},
	{"/gethouse",&Commands::getHouse},
	{"/town",&Commands::teleportToTown},
	{"/raid",&Commands::forceRaid},
	{"/up",&Commands::goUp},
	{"/down",&Commands::goDown},
	{"/refreshmap",&Commands::refreshMap},
#ifdef __PB_GMINVISIBLE__
	{"/invisible",&Commands::setGmInvisible},
#endif
#ifdef __ENABLE_SERVER_DIAGNOSTIC__
	{"/serverdiag",&Commands::serverDiag},
#endif
	{"/clean",&Commands::cleanMap},
	{"/trace",&Commands::setTrace},
	{"/bc",&Commands::broadcastColor},
	{"/newtype", &Commands::newType},
	{"/b", &Commands::banPlayer},
	
	{"!serverinfo",&Commands::serverInfo},
	{"!buyhouse",&Commands::buyHouse},
	{"!sellhouse",&Commands::sellHouse},
	{"!changesex",&Commands::changeSex},
	{"!online",&Commands::whoIsOnline},
	{"!frags",&Commands::playerKills},
	{"!leavehouse",&Commands::leaveHouse},
};


Commands::Commands()
{
	loaded = false;
	
	//setup command map
	for(uint32_t i = 0; i < sizeof(defined_commands) / sizeof(defined_commands[0]); i++){
		Command* cmd = new Command;
		cmd->loaded = false;
		cmd->accesslevel = 1;
		cmd->f = defined_commands[i].f;
		std::string key = defined_commands[i].name;
		commandMap[key] = cmd;
	}
}

bool Commands::loadXml(const std::string& _datadir)
{
	datadir = _datadir;
	std::string filename = datadir + "XML/commands.xml";

	xmlDocPtr doc = xmlParseFile(filename.c_str());
	if(doc){
		loaded = true;
		xmlNodePtr root, p;
		root = xmlDocGetRootElement(doc);

		if(xmlStrcmp(root->name,(const xmlChar*)"commands") != 0){
			xmlFreeDoc(doc);
			return false;
		}

		std::string strCmd;

		p = root->children;

		while (p){
			if(xmlStrcmp(p->name, (const xmlChar*)"command") == 0){
				if(readXMLString(p, "cmd", strCmd)){
					CommandMap::iterator it = commandMap.find(strCmd);
					int alevel;
					if(it != commandMap.end()){
						if(readXMLInteger(p,"access",alevel)){
							if(!it->second->loaded){
								it->second->accesslevel = alevel;
								it->second->loaded = true;
							}
							else{
								std::cout << "Duplicated command " << strCmd << std::endl;
							}
						}
						else{
							std::cout << "missing access tag for " << strCmd << std::endl;
						}
					}
					else{
						//error
						std::cout << "Unknown command " << strCmd << std::endl;
					}
				}
				else{
					std::cout << "missing cmd." << std::endl;
				}
			}
			p = p->next;
		}
		xmlFreeDoc(doc);
	}

	//
	for(CommandMap::iterator it = commandMap.begin(); it != commandMap.end(); ++it){
		if(it->second->loaded == false){
			std::cout << "Warning: Missing access level for command " << it->first << std::endl;
		}
		//register command tag in game
		g_game.addCommandTag(it->first.substr(0,1));
	}


	return this->loaded;
}

bool Commands::reload()
{
	loaded = false;
	for(CommandMap::iterator it = commandMap.begin(); it != commandMap.end(); ++it){
		it->second->accesslevel = 1;
		it->second->loaded = false;
	}
	g_game.resetCommandTag();
	
	loadXml(datadir);
	return true;
}

bool Commands::exeCommand(Creature* creature, const std::string& cmd)
{
	std::string str_command;
	std::string str_param;

	std::string::size_type loc = cmd.find( ' ', 0 );
	if(loc != std::string::npos && loc >= 0){
		str_command = std::string(cmd, 0, loc);
		str_param = std::string(cmd, (loc+1), cmd.size()-loc-1);
	}
	else {
		str_command = cmd;
		str_param = std::string("");
	}

	//find command
	CommandMap::iterator it = commandMap.find(str_command);
	if(it == commandMap.end()){
		return false;
	}

	Player* player = creature->getPlayer();
	//check access for this command
	if(player && player->getAccessLevel() < it->second->accesslevel)
	{
		if(player->getAccessLevel() > 0)
		{
			player->sendTextMessage(MSG_STATUS_SMALL, "You can not execute this command.");
		}

		return false;
	}

	//execute command
	CommandFunc cfunc = it->second->f;
	(this->*cfunc)(creature, str_command, str_param);
	if(player)
		player->sendTextMessage(MSG_STATUS_CONSOLE_RED, cmd.c_str());

	return true;
}

bool Commands::placeNpc(Creature* creature, const std::string& cmd, const std::string& param)
{
	Npc* npc = Npc::createNpc(param);
	if(!npc){
        return false;
	}

	// Place the npc
	if(g_game.placeCreature(npc, creature->getPosition())){
		g_game.addMagicEffect(creature->getPosition(), NM_ME_MAGIC_BLOOD);
		return true;
	}
	else{
		delete npc;
		Player* player = creature->getPlayer();
		if(player){
			player->sendCancelMessage(RET_NOTENOUGHROOM);
			g_game.addMagicEffect(creature->getPosition(), NM_ME_PUFF);
		}
		return true;
	}

	return false;
}

bool Commands::placeMonster(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();

	Monster* monster = Monster::createMonster(param);
	if(!monster){
		if(player){
			player->sendCancelMessage(RET_NOTPOSSIBLE);
			g_game.addMagicEffect(player->getPosition(), NM_ME_PUFF);
		}
		return false;
	}

	// Place the monster
	if(g_game.placeCreature(monster, creature->getPosition())){
		g_game.addMagicEffect(creature->getPosition(), NM_ME_MAGIC_BLOOD);
		return true;
	}
	else{
		delete monster;
		if(player){
			player->sendCancelMessage(RET_NOTENOUGHROOM);
			g_game.addMagicEffect(player->getPosition(), NM_ME_PUFF);
		}
	}

	return false;
}

bool Commands::placeSummon(Creature* creature, const std::string& cmd, const std::string& param)
{
	ReturnValue ret = RET_NOERROR;

	Monster* monster = Monster::createMonster(param);
	if(monster){
		// Place the monster
		creature->addSummon(monster);
		if(!g_game.placeCreature(monster, creature->getPosition())){
			creature->removeSummon(monster);
			ret = RET_NOTENOUGHROOM;
		}
	}
	else{
		ret = RET_NOTPOSSIBLE;
	}

	if(ret != RET_NOERROR){
		if(Player* player = creature->getPlayer()){
			player->sendCancelMessage(ret);
			g_game.addMagicEffect(player->getPosition(), NM_ME_PUFF);
		}
	}

	return (ret == RET_NOERROR);
}

bool Commands::broadcastMessage(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	g_game.internalBroadcastMessage(player, param);
	return true;
}

bool Commands::teleportMasterPos(Creature* creature, const std::string& cmd, const std::string& param)
{
	Position destPos = creature->getPosition();
	if(g_game.internalTeleport(creature, creature->masterPos) == RET_NOERROR){
		g_game.addMagicEffect(destPos, NM_ME_ENERGY_AREA);
		return true;
	}

	return false;
}

bool Commands::teleportHere(Creature* creature, const std::string& cmd, const std::string& param)
{
	Creature* paramCreature = g_game.getCreatureByName(param);
	if(paramCreature){
		Position destPos = paramCreature->getPosition();
		if(g_game.internalTeleport(paramCreature, creature->getPosition()) == RET_NOERROR){
			g_game.addMagicEffect(destPos, NM_ME_ENERGY_AREA);
			return true;
		}
	}

	return false;
}

bool Commands::createItemById(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	std::string tmp = param;

	std::string::size_type pos = tmp.find(' ', 0);
	if(pos == std::string::npos){
		pos = tmp.size();
	}

	int32_t type = atoi(tmp.substr(0, pos).c_str());
	int32_t count = 1;
	if(pos < tmp.size()){
		tmp.erase(0, pos+1);
		count = std::max(0, std::min(atoi(tmp.c_str()), 100));
	}

	Item* newItem = Item::CreateItem(type, count);
	if(!newItem){
		return false;
    }
    g_game.startDecay(newItem);

	ReturnValue ret = g_game.internalAddItem(player, newItem);

	if(ret != RET_NOERROR){
		ret = g_game.internalAddItem(player->getTile(), newItem, INDEX_WHEREEVER, FLAG_NOLIMIT);

		if(ret != RET_NOERROR){
			delete newItem;
			return false;
		}
	}

	g_game.addMagicEffect(player->getPosition(), NM_ME_MAGIC_POISON);
	return true;
}

bool Commands::createItemByName(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	std::string::size_type pos1 = param.find("\"");
	pos1 = (std::string::npos == pos1 ? 0 : pos1 + 1);

	std::string::size_type pos2 = param.rfind("\"");
	if(pos2 == pos1 || pos2 == std::string::npos){
		pos2 = param.rfind(' ');

		if(pos2 == std::string::npos){
			pos2 = param.size();
		}
	}

	std::string itemName = param.substr(pos1, pos2 - pos1);

	int32_t count = 1;
	if(pos2 < param.size()){
		std::string itemCount = param.substr(pos2 + 1, param.size() - (pos2 + 1));
		count = std::max(0, std::min(atoi(itemCount.c_str()), 100));
	}

	int32_t itemId = Item::items.getItemIdByName(itemName);
	if(itemId == -1){
		player->sendTextMessage(MSG_STATUS_CONSOLE_RED, "Item could not be summoned.");
		return false;
	}

	Item* newItem = Item::CreateItem(itemId, count);
	 if(!newItem){
		return false;
    }
    g_game.startDecay(newItem);

	ReturnValue ret = g_game.internalAddItem(player, newItem);

	if(ret != RET_NOERROR){
		ret = g_game.internalAddItem(player->getTile(), newItem, INDEX_WHEREEVER, FLAG_NOLIMIT);

		if(ret != RET_NOERROR){
			delete newItem;
			return false;
		}
	}

	g_game.addMagicEffect(player->getPosition(), NM_ME_MAGIC_POISON);
	return true;
}

bool Commands::subtractMoney(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	int count = atoi(param.c_str());
	uint32_t money = g_game.getMoney(player);
	if(!count){
		std::stringstream info;
		info << "You have " << money << " gold.";
		player->sendCancel(info.str().c_str());
		return true;
	}
	else if(count > (int)money){
		std::stringstream info;
		info << "You have " << money << " gold and is not sufficient.";
		player->sendCancel(info.str().c_str());
		return true;
	}

	if(!g_game.removeMoney(player, count)){
		std::stringstream info;
		info << "Can not subtract money!";
		player->sendCancel(info.str().c_str());
	}

	return true;
}

bool Commands::reloadInfo(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();

	if(param == "actions" || param == "action"){
		g_actions->reload();
		if(player) player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded actions.");
	}
	else if(param == "commands" || param == "command"){
		this->reload();
		if(player) player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded commands.");
	}
	else if(param == "monsters" || param == "monster"){
		g_monsters.reload();
		if(player) player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded monsters.");
	}
	else if(param == "npc"){
		g_npcs.reload();
		if(player) player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded npcs.");
	}
	else if(param == "config"){
		g_config.reload();
		if(player) player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded config.");
	}
	else if(param == "talk" || param == "talkactions" || param == "talk actions" || param == "ta"){
		g_talkactions->reload();
		if(player) player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded talk actions.");
	}
	else if(param == "move" || param == "movement" || param == "movement actions"){
		g_moveEvents->reload();
		if(player) player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded movement actions.");
	}
	else if(param == "spells" || param == "spell"){
		g_spells->reload();
		g_monsters.reload();
		if(player) player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded spells and monsters.");
	}
	else if(param == "raids" || param == "raid"){
		Raids::getInstance()->reload();
		Raids::getInstance()->startup();
		if(player) player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded raids.");
	}
	/*
	else if(param == "weapons"){
		g_weapons->reload();
	}
	else if(param == "items"){
		Item::items.reload();
	}
	*/
	else if(param == "creaturescripts" || param == "creature scripts" || param == "cs"){
		g_creatureEvents->reload();
		if(player) player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded creature scripts.");
	}
	else{
		if(player) player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Option not found.");
	}

	return true;
}

bool Commands::testCommand(Creature* creature, const std::string& cmd, const std::string& param)
{
	int color = atoi(param.c_str());
	Player* player = creature->getPlayer();
	if(player) {
		player->sendMagicEffect(player->getPosition(), color);
	}

	return true;
}

bool Commands::teleportToTown(Creature* creature, const std::string& cmd, const std::string& param)
{
	std::string tmp = param;
	Player* player = creature->getPlayer();

	if(!player){
		return false;
	}

	Town* town = Towns::getInstance().getTown(tmp);
	if(town){
		if(g_game.internalTeleport(creature, town->getTemplePosition()) == RET_NOERROR) {
			g_game.addMagicEffect(town->getTemplePosition(), NM_ME_ENERGY_AREA);
			return true;
		}
	}

	player->sendCancel("Could not find the town.");

	return false;
}

bool Commands::teleportTo(Creature* creature, const std::string& cmd, const std::string& param)
{
	Position destPos;
	if(Waypoint_ptr waypoint = g_game.getMap()->waypoints.getWaypointByName(param)){
		destPos = waypoint->pos;
	}
	else if(Creature* paramCreature = g_game.getCreatureByName(param)){
		destPos = paramCreature->getPosition();
	}
	else{
		return false;
	}

	if(g_game.internalTeleport(creature, destPos) == RET_NOERROR){
#ifdef __PB_GMINVISIBLE__
		if(!creature->isGmInvis())
#endif
		g_game.addMagicEffect(destPos, NM_ME_ENERGY_AREA);
		return true;
	}

	return false;
}

bool Commands::getInfo(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return true;

	Player* paramPlayer = g_game.getPlayerByName(param);
	if(paramPlayer) {
		std::stringstream info;
		if(paramPlayer->getAccessLevel() >= player->getAccessLevel() && player != paramPlayer){
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "You can not get info about this player.");
			return true;
		}
		
		uint8_t ip[4];
		*(uint32_t*)&ip = paramPlayer->lastip;
		info << "name:   " << paramPlayer->getName() << std::endl <<
				"access: " << paramPlayer->getAccessLevel() << std::endl <<
				"level:  " << paramPlayer->getPlayerInfo(PLAYERINFO_LEVEL) << std::endl <<
				"maglvl: " << paramPlayer->getPlayerInfo(PLAYERINFO_MAGICLEVEL) << std::endl <<
				"speed:  " <<  paramPlayer->getSpeed() <<std::endl <<
				"position " << paramPlayer->getPosition() << std::endl <<
				"ip: " << ipText(ip);
		player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, info.str().c_str());
	}
	else{
		player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Player not found.");
	}

	return true;
}

bool Commands::closeServer(Creature* creature, const std::string& cmd, const std::string& param)
{
	g_game.setGameState(GAME_STATE_CLOSED);
	//kick players with unauthorized players
	AutoList<Player>::listiterator it = Player::listPlayer.list.begin();
	while(it != Player::listPlayer.list.end())
	{
		if(!(*it).second->hasFlag(PlayerFlag_CanAlwaysLogin)){
			(*it).second->kickPlayer();
			it = Player::listPlayer.list.begin();
		}
		else{
			++it;
		}
	}
	// Save the rest of everything (including players that stayed online)
	g_game.saveServer();

	Player* player = creature->getPlayer();

	// Is it a real serversave?
	if(param == "serversave"){
		// Pay houses & clear bans
		Houses::getInstance().payHouses();
		g_bans.clearTemporaryBans();
	}

	// Notify player (If he's still online)
	if(player){
		player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Server is now closed.");
    }
    
    return true;
}

bool Commands::saveServer(Creature* creature, const std::string& cmd, const std::string& param)
{
    // Save most everything
	g_game.saveServer();

	// Notify player
	if(Player* player = creature->getPlayer()){
		player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Server has been saved.");
	}

	return true;
}

bool Commands::openServer(Creature* creature, const std::string& cmd, const std::string& param)
{
    g_game.setGameState(GAME_STATE_NORMAL);
    
    Player* player = creature->getPlayer();
    if(player){
        player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Server is now open.");
    }
    
    return true;
}

bool Commands::onlineList(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	int32_t alevelmin = 0;
	int32_t alevelmax = 10000;
	int i, n;

	if(param == "gm")
		alevelmin = 1;
	else if(param == "normal")
		alevelmax = 0;

	std::stringstream players;
	players << "name   level   mag" << std::endl;

	i = 0;
	n = 0;
	AutoList<Player>::listiterator it = Player::listPlayer.list.begin();
	for(;it != Player::listPlayer.list.end();++it)
	{
		if((*it).second->getAccessLevel() >= alevelmin && (*it).second->getAccessLevel() <= alevelmax){
			players << (*it).second->getName() << "   " <<
				(*it).second->getPlayerInfo(PLAYERINFO_LEVEL) << "    " <<
				(*it).second->getPlayerInfo(PLAYERINFO_MAGICLEVEL) << std::endl;
			n++;
			i++;
		}
		if(i == 10){
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, players.str().c_str());
			players.str("");
			i = 0;
		}
	}
	if(i != 0)
		player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, players.str().c_str());

	players.str("");
	players << "Total: " << n << " player(s)" << std::endl;
	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, players.str().c_str());
	return true;
}

bool Commands::teleportNTiles(Creature* creature, const std::string& cmd, const std::string& param)
{
	int ntiles = atoi(param.c_str());
	if(ntiles != 0)
	{
		Position newPos = creature->getPosition();
		switch(creature->getDirection()){
		case NORTH:
			newPos.y = newPos.y - ntiles;
			break;
		case SOUTH:
			newPos.y = newPos.y + ntiles;
			break;
		case EAST:
			newPos.x = newPos.x + ntiles;
			break;
		case WEST:
			newPos.x = newPos.x - ntiles;
			break;
		default:
			break;
		}

#ifdef __PB_GMINVISIBLE__
		if(g_game.internalTeleport(creature, newPos) == RET_NOERROR && !creature->isGmInvis()){
#else
		if(g_game.internalTeleport(creature, newPos) == RET_NOERROR){
#endif
			g_game.addMagicEffect(newPos, NM_ME_ENERGY_AREA);
		}
	}

	return true;
}

bool Commands::kickPlayer(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* playerKick = g_game.getPlayerByName(param);
	if(playerKick){
		Player* player = creature->getPlayer();
		if(player && player->getAccessLevel() <= playerKick->getAccessLevel()){
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "You cannot kick this player.");
			return true;
		}

		playerKick->kickPlayer();
		return true;
	}
	return false;
}

bool Commands::setHouseOwner(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(player){
		if(player->getTile()->hasFlag(TILESTATE_HOUSE)){
			HouseTile* houseTile = dynamic_cast<HouseTile*>(player->getTile());
			if(houseTile){

				std::string real_name = param;
				uint32_t guid;
				if(param == "none"){
					houseTile->getHouse()->setHouseOwner(0);
				}
				else if(IOPlayer::instance()->getGuidByName(guid, real_name)){
					houseTile->getHouse()->setHouseOwner(guid);
				}
				else{
					player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Player not found.");
				}
				return true;
			}
		}
	}
	return false;
}

bool Commands::sellHouse(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(player){
		House* house = Houses::getInstance().getHouseByPlayerId(player->getGUID());
		if(!house){
			player->sendCancel("You do not own any house.");
			return false;
		}

		Player* tradePartner = g_game.getPlayerByName(param);
		if(!(tradePartner && tradePartner != player)){
			player->sendCancel("Trade player not found.");
			return false;
		}

		if(tradePartner->getPlayerInfo(PLAYERINFO_LEVEL) < 1){
			player->sendCancel("Trade player level is too low.");
			return false;
		}

		if(Houses::getInstance().getHouseByPlayerId(tradePartner->getGUID())){
			player->sendCancel("Trade player already owns a house.");
			return false;
		}

		if(!Position::areInRange<2,2,0>(tradePartner->getPosition(), player->getPosition())){
			player->sendCancel("Trade player is too far away.");
			return false;
		}

		Item* transferItem = house->getTransferItem();
		if(!transferItem){
			player->sendCancel("You can not trade this house.");
			return false;
		}

		transferItem->getParent()->setParent(player);
		if(g_game.internalStartTrade(player, tradePartner, transferItem)){
			return true;
		}
		else{
			house->resetTransferItem();
		}
	}
	return false;
}

bool Commands::getHouse(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	std::string real_name = param;
	uint32_t guid;
	if(IOPlayer::instance()->getGuidByName(guid, real_name)){
		House* house = Houses::getInstance().getHouseByPlayerId(guid);
		std::stringstream str;
		str << real_name;
		if(house){
			str << " owns house: " << house->getName() << ".";
		}
		else{
			str << " does not own any house.";
		}

		player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, str.str().c_str());
	}
	return false;
}

bool Commands::serverInfo(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	std::stringstream text;
	text << "Server Info:";
	text << "\nExp Rate: " << g_game.getExperienceStage(player->getLevel());
	text << "\nSkill Rate: " << g_config.getNumber(ConfigManager::RATE_SKILL);
	text << "\nMagic Rate: " << g_config.getNumber(ConfigManager::RATE_MAGIC);
	text << "\nLoot Rate: " << g_config.getNumber(ConfigManager::RATE_LOOT);

	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, text.str().c_str());

	return true;
}

#ifdef __ENABLE_SERVER_DIAGNOSTIC__
bool Commands::serverDiag(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	std::stringstream text;
	text << "Server diagonostic:\n";
	text << "World:" << "\n";
	text << "Player: " << g_game.getPlayersOnline() << " (" << Player::playerCount << ")\n";
	text << "Npc: " << g_game.getNpcsOnline() << " (" << Npc::npcCount << ")\n";
	text << "Monster: " << g_game.getMonstersOnline() << " (" << Monster::monsterCount << ")\n";

	text << "\nProtocols:" << "\n";
	text << "--------------------\n";
	text << "Protocol76: " << Protocol76::protocol76Count << "\n";
	text << "ProtocolLogin: " << ProtocolLogin::protocolLoginCount << "\n";
	text << "ProtocolStatus: " << ProtocolStatus::protocolStatusCount << "\n\n";

	text << "\nConnections:\n";
	text << "--------------------\n";
	text << "Active connections: " << Connection::connectionCount << "\n";
	text << "Total message pool: " << OutputMessagePool::getInstance()->getTotalMessageCount() << "\n";
	text << "Auto message pool: " << OutputMessagePool::getInstance()->getAutoMessageCount() << "\n";
	text << "Free message pool: " << OutputMessagePool::getInstance()->getAvailableMessageCount() << "\n";

	text << "\nLibraries:\n";
	text << "--------------------\n";
	text << "asio: " << BOOST_ASIO_VERSION << "\n";
	text << "libxml: " << XML_DEFAULT_VERSION << "\n";
	text << "lua: " << LUA_VERSION << "\n";

	//TODO: more information that could be useful

	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, text.str().c_str());

	return true;
}
#endif

void showTime(std::stringstream& str, uint32_t time)
{
	if(time == 0xFFFFFFFF){
		str << "permanent";
	}
	else if(time == 0){
		str << "serversave";
	}
	else{
		char buffer[32];
		formatDate((time_t)time, buffer);
		str << buffer;
	}
}

uint32_t parseTime(const std::string& time)
{
	if(time == "serversave" || time == "shutdown"){
		return 0;
	}
	if(time == "permanent"){
		return 0xFFFFFFFF;
	}
	else{
		boost::char_separator<char> sep("+");
		tokenizer timetoken(time, sep);
		tokenizer::iterator timeit = timetoken.begin();
		if(timeit == timetoken.end()){
			return 0;
		}
		uint32_t number = atoi(timeit->c_str());
		uint32_t multiplier = 0;
		++timeit;
		if(timeit == timetoken.end()){
			return 0;
		}
		if(*timeit == "m") //minute
			multiplier = 60;
		if(*timeit == "h") //hour
			multiplier = 60*60;
		if(*timeit == "d") //day
			multiplier = 60*60*24;
		if(*timeit == "w") //week
			multiplier = 60*60*24*7;
		if(*timeit == "o") //month
			multiplier = 60*60*24*30;
		if(*timeit == "y") //year
			multiplier = 60*60*24*365;

		uint32_t currentTime = std::time(NULL);
		return currentTime + number*multiplier;
	}
}

std::string parseParams(tokenizer::iterator &it, tokenizer::iterator end)
{
	std::string tmp;
	if(it == end){
		return "";
	}
	else{
		tmp = *it;
		++it;
		if(tmp[0] == '"'){
			tmp.erase(0,1);
			while(it != end && tmp[tmp.length() - 1] != '"'){
				tmp += " " + *it;
				++it;
			}

			if(tmp.length() > 0 && tmp[tmp.length() - 1] == '"'){
				tmp.erase(tmp.length() - 1);
			}
		}
		return tmp;
	}
}

bool Commands::forceRaid(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player){
		return false;
	}

	Raid* raid = Raids::getInstance()->getRaidByName(param);
	if(!raid || !raid->isLoaded()){
		player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "No such raid exists.");
		return false;
	}

	if(Raids::getInstance()->getRunning()){
		player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Another raid is already being executed.");
		return false;
	}

	Raids::getInstance()->setRunning(raid);
	RaidEvent* event = raid->getNextRaidEvent();

	if(!event){
		player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "The raid does not contain any data.");
		return false;
	}

	raid->setState(RAIDSTATE_EXECUTING);
	uint32_t ticks = event->getDelay();
	if(ticks > 0){
		Scheduler::getScheduler().addEvent(createSchedulerTask(ticks,
			boost::bind(&Raid::executeRaidEvent, raid, event)));
	}
	else{
		Dispatcher::getDispatcher().addTask(createTask(
		boost::bind(&Raid::executeRaidEvent, raid, event)));

	}

	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Raid started.");
	return true;
}

bool Commands::goUp(Creature* creature, const std::string &cmd, const std::string &param)
{
    Position newPos = creature->getPosition();
	newPos.z--;
	if(g_game.internalTeleport(creature, newPos) == RET_NOERROR){
			g_game.addMagicEffect(newPos, NM_ME_ENERGY_AREA);
            		
		return true;
    }
    
    return false;
}

bool Commands::goDown(Creature* creature, const std::string &cmd, const std::string &param)
{
    Position newPos = creature->getPosition();
	newPos.z++;
	Tile* tile = g_game.getTile(newPos.x, newPos.y, newPos.z);		
	if(tile && tile->creatures.size() != 0){
		newPos.x++;
    }
    
	if(g_game.internalTeleport(creature, newPos) == RET_NOERROR){
			g_game.addMagicEffect(newPos, NM_ME_ENERGY_AREA);
		
		return true;
    }
    
    return false;
}

bool Commands::whoIsOnline(Creature* creature, const std::string &cmd, const std::string &param)
{
	if(Player* player = creature->getPlayer()){
		AutoList<Player>::listiterator iter = Player::listPlayer.list.begin();
		uint16_t totalPlayers = 0;
		uint16_t positionInList = 1;
		std::string guildNick = "";
		std::stringstream info;
		info << "Players online list:";

		while(iter != Player::listPlayer.list.end()){
			if((*iter).second->getAccessLevel() < 3){
				info << "\n" << positionInList << ". " << (*iter).second->getName() << " [" << (*iter).second->getLevel() << "] ";           
				++totalPlayers;
				++positionInList;
			}
			++iter;
		}
        
		player->sendTextWindow(1949, info.str().c_str());
	}

	return true;
}

#ifdef __PB_GMINVISIBLE__
bool Commands::setGmInvisible(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player){
		return false;
	}
 
	player->setGmInvis();
 
	SpectatorVec list;
	SpectatorVec::iterator it;
	Player* tmpPlayer;
	g_game.getSpectators(list, player->getPosition(), true);
 
	Cylinder* cylinder = player->getTopParent();
	int32_t index = cylinder->__getIndexOfThing(creature);
 
	for(it = list.begin(); it != list.end(); ++it){
		if( (tmpPlayer = (*it)->getPlayer()) ){
			//Players with access level higher or equal to the invisible player can see him
			tmpPlayer->sendCreatureChangeVisible(player, !player->isGmInvis());
			if(tmpPlayer != player && !tmpPlayer->canSeeGmInvis(player)){
				if(player->isGmInvis()){
					tmpPlayer->sendCreatureDisappear(player, index, true);
				}
				else{
					tmpPlayer->sendCreatureAppear(player, true);
				}
				tmpPlayer->sendUpdateTile(player->getTile(), player->getPosition());
			}
		}
	}
 
	for(it = list.begin(); it != list.end(); ++it){
		(*it)->onUpdateTile(player->getTile(), player->getPosition());
	}
 
	if(player->isGmInvis()){
		player->sendTextMessage(MSG_INFO_DESCR, "You are invisible.");
		//Nate Edit
		for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it){
			if(!it->second->canSeeGmInvis(player)){
				it->second->notifyLogOut(player);
			}
		}
		//Nate Edit
	}
	else{
		player->sendTextMessage(MSG_INFO_DESCR, "You are visible.");
		//Nate Edit
		for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it){
			if(!it->second->canSeeGmInvis(player)){
				it->second->notifyLogIn(player);
			}
		}
		//Nate Edit
	}
	return true;
}
#endif

bool Commands::playerKills(Creature* creature, const std::string& cmd, const std::string& param)
{
	if (Player* player = creature->getPlayer()) {
		int32_t fragTime = g_config.getNumber(ConfigManager::FRAG_TIME);
		if (player->redSkullTicks && fragTime > 0) {
			int32_t frags = (player->redSkullTicks / fragTime) + 1;
			int32_t remainingTime = player->redSkullTicks - (fragTime * (frags - 1));
			int32_t hours = ((remainingTime / 1000) / 60) / 60;
			int32_t minutes = ((remainingTime / 1000) / 60) - (hours * 60);

			char buffer[175];
			sprintf(buffer, "You have %d unjustified kill%s. The amount of unjustified kills will decrease after: %s.", frags, (frags > 1 ? "s" : ""), formatTime(hours, minutes).c_str());
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, buffer);
		}
		else {
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "You do not have any unjustified kill.");
		}
	}

	return false;
}

bool Commands::buyHouse(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;
		
	Position pos = player->getPosition();
	switch(player->getDirection()){
		case NORTH:
			pos.y--;
			break;
		case SOUTH:
			pos.y++;
			break;
		case WEST:
			pos.x--;
			break;
		case EAST:
			pos.x++;
			break;
	}
		if(Houses::getInstance().getHouseByPlayerId(player->getGUID()))
        {
			player->sendCancel("You already have a house.");
			return false;
		}
	Tile* tile = g_game.getTile(pos.x, pos.y, pos.z);
	if(tile){
		HouseTile* houseTile = dynamic_cast<HouseTile*>(tile);
		if(houseTile){
			House* house = houseTile->getHouse();
			if(house){
				Door* door = house->getDoorByPosition(pos);
				if(door){
					int price = 0;
					if(!player->isPremium() && g_config.getBool(ConfigManager::HOUSE_NEED_PREMIUM))
                    {
                        player->sendCancel("Sorry, you need a premium account to buy a house.");
						return false;
					}
                    if(player->getLevel() < g_config.getNumber(ConfigManager::HOUSE_LEVEL))
                    {
						std::stringstream msg;
						msg << "You need level " << g_config.getNumber(ConfigManager::HOUSE_LEVEL) << " to buy a house.";
                        player->sendCancel(msg.str().c_str());
						return false;
					}
					if(house->getHouseOwner() != 0)
                    {
						player->sendCancel("This house already has an owner.");
						return false;
					}

					for(HouseTileList::iterator it = house->getHouseTileBegin(); it != house->getHouseTileEnd(); it++){
						price += g_config.getNumber(ConfigManager::HOUSE_PRICE);
					}
					if(price <= 0)
						return false;
					uint32_t money = g_game.getMoney(player);
					if(money < price){
						player->sendCancel("You do not have enough money.");
						return false;
					}
					if(!g_game.removeMoney(player, price)){
						player->sendCancel("You do not have enough money.");
						return false;
					}

					house->setHouseOwner(player->getGUID());
					player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Now you own this house.");
				}
			}
		}
	}

	return true;
}

bool Commands::refreshMap(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player){
		return false;
	}

	g_game.proceduralRefresh();

	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Refreshed map.");
	return true;
}

bool Commands::cleanMap(Creature* creature, const std::string& cmd, const std::string& param)
{

	int32_t itemscleaned = g_game.map->onRemoveTileItem();
	char* buffer = new char[128];
	sprintf(buffer, "Game map cleaned. Collected %d items from the map.", itemscleaned);
	g_game.anonymousBroadcastMessage(MSG_STATUS_WARNING, std::string(buffer));
	delete [] buffer;
	return true;
}

bool Commands::setTrace(Creature* creature, const std::string &cmd, const std::string &param)
{
     
     Player* player = creature->getPlayer();
     if(!player || player->isRemoved())
     return false;

     if(param == "")
     player->setTrace("");
     player->sendTextMessage(MSG_INFO_DESCR, "Trace removed!");
     return true;

     std::string lplayer, lparam;
     lplayer = player->getName();
     lparam = param;
     toLowerCaseString(lplayer);
     toLowerCaseString(lparam);

     if(lplayer == lparam)
     player->sendCancel("Sorry, you cannot trace yourself!");
     return true;
     
     Player* traced = g_game.getPlayerByName(param);
     if(!traced)
     player->sendCancel("Sorry, a player with this name is not online!");
     return true;

     player->setTrace(traced->getName());
     player->sendTextMessage(MSG_INFO_DESCR, "Trace successfully set!");
}


bool Commands::changeSex(Creature* creature, const std::string &cmd, const std::string &param)
{
     
     Player* player = creature->getPlayer();
     if (!player)
        return false;

     uint32_t money = g_game.getMoney(player);
	 const uint32_t changeSexPrice = g_config.getNumber(ConfigManager::CHANGESEX_PRICE);
	 
     if(changeSexPrice <= 0)
	      return false;
	      
	if(!player->isPremium() && g_config.getBool(ConfigManager::CHANGESEX_ONLY_PREMIUM))
    {
        player->sendCancel("Sorry, you need a premium account to change your sex.");
		return false;
	}
	
	if(money < changeSexPrice)
	{
		char buffer[70 + changeSexPrice];
		sprintf(buffer, "You do not have enough money. You need %d gold coins to change your sex.", changeSexPrice);
		player->sendCancel(buffer);
		return false;
	}
	
	if(!g_game.removeMoney(player, changeSexPrice))
    {
		player->sendCancel("You do not have enough money.");
		return false;
	}
	
	
     bool sex = player->sex == PLAYERSEX_MALE;
	 std::stringstream ss;
     if (sex) 
     {
        player->sex = (playersex_t)PLAYERSEX_FEMALE;
		ss << "You have changed your sex to a sex girl.";
		player->sendTextMessage(MSG_INFO_DESCR, ss.str());
        player->sendMagicEffect(player->getPosition(), NM_ME_MAGIC_ENERGY);
     }
     else
     {
        player->sex = (playersex_t)PLAYERSEX_MALE;
		ss << "You have changed your sex to a man.";
		player->sendTextMessage(MSG_INFO_DESCR, ss.str());
        player->sendMagicEffect(player->getPosition(), NM_ME_MAGIC_ENERGY);
     }
     return true;
}

bool Commands::broadcastColor(Creature* creature, const std::string &cmd, const std::string &param)
{
    int a;
    int colorInt;
    Player* player = creature->getPlayer();
    std::string message = param.c_str();
    std::stringstream fullMessage;
    std::string color;
    MessageClasses mclass;
    
    for(a=0; a<param.length(); ++a){
       if(param[a] > 3 && param[a] == ' '){
         color = param;
         color.erase(a,1-param.length());
         message.erase(0,1+a);
         break;
       }
       else
          message = param.c_str();       
    }
    
    std::transform(color.begin(), color.end(), color.begin(), tolower);
    fullMessage << message.c_str() <<std::endl; //Name: Message
    
    if(color == "blue_console")
       mclass = MSG_STATUS_CONSOLE_BLUE;
    else if(color == "red")
    {
       g_game.internalBroadcastMessage(player, fullMessage.str().c_str());
       return false;
    }
    else if(color == "red_console")
       mclass = MSG_STATUS_CONSOLE_RED;
    else if(color == "advance")
       mclass = MSG_EVENT_ADVANCE; //Invasion
    else if(color == "default")
       mclass = MSG_EVENT_DEFAULT;
    else if(color == "green")
       mclass = MSG_INFO_DESCR;
    else if(color == "small")
       mclass = MSG_STATUS_DEFAULT;                                      
    else if(color == "warning")
        mclass = MSG_STATUS_WARNING;
    else if(color == "orange")
       mclass = MSG_STATUS_CONSOLE_ORANGE;
        
    else{
       player->sendTextMessage(MSG_STATUS_SMALL, "Define a color, or use #B to broadcast red.");
       return false;
    }
                      
    for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it){
       if(dynamic_cast<Player*>(it->second))
         (*it).second->sendTextMessage(mclass, fullMessage.str().c_str());
    }
    
    return true;
}

bool Commands::leaveHouse(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(player){
		House* house = Houses::getInstance().getHouseByPlayerId(player->getGUID());
		if(!house){
			player->sendCancel("You do not own a house.");	
			return false;
		}

		house->setHouseOwner(0);
		player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "You have successfully left your house.");
	}
	
	return false;
}

bool Commands::newType(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	int32_t lookType = atoi(param.c_str());
	if(player)
	{	
		if(lookType < 0 || lookType == 1 || lookType == 135 || 
		lookType == 143 ||lookType > 144)
			player->sendTextMessage(MSG_STATUS_SMALL, "This looktype does not exist.");
		else
			g_game.internalCreatureChangeOutfit(creature, (const Outfit_t&)lookType);
			player->sendMagicEffect(player->getPosition(), NM_ME_SOUND_PURPLE);
	}
}

bool Commands::banPlayer(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* playerBan = g_game.getPlayerByName(param);
	if(playerBan)
	{
		if(playerBan->hasFlag(PlayerFlag_CannotBeBanned))
		{
			if(Player* player = creature->getPlayer())
				player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "You cannot ban this player.");
			return true;
		}

		playerBan->sendTextMessage(MSG_STATUS_CONSOLE_RED, "You have been banned.");
		uint32_t ip = playerBan->lastip;
		if(ip > 0)
			  g_bans.addIpBanishment(ip, (std::time(NULL) + g_config.getNumber(ConfigManager::IPBANISHMENT_LENGTH)), playerBan->getGUID(), "nothing", "Player banished by IP!");
			//g_bans.addIpBanishment(ip, 0xFFFFFFFF, (time(NULL) + 86400));
			//g_bans.addIpBanishment(ip, (std::time(NULL) + g_config.getNumber(ConfigManager::IPBANISHMENT_LENGTH)), player->getGUID(), banReason, comment);

		playerBan->kickPlayer();
		return true;
	}
	return false;
}
