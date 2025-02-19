//=============================================================================//
//
// Purpose: Expose native code to VScript API
// 
//-----------------------------------------------------------------------------
// 
// See 'game/shared/vscript_shared.cpp' for more details.
//
//=============================================================================//

#include "core/stdafx.h"
#include "engine/server/server.h"
#include "game/shared/vscript_shared.h"
#include "vscript/vscript.h"
#include "vscript/languages/squirrel_re/include/sqvm.h"

#include "liveapi/liveapi.h"
#include "vscript_server.h"
#include <engine/host_state.h>
#include <networksystem/hostmanager.h>
#include <random>

#include "game/server/logger.h"
#include "player.h"
#include <common/callback.h>
#include <vscript/languages/squirrel_re/include/sqarrayutils.h>
/*
=====================
SQVM_ServerScript_f

  Executes input on the
  VM in SERVER context.
=====================
*/
static void SQVM_ServerScript_f(const CCommand& args)
{
    if (args.ArgC() >= 2)
    {
        Script_Execute(args.ArgS(), SQCONTEXT::SERVER);
    }
}
static ConCommand script("script", SQVM_ServerScript_f, "Run input code as SERVER script on the VM", FCVAR_DEVELOPMENTONLY | FCVAR_GAMEDLL | FCVAR_CHEAT | FCVAR_SERVER_FRAME_THREAD);


namespace VScriptCode
{
    namespace Server
    {
        //-----------------------------------------------------------------------------
        // Purpose: sets whether the server could auto reload at this time (e.g. if
        // server admin has host_autoReloadRate AND host_autoReloadRespectGameState
        // set, and its time to auto reload, but the match hasn't finished yet, wait
        // until this is set to proceed the reload of the server
        //-----------------------------------------------------------------------------
        SQRESULT SetAutoReloadState(HSQUIRRELVM v)
        {
            SQBool state = false;
            sq_getbool(v, 2, &state);

            g_hostReloadState = state;
            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }

        //-----------------------------------------------------------------------------
        // Purpose: kicks a player by given name
        //-----------------------------------------------------------------------------
        SQRESULT KickPlayerByName(HSQUIRRELVM v)
        {
            const SQChar* playerName = nullptr;
            const SQChar* reason = nullptr;

            sq_getstring(v, 2, &playerName);
            sq_getstring(v, 3, &reason);

            if (!VALID_CHARSTAR(playerName))
            {
                v_SQVM_ScriptError("Empty or null player name");
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            // Discard empty strings, this will use the default message instead.
            if (!VALID_CHARSTAR(reason))
                reason = nullptr;

            g_BanSystem.KickPlayerByName(playerName, reason);
            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }

        //-----------------------------------------------------------------------------
        // Purpose: kicks a player by given handle or id
        //-----------------------------------------------------------------------------
        SQRESULT KickPlayerById(HSQUIRRELVM v)
        {
            const SQChar* playerHandle = nullptr;
            const SQChar* reason = nullptr;

            sq_getstring(v, 2, &playerHandle);
            sq_getstring(v, 3, &reason);

            if (!VALID_CHARSTAR(playerHandle))
            {
                v_SQVM_ScriptError("Empty or null player handle");
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            // Discard empty strings, this will use the default message instead.
            if (!VALID_CHARSTAR(reason))
                reason = nullptr;

            g_BanSystem.KickPlayerById(playerHandle, reason);
            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }

        //-----------------------------------------------------------------------------
        // Purpose: bans a player by given name
        //-----------------------------------------------------------------------------
        SQRESULT BanPlayerByName(HSQUIRRELVM v)
        {
            const SQChar* playerName = nullptr;
            const SQChar* reason = nullptr;

            sq_getstring(v, 2, &playerName);
            sq_getstring(v, 3, &reason);

            if (!VALID_CHARSTAR(playerName))
            {
                v_SQVM_ScriptError("Empty or null player name");
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            // Discard empty strings, this will use the default message instead.
            if (!VALID_CHARSTAR(reason))
                reason = nullptr;

            g_BanSystem.BanPlayerByName(playerName, reason);
            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }

        //-----------------------------------------------------------------------------
        // Purpose: bans a player by given handle or id
        //-----------------------------------------------------------------------------
        SQRESULT BanPlayerById(HSQUIRRELVM v)
        {
            const SQChar* playerHandle = nullptr;
            const SQChar* reason = nullptr;

            sq_getstring(v, 2, &playerHandle);
            sq_getstring(v, 3, &reason);

            if (!VALID_CHARSTAR(playerHandle))
            {
                v_SQVM_ScriptError("Empty or null player handle");
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            // Discard empty strings, this will use the default message instead.
            if (!VALID_CHARSTAR(reason))
                reason = nullptr;

            g_BanSystem.BanPlayerById(playerHandle, reason);
            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }

          //-----------------------------------------------------------------------------
          // Purpose: adds an id to banlist
          //-----------------------------------------------------------------------------
            SQRESULT AddBanByID(HSQUIRRELVM v)
            {
                const SQChar* ip = nullptr;
                const SQChar* p_id = nullptr;

                sq_getstring(v, 2, &ip );
                sq_getstring(v, 3, &p_id );

                bool bResult = false;

                // Discard empty strings, this will use the default message instead.
                if (!VALID_CHARSTAR(ip))
                    ip = nullptr;

                // string to NucleusID_t
                char* endPtr = nullptr;
                NucleusID_t id = strtoull(p_id, &endPtr, 10);

                if (*endPtr != '\0') 
                {
                    bResult = false;
                }

                if (g_BanSystem.AddEntry(ip, id))
                {   
                    g_BanSystem.SaveList();
                    bResult = true;
                }


                sq_pushbool(v, bResult);
                return SQ_OK;

            }


        //-----------------------------------------------------------------------------
        // Purpose: unbans a player by given nucleus id or ip address
        //-----------------------------------------------------------------------------
        SQRESULT UnbanPlayer(HSQUIRRELVM v)
        {
            const SQChar* szCriteria = nullptr;
            sq_getstring(v, 2, &szCriteria);

            if (!VALID_CHARSTAR(szCriteria))
            {
                v_SQVM_ScriptError("Empty or null player criteria");
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            g_BanSystem.UnbanPlayer(szCriteria);

            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }

        //-----------------------------------------------------------------------------
        // Purpose: gets the number of real players on this server
        //-----------------------------------------------------------------------------
        SQRESULT GetNumHumanPlayers(HSQUIRRELVM v)
        {
            sq_pushinteger(v, g_pServer->GetNumHumanPlayers());
            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }

        //-----------------------------------------------------------------------------
        // Purpose: gets the number of fake players on this server
        //-----------------------------------------------------------------------------
        SQRESULT GetNumFakeClients(HSQUIRRELVM v)
        {
            sq_pushinteger(v, g_pServer->GetNumFakeClients());
            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }

        //-----------------------------------------------------------------------------
        // Purpose: gets the current server id
        //-----------------------------------------------------------------------------
        SQRESULT GetServerID(HSQUIRRELVM v)
        {
            sq_pushstring(v, g_LogSessionUUID.c_str(), (SQInteger)g_LogSessionUUID.length());
            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }

        //-----------------------------------------------------------------------------
        // Purpose: checks whether the server is active
        //-----------------------------------------------------------------------------
        SQRESULT IsServerActive(HSQUIRRELVM v)
        {
            bool isActive = g_pServer->IsActive();
            sq_pushbool(v, isActive);

            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }

        //-----------------------------------------------------------------------------
       // Generate / get usable matchID 
       //-----------------------------------------------------------------------------

        std::atomic<int64_t> g_MatchID{ 0 };

        //not exposed to sqvm
        void setMatchID(int64_t newID)
        {
            g_MatchID.store(newID);
        }

        //not exposed to sqvm
        int64_t getMatchID()
        {
            return g_MatchID.load();
        }


        // exposed to sqvm - retrieves matchID
        SQRESULT TrackerMatchID__internal(HSQUIRRELVM v)
        {
            std::string matchIDStr = std::to_string(getMatchID());
            sq_pushstring(v, matchIDStr.c_str(), -1);
            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }

        // set match ID
        int64_t selfSetMatchID()
        {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<int64_t> distr(0, INT64_MAX);

            int64_t randomNumber = distr(gen);
            std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
            time_t nowAsTimeT = std::chrono::system_clock::to_time_t(now);
            int64_t nowAsInt64 = static_cast<int64_t>(nowAsTimeT);

            int64_t newID = randomNumber + nowAsInt64;
            setMatchID(newID);

            return newID;
        }


        //-----------------------------------------------------------------------------
        // Purpose: mkos funni io logger
        //-----------------------------------------------------------------------------

        // Check of is currently running -- returns true if logging, false if not running
        SQRESULT TrackerIsLogging__internal(HSQUIRRELVM v)
        {
            bool state = LOGGER::Logger::getInstance().isLogging();
            sq_pushbool(v, state);
            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }

        SQRESULT TrackerGetLogState__internal(HSQUIRRELVM v)
        {
            SQInteger flag = NULL;

            if (SQ_SUCCEEDED(sq_getinteger(v, 2, &flag)))
            {
                LOGGER::Logger& logger = LOGGER::Logger::getInstance();
                LOGGER::Logger::LogState LogState = logger.intToLogState(flag);
                bool state = logger.getLogState(LogState);
                sq_pushbool(v, state);
            }
            else
            {
                Error(eDLL_T::SERVER, NO_ERROR, "SQ_ERROR: SQ_GetLogState");
                sq_pushbool(v, false);
            }

            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }



        SQRESULT TrackerLogEvent__internal(HSQUIRRELVM v)
        {
            const SQChar* logString = nullptr;
            SQBool encrypt = false;

            SQRESULT getStringResult = sq_getstring(v, 2, &logString);
            SQRESULT getBoolResult = sq_getbool(v, 3, &encrypt);

            if (SQ_SUCCEEDED(getStringResult) && SQ_SUCCEEDED(getBoolResult))
            {
                if (!VALID_CHARSTAR(logString))
                {
                    v_SQVM_ScriptError("Invalid CHARSTAR during logging.");
                    SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
                }

                LOGGER::pMkosLogger->LogEvent(logString, encrypt);
                SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
            }
            else
            {
                v_SQVM_ScriptError("Error retrieving parameters in %s", __FUNCTION__);
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }
        }


        SQRESULT InitializeTrackerLogThread__internal(HSQUIRRELVM v)
        {
            SQBool encrypt = false;
            if (getMatchID() == 0)
            {
                selfSetMatchID();
            }

            if (SQ_FAILED(sq_getbool(v, 2, &encrypt)))
            {
                v_SQVM_ScriptError("Failed to retrieve 'encrypt' parameter in %s", __FUNCTION__);
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            LOGGER::Logger& logger = LOGGER::Logger::getInstance();
            logger.InitializeLogThread(encrypt);

            sq_pushbool(v, true);
            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }



        SQRESULT TrackerStopLogging__internal(HSQUIRRELVM v)
        {
            SQBool sendToAPI = false;

            if (SQ_FAILED(sq_getbool(v, 2, &sendToAPI)))
            {
                v_SQVM_ScriptError("Failed to retrieve 'sendToAPI' parameter in %s", __FUNCTION__);
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            bool doSendToAPI = sendToAPI != 0;

            //DevMsg(eDLL_T::SERVER, "Send to API bool set to: %s \n", doSendToAPI ? "true" : "false");
            LOGGER::pMkosLogger->stopLogging(doSendToAPI);

            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }

        //-----------------------------------------------------------------------------
        // Purpose: deletes oldest logs after specified MB limit within specified 
        //          logfolder defined in settings json
        //-----------------------------------------------------------------------------

        SQRESULT TrackerCleanupLogs__internal(HSQUIRRELVM v)
        {
            LOGGER::CleanupLogs(FileSystem());
            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }

        //-----------------------------------------------------------------------------
        // Purpose: mkos debug - prints to console without devmode
        //-----------------------------------------------------------------------------

        SQRESULT sqprint__internal(HSQUIRRELVM v)
        {
            const SQChar* sqprintmsg = nullptr;

            if (SQ_FAILED( sq_getstring(v, 2, &sqprintmsg)) || !VALID_CHARSTAR(sqprintmsg))
            {
                v_SQVM_ScriptError("Failed to retrieve 'sqprintmsg' parameter in %s", __FUNCTION__);
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            std::string str = sqprintmsg;
            Msg(eDLL_T::SERVER, ":Tracker: %s\n", str.c_str());

            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }


        SQRESULT sqerror__internal(HSQUIRRELVM v)
        {
            const SQChar* sqprintmsg = nullptr;

            if (SQ_FAILED(sq_getstring(v, 2, &sqprintmsg) ) || !VALID_CHARSTAR( sqprintmsg) )
            {
                v_SQVM_ScriptError("Failed to retrieve 'sqprintmsg' parameter in %s.", __FUNCTION__);
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            std::string str = sqprintmsg;
            Error(eDLL_T::SERVER, NO_ERROR, ":Tracker: %s\n", str.c_str());

            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }

        SQRESULT sqwarning__internal(HSQUIRRELVM v)
        {
            const SQChar* sqprintmsg = nullptr;

            if (SQ_FAILED(sq_getstring(v, 2, &sqprintmsg)) || !VALID_CHARSTAR(sqprintmsg))
            {
                v_SQVM_ScriptError("Failed to retrieve 'sqprintmsg' parameter in %s", __FUNCTION__);
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            std::string str = sqprintmsg;
            Warning(eDLL_T::SERVER, ":Tracker: %s\n\n", str.c_str());

            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }

        void Sanitize_AlphaNumHyphenUnderscore(std::string& input)
        {
            input.erase
            (
                std::remove_if
                ( 
                    input.begin(), input.end(), []( unsigned char c ) 
                    {
                        return !std::isalnum(c) && c != '-' && c != '_';
                    }
                ),

                input.end()
            );
        }

        std::string Sanitize_NumbersOnly( const std::string& input )
        {
            std::string sanitized( input );
            sanitized.erase
            (
                std::remove_if
                ( 
                    sanitized.begin(), sanitized.end(), []( unsigned char c ) 
                    {
                        return !std::isdigit(c);
                    }
                ), 
                
                sanitized.end()
            );
            
            return sanitized;
        }

        //-----------------------------------------------------------------------------
        // Purpose: facilitates communication between sqvm and logger api calls 
        // for ea account verification
        //-----------------------------------------------------------------------------

        SQRESULT TrackerEAVerify__internal(HSQUIRRELVM v)
        {
            const SQChar* pToken = nullptr;
            const SQChar* pOID = nullptr;
            const SQChar* pEAName = nullptr;

            if (SQ_FAILED(sq_getstring(v, 2, &pToken)) || !pToken ||
                SQ_FAILED(sq_getstring(v, 3, &pOID)) || !pOID ||
                SQ_FAILED(sq_getstring(v, 4, &pEAName)) || !pEAName)
            {
                v_SQVM_ScriptError("Failed to retrieve parameters in %s", __FUNCTION__);
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            //We must make a copy here, because we are passing it to another thread, getting the stack object will not
            //matter even if we ref count it. This is because the server can reload / mapchange freeing the object
            //regardless of our ref counting.
            std::string token(pToken);
            std::string oid(pOID);
            std::string ea_name(pEAName);

            LOGGER::TaskManager::getInstance().AddTask
            (
                [ token, oid, ea_name ]()
                {
                    int32_t status_num = 0;
                    std::string status = LOGGER::VerifyEaAccount(token, oid, ea_name);

                    try {
                        status_num = std::stoi(status);
                    }
                    catch (const std::invalid_argument& e) {
                        Msg(eDLL_T::SERVER, "Error: Invalid argument for conversion: %s\n", e.what());
                    }
                    catch (const std::out_of_range& e) {
                        Msg(eDLL_T::SERVER, "Error: Value out of range for conversion: %s\n", e.what());
                    }
                    catch (...)
                    {
                        Msg(eDLL_T::SERVER, "Unknown error in ea_verify\n");
                    }

                    if (!g_pServer->IsActive())
                    {
                        std::string command = "CodeCallback_VerifyEaAccount(\"" + Sanitize_NumbersOnly(oid) + "\", " + status + ")";
                        g_TaskQueue.Dispatch
                        (
                            [command]
                            {
                                g_pServerScript->Run(command.c_str());
                            }
                            ,1 //delayed
                        );
                    }
                }
            );

            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }


        //-----------------------------------------------------------------------------
        // Purpose: api calls for stats
        //-----------------------------------------------------------------------------

        SQRESULT TrackerUpdatePlayerCount__internal(HSQUIRRELVM v)
        {
            const SQChar* action = nullptr;
            const SQChar* player = nullptr;
            const SQChar* OID = nullptr;
            const SQChar* count = nullptr;
            const SQChar* DISCORD_HOOK = nullptr;

            if (SQ_FAILED(sq_getstring(v, 2, &action)) || !action ||
                SQ_FAILED(sq_getstring(v, 3, &player)) || !player ||
                SQ_FAILED(sq_getstring(v, 4, &OID)) || !OID ||
                SQ_FAILED(sq_getstring(v, 5, &count)) || !count ||
                SQ_FAILED(sq_getstring(v, 6, &DISCORD_HOOK)) || !DISCORD_HOOK)
            {
                v_SQVM_ScriptError("Failed to retrieve parameters in %s", __FUNCTION__);
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            LOGGER::UPDATE_PLAYER_COUNT(action, player, OID, count, DISCORD_HOOK);
            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }


        SQRESULT TrackerEndMatchUpdate__internal(HSQUIRRELVM v)
        {
            const SQChar* recap = nullptr;
            const SQChar* DISCORD_HOOK = nullptr;

            if (SQ_FAILED(sq_getstring(v, 2, &recap)) || !recap ||
                SQ_FAILED(sq_getstring(v, 3, &DISCORD_HOOK)) || !DISCORD_HOOK)
            {
                v_SQVM_ScriptError("Failed to retrieve parameters in %s", __FUNCTION__);
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            LOGGER::NOTIFY_END_OF_MATCH(recap, DISCORD_HOOK);
            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }

        SQRESULT FetchPlayerPersistenceData__internal(HSQUIRRELVM v)
        {
            const SQChar* player_oid = nullptr;

            if ( sq_gettype(v, 2) != OT_STRING )
            {
                v_SQVM_ScriptError("First parameter is expected to be of type string, but %s was provided.\n", sq_typename(sq_gettype(v, 2)));
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }
            if (SQ_FAILED(sq_getstring(v, 2, &player_oid)) || !player_oid)
            {
                v_SQVM_ScriptError("Failed to retrieve 'player_oid' parameter.\n");
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            if (sq_gettype(v, 3) != OT_ARRAY)
            {
                v_SQVM_ScriptError("Second parameter is expected to be of type array. %s provided.\n", sq_typename(sq_gettype(v, 3)));
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }
            if (sq_gettype(v, 4) != OT_ARRAY)
            {
                v_SQVM_ScriptError("Third parameter is expected to be of type array. %s provided.\n", sq_typename(sq_gettype(v, 4)));
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            std::vector<std::string> requestedStats = SQArrayToVector<std::string>(v, 3);
            std::vector<std::string> requestedSettings = SQArrayToVector<std::string>(v, 4);

            LOGGER::TaskManager::getInstance().RequestPlayerPersistenceData(
                player_oid,
                requestedStats,
                requestedSettings
            );

            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }

        SQRESULT FetchBatchPersistenceData__internal(HSQUIRRELVM v)
        {
            if (sq_gettype(v, 2) != OT_ARRAY)
            {
                v_SQVM_ScriptError("First parameter is expected to be of type array. %s provided.\n", sq_typename(sq_gettype(v, 2)));
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }
            if (sq_gettype(v, 3) != OT_ARRAY)
            {
                v_SQVM_ScriptError("Second parameter is expected to be of type array. %s provided.\n", sq_typename(sq_gettype(v, 3)));
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }
            if (sq_gettype(v, 4) != OT_ARRAY)
            {
                v_SQVM_ScriptError("Third parameter is expected to be of type array. %s provided.\n", sq_typename(sq_gettype(v, 4)));
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            std::vector<std::string> playerOids = SQArrayToVector<std::string>(v, 2);
            std::vector<std::string> requestedStats = SQArrayToVector<std::string>(v, 3);
            std::vector<std::string> requestedSettings = SQArrayToVector<std::string>(v, 4);

            LOGGER::TaskManager::getInstance().RequestBatchPlayerPersistenceData(
                playerOids, requestedStats, requestedSettings);

            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }


        SQRESULT GetPlayerPersistenceData__internal(HSQUIRRELVM v)
        {
            const SQChar* player_oid = nullptr;

            if (SQ_FAILED(sq_getstring(v, 2, &player_oid)) || !player_oid)
            {
                v_SQVM_ScriptError("Failed to retrieve 'player_oid' parameter.\n");
                sq_pushinteger(v, -1);
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            const char* statsJson = LOGGER::GetPlayerJsonData(player_oid);

            if (strcmp(statsJson, "") == 0 || strcmp(statsJson, "NA") == 0)
            {
                sq_pushinteger(v, -1);
                SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
            }

            //Msg(eDLL_T::SERVER, "Received JSON: %s\n", statsJson);
            rapidjson::Document document;
            if (document.Parse(statsJson).HasParseError())
            {
                Error(eDLL_T::SERVER, NO_ERROR, "JSON parsing failed: %s\n", rapidjson::GetParseError_En(document.GetParseError()));
                sq_pushinteger(v, -1);
                SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
            }

            if (!document.IsObject())
            {
                Error(eDLL_T::SERVER, NO_ERROR, "JSON root is not an object\n");
                sq_pushinteger(v, -1);
                SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
            }

            sq_newtable(v);
            for (rapidjson::Value::ConstMemberIterator itr = document.MemberBegin(); itr != document.MemberEnd(); ++itr)
            {
                const char* key = itr->name.GetString();
                if (!key || key[0] == '\0')
                {
                    Msg(eDLL_T::SERVER, "Empty key in stats skipped\n");
                    continue;
                }

                sq_pushstring(v, key, -1);
                const rapidjson::Value& value = itr->value;

                if ( strcmp( key, "settings" ) == 0 && value.IsObject())
                {
                    //Msg(eDLL_T::SERVER, "Settings found. Iterating.\n");     
                    sq_newtable(v);
                    for (auto m = value.MemberBegin(); m != value.MemberEnd(); ++m)
                    {
                        const char* subKey = m->name.GetString();
                        sq_pushstring(v, subKey, -1);
                        const rapidjson::Value& subVal = m->value;
                        if (subVal.IsString())
                        {
                            //Msg(eDLL_T::SERVER, "settings[%s] = %s\n", subKey, subVal.GetString());
                            sq_pushstring(v, subVal.GetString(), -1);
                        }
                        else if (subVal.IsInt())
                            sq_pushinteger(v, subVal.GetInt());
                        else if (subVal.IsBool())
                            sq_pushbool(v, subVal.GetBool());
                        else if (subVal.IsFloat() || subVal.IsDouble())
                            sq_pushfloat(v, static_cast<float>(subVal.GetDouble()));
                        else
                        {
                            Msg(eDLL_T::SERVER, "Unknown value type in settings table. Skipping\n");
                            continue;
                        }

                        sq_newslot(v, -3);
                    }

                    sq_newslot(v, -3);
                }
                else if (value.IsArray())
                {
                    sq_newarray(v, 0);
                    int arrayType = -1;
                    auto getTypeName = [](int type) -> const char*
                        {
                            switch (type)
                            {
                                case 0: return "int";
                                case 1: return "string";
                                case 2: return "bool";
                                case 3: return "float";
                                case 4: return "object";
                                case 5: return "unknown";
                                default: return "none";
                            }
                        };

                    for (rapidjson::SizeType i = 0; i < value.Size(); i++)
                    {
                        const rapidjson::Value& elem = value[i];
                        int currentType = -1;
                        if (elem.IsInt())
                            currentType = 0;
                        else if (elem.IsString())
                            currentType = 1;
                        else if (elem.IsBool())
                            currentType = 2;
                        else if (elem.IsFloat() || elem.IsDouble())
                            currentType = 3;
                        else if (elem.IsObject())
                            currentType = 4;
                        else
                            currentType = 5;

                        if (arrayType == -1)
                            arrayType = currentType;
                        else if (currentType != arrayType)
                        {
                            Warning(eDLL_T::SERVER, "Key '%s': Cannot add element of type '%s' to array of type '%s'",
                                key, getTypeName(currentType), getTypeName(arrayType));
                            continue;
                        }

                        if (elem.IsInt())
                            sq_pushinteger(v, elem.GetInt());
                        else if (elem.IsString())
                            sq_pushstring(v, elem.GetString(), -1);
                        else if (elem.IsBool())
                            sq_pushbool(v, elem.GetBool());
                        else if (elem.IsFloat() || elem.IsDouble())
                            sq_pushfloat(v, static_cast<float>(elem.GetDouble()));
                        else if (elem.IsObject())
                        {
                            sq_newtable(v);
                            for (auto objItr = elem.MemberBegin(); objItr != elem.MemberEnd(); ++objItr)
                            {
                                const char* objKey = objItr->name.GetString();
                                if (!objKey || objKey[0] == '\0')
                                    continue;
                                sq_pushstring(v, objKey, -1);
                                const rapidjson::Value& objVal = objItr->value;
                                if (objVal.IsInt())
                                    sq_pushinteger(v, objVal.GetInt());
                                else if (objVal.IsString())
                                    sq_pushstring(v, objVal.GetString(), -1);
                                else if (objVal.IsBool())
                                    sq_pushbool(v, objVal.GetBool());
                                else if (objVal.IsFloat() || objVal.IsDouble())
                                    sq_pushfloat(v, static_cast<float>(objVal.GetDouble()));
                                else
                                    sq_pushstring(v, "Invalid Stat Array Value", -1);
                                sq_newslot(v, -3);
                            }
                        }
                        sq_arrayappend(v, -2);
                    }
                    sq_newslot(v, -3);
                }
                else
                {
                    if (value.IsInt())
                        sq_pushinteger(v, value.GetInt());
                    else if (value.IsString())
                        sq_pushstring(v, value.GetString(), -1);
                    else if (value.IsBool())
                        sq_pushbool(v, value.GetBool());
                    else if (value.IsFloat() || value.IsDouble())
                        sq_pushfloat(v, static_cast<float>(value.GetDouble()));
                    else
                        sq_pushstring(v, "Invalid value type", -1);

                    sq_newslot(v, -3);
                }
            }

            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }


        SQRESULT TrackerUpdateLiveStats__internal(HSQUIRRELVM v)
        {
            const SQChar* stats_json = nullptr;

            if (SQ_FAILED(sq_getstring(v, 2, &stats_json)))
            {
                v_SQVM_ScriptError("Failed to get stats_json\n");
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            std::string copyStatsJson(stats_json); //we must do this, as a thread is spawned to ship livestats

            LOGGER::UpdateLiveStats( copyStatsJson );
            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }



        SQRESULT TrackerResetStats__internal(HSQUIRRELVM v)
        {
            const SQChar* player_oid = nullptr;
            if (SQ_SUCCEEDED(sq_getstring(v, 2, &player_oid)) && player_oid)
            {
                LOGGER::TaskManager::getInstance().ResetPlayerData(player_oid);
                SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
            }

            SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
        }


        SQRESULT FetchGlobalTrackerSettings__internal(HSQUIRRELVM v)
        {
            const SQChar* query = nullptr;
            if (SQ_SUCCEEDED(sq_getstring(v, 2, &query)) && query)
            {
                if (std::strcmp(query, "") == 0)
                {
                    Error(eDLL_T::SERVER, NO_ERROR, "Query string was empty\n");
                    SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
                }

                SQObject queryObj;
                sq_getstackobj(v, 2, &queryObj);
                sq_addref(v, &queryObj);

                LOGGER::TaskManager::getInstance().AddTask([v, queryObj, query]() mutable
                {
                    std::string settings = LOGGER::FetchGlobalSettings(query);

                    bool success = CALL_SERVER_SCRIPT_FUNC("CodeCallback_TrackerGlobalSettingsReady",
                        settings.c_str(),
                        "void functionref( string )");
                        
                    if (!success)
                        Error(eDLL_T::SERVER, NO_ERROR, "Failed to execute CodeCallback_TrackerGlobalSettingsReady for query '%s'.\n", query);
                        
                    sq_release(v, &queryObj);
                });

                SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
            }

            v_SQVM_ScriptError("Failed to get Query string in %s", __FUNCTION__);
            SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
        }




        //-----------------------------------------------------------------------------
        // Purpose: fetches a setting value by key in the settings map. 
        // loaded from (r5rdev_config)
        //-----------------------------------------------------------------------------

        SQRESULT TrackerGetSetting__internal(HSQUIRRELVM v)
        {
            const SQChar* setting_key = nullptr;
            if (!SQ_SUCCEEDED(sq_getstring(v, 2, &setting_key)) || !VALID_CHARSTAR( setting_key))
            {
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            const char* setting_value = LOGGER::GetSetting(setting_key);
            sq_pushstring(v, setting_value, -1);
            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }


        SQRESULT TrackerReloadConfig__internal(HSQUIRRELVM v)
        {
            LOGGER::ReloadConfig("r5rdev_config.json");
            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }

        SQRESULT TrackerServerMsg__internal(HSQUIRRELVM v)
        {
            const SQChar* inMsg = nullptr;

            if (SQ_FAILED(sq_getstring(v, 2, &inMsg)) || !inMsg)
            {
                v_SQVM_ScriptError("Failed to get servermsg string");
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            SQInteger senderId = 0;

            if (SQ_FAILED(sq_getinteger(v, 3, &senderId)) || senderId < 0 || senderId > 255)
            {
                v_SQVM_ScriptError("Failed to get servermsg int or out of bounds");
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            const std::string msg(inMsg);

            if( g_pServer->IsActive() )
                CServerGameDLL::OnReceivedSayTextMessage(g_pServerGameDLL, static_cast<int>(senderId), msg.c_str(), false);

            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }



        SQRESULT TrackerCreateServerBot__internal(HSQUIRRELVM v)
        {
            if (!g_pServer->IsActive())
            {
                SCRIPT_CHECK_AND_RETURN(v, SQ_OK);

                //return array with -1 at element[0]
                sq_newarray(v, 0);
                sq_pushinteger(v, -1);
                sq_arrayappend(v, -2);
            }

            const SQChar* ImmutableName = nullptr;
            if (SQ_FAILED(sq_getstring(v, 2, &ImmutableName)) || !VALID_CHARSTAR( ImmutableName ))
            {
                v_SQVM_ScriptError("Failed to get server msgbot name");
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            std::vector<std::string> args = { "sv_addbot", "[" + std::string(ImmutableName) + "]", "1" };
            const char* c_args[3]{};

            for (size_t i = 0; i < args.size(); ++i)
            {
                c_args[i] = args[i].c_str();
            }

            CCommand cmd(3, c_args, cmd_source_t::kCommandSrcCode);
            CC_CreateFakePlayer_f(cmd);

            for (int i = 0; i < gpGlobals->maxClients; i++)
            {
                CClient* pClient = g_pServer->GetClient(i);

                if (!pClient)
                    continue;

                if (pClient->IsHumanPlayer())
                    continue;

                const CNetChan* pNetChan = pClient->GetNetChan();

                if (!pNetChan)
                    continue;

                const char* clientName = pNetChan->GetName();

                if (!clientName)
                    continue;

                std::string expectedBotName = "[" + std::string(ImmutableName) + "]"; //heaaaap

                if (strcmp(clientName, expectedBotName.c_str()) == 0)
                {
                    int ID = pClient->GetUserID();
                    if (ID >= 0 && ID <= 120)
                    {
                        sq_newarray(v, 0);
                        sq_pushinteger(v, static_cast<int>(pClient->GetHandle()));
                        sq_arrayappend(v, -2);
                        SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
                    }
                }
            }

            //return array with -1 at element[0]
            sq_newarray(v, 0);
            sq_pushinteger(v, -1);
            sq_arrayappend(v, -2);
            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }

        SQRESULT PrintStack(HSQUIRRELVM v)
        {
            SQInteger top = sq_gettop(v);
            for (SQInteger i = 1; i <= top; i++)
            {
                SQObjectType type = sq_gettype(v, i);
                const SQChar* typeName;
                const SQChar* name = nullptr;

                sq_getstring(v, i, &name); //idc

                switch (type) 
                {
                    case OT_NULL: typeName = "null"; break;
                    case OT_INTEGER: typeName = "integer"; break;
                    case OT_FLOAT: typeName = "float"; break;
                    case OT_BOOL: typeName = "bool"; break;
                    case OT_STRING: typeName = "string"; break;
                    case OT_TABLE: typeName = "table"; break;
                    case OT_ARRAY: typeName = "array"; break;
                    case OT_USERDATA: typeName = "userdata"; break;
                    case OT_CLOSURE: typeName = "closure"; break;
                    case OT_NATIVECLOSURE: typeName = "nativeclosure"; break;
                    case OT_USERPOINTER: typeName = "userpointer"; break;
                    case OT_THREAD: typeName = "thread"; break;
                    case OT_FUNCPROTO: typeName = "funcproto"; break;
                    case OT_CLASS: typeName = "class"; break;
                    case OT_INSTANCE: typeName = "instance"; break;
                    case OT_WEAKREF: typeName = "weakref"; break;
                    default: typeName = "unknown"; break;
                }
                Msg(eDLL_T::SERVER, "Stack [%d]: %s -- %s\n", i, name, typeName);
            }

            return SQ_OK;
        }
    } //namespace Server

    namespace PlayerEntity
    {
        //-----------------------------------------------------------------------------
        // Purpose: sets a class var on the server and each client
        //-----------------------------------------------------------------------------
        SQRESULT ScriptSetClassVar(HSQUIRRELVM v)
        {
            CPlayer* player = nullptr;

            if (!v_sq_getentity(v, (SQEntity*)&player))
                return SQ_ERROR;

            const SQChar* key = nullptr;
            sq_getstring(v, 2, &key);

            if (!VALID_CHARSTAR(key))
            {
                v_SQVM_ScriptError("Empty or null class key");
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            const SQChar* val = nullptr;
            sq_getstring(v, 3, &val);

            if (!VALID_CHARSTAR(val))
            {
                v_SQVM_ScriptError("Empty or null class value");
                SCRIPT_CHECK_AND_RETURN(v, SQ_ERROR);
            }

            CClient* const client = g_pServer->GetClient(player->GetEdict() - 1);
            SVC_SetClassVar msg(key, val);

            const bool success = client->SendNetMsgEx(&msg, false, true, false);

            if (success)
            {
                const char* pArgs[3] = {
                    "_setClassVarServer",
                    key,
                    val
                };

                const CCommand cmd((int)V_ARRAYSIZE(pArgs), pArgs, cmd_source_t::kCommandSrcCode);
                const int oldIdx = *g_nCommandClientIndex;

                *g_nCommandClientIndex = client->GetUserID();
                v__setClassVarServer_f(cmd);

                *g_nCommandClientIndex = oldIdx;
            }

            sq_pushbool(v, success);
            SCRIPT_CHECK_AND_RETURN(v, SQ_OK);
        }
    } //namespace PlayerEntity
}


//---------------------------------------------------------------------------------
// Purpose: registers script functions in SERVER context
// Input  : *s - 
//---------------------------------------------------------------------------------
void Script_RegisterServerFunctions(CSquirrelVM* s)
{
    Script_RegisterCommonAbstractions(s);
    Script_RegisterCoreServerFunctions(s);
    Script_RegisterAdminServerFunctions(s);

    Script_RegisterLiveAPIFunctions(s);
}

void Script_RegisterServerEnums(CSquirrelVM* const s)
{
    Script_RegisterLiveAPIEnums(s);
}

//---------------------------------------------------------------------------------
// Purpose: core server script functions
// Input  : *s - 
//---------------------------------------------------------------------------------
void Script_RegisterCoreServerFunctions(CSquirrelVM* s)
{
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, SetAutoReloadState, "Set whether we can auto-reload the server", "void", "bool canAutoReload");
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, GetServerID, "Gets the current server ID", "string", "");

    //for stat settings (api keys, discord webhooks, server identifiers, preferences))
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, TrackerGetSetting__internal, "Fetches value by key", "string", "string");
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, TrackerReloadConfig__internal, "Reloads R5R.DEV config file", "void", "");

    //for tracker 
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, InitializeTrackerLogThread__internal, "Initializes internal logevent thread", "void", "bool");
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, TrackerLogEvent__internal, "Logs event with GameEvent,Encryption", "void", "string, bool");
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, TrackerMatchID__internal, "Gets the match ID", "string", "");
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, TrackerStopLogging__internal, "Stops the logging thread, writes remaining queued messages", "void", "bool");
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, TrackerIsLogging__internal, "Checks if the log thread is running, atomic", "bool", "");
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, TrackerGetLogState__internal, "Checks various states, returns true false", "bool", "int");
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, TrackerCleanupLogs__internal, "Deletes oldest logs in platform/eventlogs when directory exceeds 20mb", "void", "");
    
    // for tracker api
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, FetchGlobalTrackerSettings__internal, "Fetches global settings based on query", "string", "string");
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, FetchBatchPersistenceData__internal, "Fetches batch player stats queries", "void", "array< string >, array< string >, array< string >"); //specify what stats|settings
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, FetchPlayerPersistenceData__internal, "Initializes grabbing stats for player", "void", "string, array< string >, array< string >"); //specify what stats|settings
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, GetPlayerPersistenceData__internal, "Gets stats table for player from native map", "table", "string");
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, TrackerResetStats__internal, "Sets map value for player_oid stats to empty string", "void", "string");
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, TrackerUpdateLiveStats__internal, "Updates live server stats R5R.DEV", "void", "string");
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, TrackerUpdatePlayerCount__internal, "Updates LIVE player count on R5R.DEV", "void", "string, string, string, string, string");
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, TrackerEndMatchUpdate__internal, "Updates match recap on R5R.DEV", "void", "string, string");
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, TrackerEAVerify__internal, "Verifys EA Account on R5R.DEV", "void", "string, string, string");
    
    // for debugging the sqvm
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, sqprint__internal, "Prints string to console window from sqvm", "void", "string");
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, sqerror__internal, "Prints error string to console window from sqvm", "void", "string");
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, sqwarning__internal, "Prints warning string to console window from sqvm", "void", "string");
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, PrintStack, "PRINT STACK", "void", "");

    //send a message as a bot. 
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, TrackerCreateServerBot__internal, "Creates a bot to send messages", "array< int >", "string");
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, TrackerServerMsg__internal, "Says message from specified senderId", "void", "string,int");
}

//---------------------------------------------------------------------------------
// Purpose: admin server script functions
// Input  : *s - 
//---------------------------------------------------------------------------------
void Script_RegisterAdminServerFunctions(CSquirrelVM* s)
{
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, GetNumHumanPlayers, "Gets the number of human players on the server", "int", "");
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, GetNumFakeClients, "Gets the number of bot players on the server", "int", "");

    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, KickPlayerByName, "Kicks a player from the server by name", "void", "string name, string reason");
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, KickPlayerById, "Kicks a player from the server by handle or nucleus id", "void", "string id, string reason");

    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, BanPlayerByName, "Bans a player from the server by name", "void", "string name, string reason");
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, BanPlayerById, "Bans a player from the server by handle or nucleus id", "void", "string id, string reason");

    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, AddBanByID, "Adds a player to banlist by ip & nucleus id, returns true for success", "bool", "string, string");
    DEFINE_SERVER_SCRIPTFUNC_NAMED(s, UnbanPlayer, "Unbans a player from the server by nucleus id or ip address", "void", "string handle");
}

//---------------------------------------------------------------------------------
// Purpose: script code class function registration
//---------------------------------------------------------------------------------
static void Script_RegisterServerEntityClassFuncs()
{
    v_Script_RegisterServerEntityClassFuncs();
    static bool initialized = false;

    if (initialized)
        return;

    initialized = true;
}
//---------------------------------------------------------------------------------
static void Script_RegisterServerPlayerClassFuncs()
{
    v_Script_RegisterServerPlayerClassFuncs();
    static bool initialized = false;

    if (initialized)
        return;

    initialized = true;

    g_serverScriptPlayerStruct->AddFunction("SetClassVar",
        "ScriptSetClassVar",
        "Change a variable in the player's class settings",
        "bool",
        "string key, string value",
        VScriptCode::PlayerEntity::ScriptSetClassVar);
}
//---------------------------------------------------------------------------------
static void Script_RegisterServerAIClassFuncs()
{
    v_Script_RegisterServerAIClassFuncs();
    static bool initialized = false;

    if (initialized)
        return;

    initialized = true;
}
//---------------------------------------------------------------------------------
static void Script_RegisterServerWeaponClassFuncs()
{
    v_Script_RegisterServerWeaponClassFuncs();
    static bool initialized = false;

    if (initialized)
        return;

    initialized = true;
}
//---------------------------------------------------------------------------------
static void Script_RegisterServerProjectileClassFuncs()
{
    v_Script_RegisterServerProjectileClassFuncs();
    static bool initialized = false;

    if (initialized)
        return;

    initialized = true;
}
//---------------------------------------------------------------------------------
static void Script_RegisterServerTitanSoulClassFuncs()
{
    v_Script_RegisterServerTitanSoulClassFuncs();
    static bool initialized = false;

    if (initialized)
        return;

    initialized = true;
}
//---------------------------------------------------------------------------------
static void Script_RegisterServerPlayerDecoyClassFuncs()
{
    v_Script_RegisterServerPlayerDecoyClassFuncs();
    static bool initialized = false;

    if (initialized)
        return;

    initialized = true;
}
//---------------------------------------------------------------------------------
static void Script_RegisterServerSpawnpointClassFuncs()
{
    v_Script_RegisterServerSpawnpointClassFuncs();
    static bool initialized = false;

    if (initialized)
        return;

    initialized = true;
}
//---------------------------------------------------------------------------------
static void Script_RegisterServerFirstPersonProxyClassFuncs()
{
    v_Script_RegisterServerFirstPersonProxyClassFuncs();
    static bool initialized = false;

    if (initialized)
        return;

    initialized = true;
}

void VScriptServer::Detour(const bool bAttach) const
{
    DetourSetup(&v_Script_RegisterServerEntityClassFuncs, &Script_RegisterServerEntityClassFuncs, bAttach);
    DetourSetup(&v_Script_RegisterServerPlayerClassFuncs, &Script_RegisterServerPlayerClassFuncs, bAttach);
    DetourSetup(&v_Script_RegisterServerAIClassFuncs, &Script_RegisterServerAIClassFuncs, bAttach);
    DetourSetup(&v_Script_RegisterServerWeaponClassFuncs, &Script_RegisterServerWeaponClassFuncs, bAttach);
    DetourSetup(&v_Script_RegisterServerProjectileClassFuncs, &Script_RegisterServerProjectileClassFuncs, bAttach);
    DetourSetup(&v_Script_RegisterServerTitanSoulClassFuncs, &Script_RegisterServerTitanSoulClassFuncs, bAttach);
    DetourSetup(&v_Script_RegisterServerPlayerDecoyClassFuncs, &Script_RegisterServerPlayerDecoyClassFuncs, bAttach);
    DetourSetup(&v_Script_RegisterServerSpawnpointClassFuncs, &Script_RegisterServerSpawnpointClassFuncs, bAttach);
    DetourSetup(&v_Script_RegisterServerFirstPersonProxyClassFuncs, &Script_RegisterServerFirstPersonProxyClassFuncs, bAttach);
}
