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

#include "definitions.h"
#include "configmanager.h"
#include <iostream>

ConfigManager::ConfigManager()
{
	m_isLoaded = false;

	m_confString[IP] = "";
	m_confInteger[PORT] = 0;
}

ConfigManager::~ConfigManager()
{
	//
}

bool ConfigManager::loadFile(const std::string& _filename)
{
	if(L)
		lua_close(L);

	L = lua_open();

	if(!L) return false;

	if(luaL_dofile(L, _filename.c_str()))
	{
		lua_close(L);
		L = NULL;
		return false;
	}

	// parse config
	if(!m_isLoaded) // info that must be loaded one time (unless we reset the modules involved)
	{
		m_confString[CONFIG_FILE] = _filename;
		// These settings might have been set from command line
		if(m_confString[IP] == "")
			m_confString[IP] = getGlobalString(L, "ip", "127.0.0.1");
		if(m_confInteger[PORT] == 0)
			m_confInteger[PORT] = getGlobalNumber(L, "port");
		
	#if defined __CONFIG_V2__
	    unsigned int pos = _filename.rfind("/");
	    std::string configPath = "";
	    if(pos != std::string::npos)
	        configPath = _filename.substr(0, pos+1);
	        
        m_confString[DATA_DIRECTORY] = configPath + getGlobalString(L, "dataDirectory", "data/");
        m_confString[MAP_FILE] = m_confString[DATA_DIRECTORY] + getGlobalString(L, "map");
        m_confString[MAP_STORE_FILE] = m_confString[DATA_DIRECTORY] + getGlobalString(L, "mapStore");
        m_confString[HOUSEstage_STORE_FILE] = m_confString[DATA_DIRECTORY] + getGlobalString(L, "houseStore");
     #else
		m_confString[DATA_DIRECTORY] = getGlobalString(L, "dataDirectory");
		m_confString[MAP_FILE] = getGlobalString(L, "map");
		m_confString[MAP_STORE_FILE] = getGlobalString(L, "mapStore");
		m_confString[HOUSE_STORE_FILE] = getGlobalString(L, "houseStore");
	 #endif
		m_confString[MAP_KIND] = getGlobalString(L, "mapKind");
		if(getGlobalString(L, "md5passwords") != ""){
            std::cout << "Warning: [ConfigManager] md5passwords is deprecated. Use PasswordType instead." << std::endl;
        }
        m_confString[PASSWORD_TYPE_STR] = getGlobalString(L, "passwordType");
		m_confString[WORLD_TYPE] = getGlobalString(L, "worldType");
		m_confString[SQL_HOST] = getGlobalString(L, "sqlHost");
		m_confString[SQL_USER] = getGlobalString(L, "sqlUser");
		m_confString[SQL_PASS] = getGlobalString(L, "sqlPass");
		m_confInteger[SQL_PORT] = getGlobalNumber(L, "sqlPort");
		m_confString[SQL_DB] = getGlobalString(L, "sqlDatabaseName");
		m_confInteger[PASSWORD_TYPE] = PASSWORD_TYPE_PLAIN;
		m_confInteger[STATUSQUERY_TIMEOUT] = getGlobalNumber(L, "statusTimeout", 30 * 1000);
	
		#if defined __USE_MYSQL__ && defined __USE_SQLITE__
		m_confString[SQL_TYPE] = getGlobalString(L, "sqlType", "mysql");
		m_confInteger[SQLTYPE] = SQL_TYPE_NONE;
		#endif
	}
	m_confString[LOGIN_MSG] = getGlobalString(L, "loginMessage", "Welcome to Crystal Server.");
	m_confString[SERVER_NAME] = getGlobalString(L, "serverName");
	m_confString[WORLD_NAME] = getGlobalString(L, "worldName", "Crystal");
	m_confString[OWNER_NAME] = getGlobalString(L, "ownerName");
	m_confString[OWNER_EMAIL] = getGlobalString(L, "ownerEmail");
	m_confString[URL] = getGlobalString(L, "url");
	m_confString[LOCATION] = getGlobalString(L, "location");
	m_confString[MAP_STORAGE_TYPE] = getGlobalString(L, "mapStoreType", "relational");
	m_confInteger[LOGIN_TRIES] = getGlobalNumber(L, "loginTries", 3);
	m_confInteger[RETRY_TIMEOUT] = getGlobalNumber(L, "retryTimeout", 30 * 1000);
	m_confInteger[LOGIN_TIMEOUT] = getGlobalNumber(L, "loginTimeout", 5 * 1000);
	m_confInteger[MAX_PLAYERS] = getGlobalNumber(L, "maxPlayers");
	
	m_confInteger[EXHAUSTED] = getGlobalNumber(L, "exhausted", 1000);
	m_confInteger[EXHAUSTED_ADD] = getGlobalNumber(L, "exhaustedAdd", 200);
	m_confInteger[FIGHTEXHAUSTED] = getGlobalNumber(L, "fightExhausted", 2000);
	m_confInteger[HEALEXHAUSTED] = getGlobalNumber(L, "healExhausted", 1000);
	m_confInteger[PZ_LOCKED] = getGlobalNumber(L, "pzLock", 60 * 1000);
	m_confInteger[FIELD_OWNERSHIP_DURATION] = getGlobalNumber(L, "fieldOwnershipDuration", 5 * 1000);
	m_confInteger[MIN_ACTIONTIME] = getGlobalNumber(L, "minActionInterval", 200);
	m_confInteger[MIN_ACTIONEXTIME] = getGlobalNumber(L, "minActionExInterval", 1000);
	m_confInteger[MAX_MESSAGEBUFFER] = getGlobalNumber(L, "maxMessageBuffer", 4);
	m_confInteger[SAVE_CLIENT_DEBUG_ASSERTIONS] = getGlobalBool(L, "saveClientDebug", false);
	m_confBool[CHECK_ACCOUNTS] = getGlobalBool(L, "checkAccounts", false);
	m_confBool[USE_ACCBALANCE] = getGlobalBool(L, "useAccBalance", false);
	m_confInteger[MAX_IDLE_TIME] = getGlobalNumber(L, "maxIdleTime", 15)*60*1000;
	m_confBool[STORE_DEATHS] = getGlobalBool(L, "storePlayerDeathsInDB", false);
	m_confInteger[ALLOW_CLONES] = getGlobalNumber(L, "allowClones", 0);
	m_confBool[RANDOMIZE_TILES] = getGlobalBool(L, "randomizeTiles", false);
	m_confInteger[DEATH_LOSE_PERCENT] = getGlobalNumber(L, "deathLosePercent", 10);
	m_confBool[SHOW_ANNIMATION_UP] = getGlobalBool(L, "showAnnimationLevelSkill", false);
	
	
	m_confInteger[LEVEL_TO_ROOK] = getGlobalNumber(L, "levelToRook", 5);
    m_confInteger[ROOK_TEMPLE_ID] = getGlobalNumber(L, "rookTempleId", 1);

	m_confInteger[RATE_EXPERIENCE] = getGlobalNumber(L, "rateExp", 1);
	m_confInteger[RATE_SKILL] = getGlobalNumber(L, "rateSkill", 1);
	m_confInteger[RATE_LOOT] = getGlobalNumber(L, "rateLoot", 1);
	m_confInteger[RATE_MAGIC] = getGlobalNumber(L, "rateMagic", 1);

	m_confInteger[RATE_SPAWN] = getGlobalNumber(L, "rateSpawn", 1);
	m_confInteger[DEFAULT_DESPAWNRANGE] = getGlobalNumber(L, "spawnRange", 2);
	m_confInteger[DEFAULT_DESPAWNRADIUS] = getGlobalNumber(L, "spawnRadius", 50);
	
	m_confInteger[NOTATIONS_TO_BAN] = getGlobalNumber(L, "notationsToBan", 3);
	m_confInteger[WARNINGS_TO_FINALBAN] = getGlobalNumber(L, "warningsToFinalBan", 4);
	m_confInteger[WARNINGS_TO_DELETION] = getGlobalNumber(L, "warningsToDeletion", 5);
	m_confInteger[BAN_LENGTH] = getGlobalNumber(L, "banLength", 7 * 24 * 60 * 60);
	m_confInteger[FINALBAN_LENGTH] = getGlobalNumber(L, "finalBanLength", 30 * 24 * 60 * 60);
	m_confInteger[IPBANISHMENT_LENGTH] = getGlobalNumber(L, "ipBanishmentLength", 1 * 24 * 60 * 60);
	m_confInteger[WHITE_SKULL_TIME] = getGlobalNumber(L, "whiteSkullTime", 15);
	m_confInteger[KILLS_TO_RED] = getGlobalNumber(L, "killsToRedSkull", 3);
	m_confInteger[KILLS_TO_BAN] = getGlobalNumber(L, "killsToBan", 6);
	m_confInteger[FRAG_TIME] = getGlobalNumber(L, "timeToDecreaseFrags", 24 * 60 * 60 * 1000);

	m_confBool[REMOVE_AMMUNITION] = getGlobalBool(L, "removeAmmunition", true);
	m_confBool[REMOVE_RUNE_CHARGES] = getGlobalBool(L, "removeRuneCharges", true);
	m_confBool[REMOVE_WEAPON_CHARGES] = getGlobalBool(L, "removeWeaponCstagesenabledharges", true);
	
	m_confBool[FREE_PREMIUM] = getGlobalBool(L, "freePremium", false);
	m_confBool[CHANGE_OUTFIT] = getGlobalBool(L, "playersCannotChangeOutfit", false);
	m_confBool[USE_CAP] = getGlobalBool(L, "useCapSystem", true);
	m_confString[MOTD] = getGlobalString(L, "motd");

	m_confString[HOUSE_RENT_PERIOD] = getGlobalString(L, "houseRentPeriod", "monthly");
	m_confInteger[HOUSE_PRICE] = getGlobalNumber(L, "housePriceEachSqm", 1);
	m_confInteger[HOUSE_LEVEL] = getGlobalNumber(L, "levelToBuyHouse", 1);
	m_confBool[HOUSE_NEED_PREMIUM] = getGlobalBool(L, "housesOnlyPremiumAccounts", true);
	m_confBool[PREMIUM_ONLY_BEDS] = getGlobalBool(L, "bedsOnlyPremiumAccount", true);
	m_confBool[SHOW_HOUSE_PRICE] = getGlobalBool(L, "showHousesPrices", true);
	
	m_confBool[GLOBALSAVE_ENABLED] = getGlobalBool(L, "globalSaveEnabled", true);
	m_confBool[SHUTDOWN_AT_GLOBALSAVE] = getGlobalBool(L, "shutdownAtGlobalSave", false);
	m_confBool[CLEAN_MAP_AT_GLOBALSAVE] = getGlobalBool(L, "cleanMapAtGlobalSave", true);
	m_confInteger[GLOBALSAVE_H] = getGlobalNumber(L, "globalSaveHour", 8);
	m_confInteger[AUTO_SAVE_EACH_MINUTES] = getGlobalNumber(L, "autoSaveEachMinutes", 0);
	
	m_confInteger[CHANGESEX_PRICE] = getGlobalNumber(L, "changeSexPrice", 1);
	m_confBool[CHANGESEX_ONLY_PREMIUM] = getGlobalBool(L, "changeSexOnlyPremium", true);
	

	m_isLoaded = true;
	return true;
}

bool ConfigManager::reload()
{
	if(!m_isLoaded)
		return false;

	return loadFile(m_confString[CONFIG_FILE]);
}

const std::string& ConfigManager::getString(uint32_t _what) const
{
    if(m_isLoaded && _what < LAST_STRING_CONFIG){
        return m_confString[_what];
    }
    else{
        std::cout << "Warning: [ConfigManager::getString] " << _what << std::endl;
        return m_confString[DUMMY_STR];
    }
}

int ConfigManager::getNumber(uint32_t _what) const
{
	if(m_isLoaded && _what < LAST_INTEGER_CONFIG)
		return m_confInteger[_what];
	else
	{
		std::cout << "Warning: [ConfigManager::getNumber] " << _what << std::endl;
		return 0;
	}
}
bool ConfigManager::setNumber(uint32_t _what, int _value)
{
	if(_what < LAST_INTEGER_CONFIG)
	{
		m_confInteger[_what] = _value;
		return true;
	}
	else
	{
		std::cout << "Warning: [ConfigManager::setNumber] " << _what << std::endl;
		return false;
	}
}

bool ConfigManager::setString(uint32_t _what, const std::string& _value)
{
	if(_what < LAST_STRING_CONFIG)
	{
		m_confString[_what] = _value;
		return true;
	}
	else{
		std::cout << "Warning: [ConfigManager::setString] " << _what << std::endl;
		return false;
	}
}

std::string ConfigManager::getGlobalString(lua_State* _L, const std::string& _identifier, const std::string& _default)
{
	lua_getglobal(_L, _identifier.c_str());

	if(!lua_isstring(_L, -1)){
		lua_pop(_L, 1);
		return _default;
	}

	int len = (int)lua_strlen(_L, -1);
	std::string ret(lua_tostring(_L, -1), len);
	lua_pop(_L,1);

	return ret;
}

int ConfigManager::getGlobalNumber(lua_State* _L, const std::string& _identifier, int _default)
{
	lua_getglobal(_L, _identifier.c_str());

	if(!lua_isnumber(_L, -1)){
		lua_pop(_L, 1);
		return _default;
	}

	int val = (int)lua_tonumber(_L, -1);
	lua_pop(_L,1);

	return val;
}

bool ConfigManager::getGlobalBool(lua_State* _L, const std::string& _identifier, bool _default)
{
	lua_getglobal(_L, _identifier.c_str());

	if(lua_isnumber(_L, -1)){
		int val = (int)lua_tonumber(_L, -1);
		lua_pop(_L, 1);
		return val != 0;
	} else if(lua_isstring(_L, -1)){
		std::string val = lua_tostring(_L, -1);
		lua_pop(_L, 1);
		return val == "yes";
	} else if(lua_isboolean(_L, -1)){
		bool v = lua_toboolean(_L, -1) != 0;
		lua_pop(_L, 1);
		return v;
	}

	return _default;
}

void ConfigManager::getConfigValue(const std::string& key, lua_State* toL)
{
	lua_getglobal(L, key.c_str());
	moveValue(L, toL);
}

void ConfigManager::moveValue(lua_State* from, lua_State* to)
{
	switch(lua_type(from, -1)){
		case LUA_TNIL:
			lua_pushnil(to);
			break;
		case LUA_TBOOLEAN:
			lua_pushboolean(to, lua_toboolean(from, -1));
			break;
		case LUA_TNUMBER:
			lua_pushnumber(to, lua_tonumber(from, -1));
			break;
		case LUA_TSTRING:
		{
			size_t len;
			const char* str = lua_tolstring(from, -1, &len);
			lua_pushlstring(to, str, len);
		}
			break;
		case LUA_TTABLE:
			lua_newtable(to);
			
			lua_pushnil(from); // First key
			while(lua_next(from, -2)){
				// Move value to the other state
				moveValue(from, to);
				// Value is popped, key is left

				// Move key to the other state
				lua_pushvalue(from, -1); // Make a copy of the key to use for the next iteration
				moveValue(from, to);
				// Key is in other state.
				// We still have the key in the 'from' state ontop of the stack

				lua_insert(to, -2); // Move key above value
				lua_settable(to, -3); // Set the key
			}
		default:
			break;
	}
	// Pop the value we just read
	lua_pop(from, 1);
}

bool ConfigManager::getBool(uint32_t _what) const
{
	if(m_isLoaded && _what < LAST_BOOL_CONFIG)
		return m_confBool[_what];
	else
	{
		std::cout << "Warning: [ConfigManager::getBool] " << _what << std::endl;
		return false;
	}
}




