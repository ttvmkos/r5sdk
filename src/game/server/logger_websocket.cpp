#pragma once
#ifndef CLIENT_DLL
#include "core/stdafx.h"
#include "logger_websocket.h"
#include "logger.h"
#include "networksystem/bansystem.h"
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "engine/server/server.h"
#include "engine/client/client.h"
#include "tier1/fmtstr.h"
#include "tier0/dbg.h"
#include "tier2/websocket.h"
#include <game/server/vscript_server.h> //required for FileSystem()

namespace LOGGER
{
    //===========================================================================
    // Singleton Instance
    //===========================================================================

    WebSocketCommandHandler& WebSocketCommandHandler::getInstance()
    {
        static WebSocketCommandHandler instance;
        return instance;
    }

    //===========================================================================
    // Constructor / Destructor
    //===========================================================================

    WebSocketCommandHandler::WebSocketCommandHandler()
        : m_webSocket(nullptr)
        , m_serverPort(tracker_ws_port.GetInt())
        , m_isConnected(false)
        , m_initialized(false)
        , m_lastUpdateTime(0.0)
        , m_lastConnectAttempt(0.0)
        , m_cachedApiKey("")
        , m_configDirty(false)
        , m_receiveBuffer(RECEIVE_BUFFER_SIZE) //255kb max (heap)
        , m_messageCount(0)
        , m_connectedAddress("")
    {
    }

    WebSocketCommandHandler::~WebSocketCommandHandler()
    {
        Disconnect();
    }

    //===========================================================================
    // Connection Management
    //===========================================================================

    bool WebSocketCommandHandler::Connect(const char* address, int port)
    {
        if (!address || address[0] == '\0')
        {
            Error(eDLL_T::SERVER, NO_ERROR,
                "TrackerSocket: Invalid address provided\n");
            return false;
        }

        m_serverAddress = address;
        m_serverPort = port;

        if (!m_initialized.load())
        {
            m_webSocket = std::make_unique<CWebSocket>();

            CWebSocket::ConnParams_s params;
            params.bufSize = tracker_ws_buffer_size.GetInt(); //was 4096, now 256kb ( 262144 )
            params.retryTime = tracker_ws_retry_time.GetFloat();
            params.maxRetries = tracker_ws_max_retries.GetInt();
            params.timeOut = tracker_ws_time_out.GetInt();
            params.keepAlive = tracker_ws_keep_alive.GetInt();
            params.laxSSL = tracker_ws_lax_ssl.GetInt();

            std::string addressStr;
            if (tracker_ws_use_ssl.GetBool())
                addressStr = CFmtStr("wss://%s:%d", address, port).Get();
            else 
				addressStr = CFmtStr("ws://%s:%d", address, port).Get();

            const char* errorMsg = nullptr;

            if (!m_webSocket->Init(addressStr.c_str(), params, errorMsg))
            {
                Error(eDLL_T::SERVER, NO_ERROR,
                    "TrackerSocket: Failed to initialize: %s\n",
                    errorMsg ? errorMsg : "Unknown error");
                return false;
            }

			m_connectedAddress = addressStr;
            m_initialized.store(true);
            Msg(eDLL_T::SERVER, "TrackerSocket: Initialized (address=%s, port=%d)\n",
                address, port);
        }

        m_lastConnectAttempt = Plat_FloatTime();
        m_isConnected.store(true);  // Assume connected until timeout proves otherwise

        if (tracker_ws_debug.GetBool())
        {
            Msg(eDLL_T::SERVER, "TrackerSocket: Connection attempt to %s:%d (will timeout in 15 seconds if server unreachable)\n",
                address, port);
        }

        return true;
    }

    void WebSocketCommandHandler::Disconnect()
    {
        m_isConnected.store(false);

        if (m_webSocket)
        {
            m_webSocket->DisconnectAll();
        }

        Msg(eDLL_T::SERVER, "TrackerSocket: Disconnected\n");
    }

    bool WebSocketCommandHandler::IsConnected() const
    {
        return m_isConnected.load();
    }

    void WebSocketCommandHandler::Update()
    {
        if (!m_initialized.load() || !m_webSocket)
            return;

        m_webSocket->Update();

        // Check if connection attempt has timed out (15 seconds)
        double timeSinceAttempt = Plat_FloatTime() - m_lastConnectAttempt;

        if (timeSinceAttempt > 15.0 && !m_isConnected.load() && m_lastConnectAttempt > 0.0)
        {
            // Initial connection attempt failed - server unreachable
            Error(eDLL_T::SERVER, NO_ERROR,
                "TrackerSocket: Failed to connect to %s:%d after 15 seconds (server unreachable)\n",
                m_serverAddress.c_str(), m_serverPort);
            m_lastConnectAttempt = 0.0;  // Reset so we don't spam errors
        }

        int32_t received = m_webSocket->ReceiveData(m_receiveBuffer.data(), RECEIVE_BUFFER_SIZE);
        if (received > 0)
        {
            std::string incomingMsg(m_receiveBuffer.data(), received);
            OnMessageReceived(incomingMsg);
        }

        // Send queued responses only if we believe connection succeeded
        // (No need to check m_isConnected here since SendData() silently fails on disconnected sockets)
        {
            std::lock_guard<std::shared_timed_mutex> lock(m_queueMutex);
            while (!m_responseQueue.empty())
            {
                const std::string& response = m_responseQueue.front();
                m_webSocket->SendData(response.c_str(),
                    static_cast<int32_t>(response.length()));
                m_responseQueue.pop();
            }
        }

        m_lastUpdateTime = Plat_FloatTime();
    }

    void WebSocketCommandHandler::Shutdown()
    {
        Disconnect();
        m_initialized.store(false);
    }

    //===========================================================================
    // Message Reception
    //===========================================================================

    void WebSocketCommandHandler::OnMessageReceived(const std::string& rawMessage)
    {
        m_messageCount++;

        if (rawMessage.empty())
        {
            Error(eDLL_T::SERVER, NO_ERROR,
                "TrackerSocket: Received empty message\n");
            return;
        }

        rapidjson::Document doc;
        doc.Parse(rawMessage.c_str());

        if (doc.HasParseError())
        {
            Error(eDLL_T::SERVER, NO_ERROR,
                "TrackerSocket: Invalid JSON received (offset %zu): %s\n",
                doc.GetErrorOffset(),
                rapidjson::GetParseError_En(doc.GetParseError()));
            return;
        }

        // Validate request format
        std::string validationError;
        if (!ValidateMessage(doc, validationError))
        {
            Error(eDLL_T::SERVER, NO_ERROR,
                "TrackerSocket: Message validation failed: %s\n",
                validationError.c_str());
            return;
        }

        // Authenticate request
        if (!AuthenticateMessage(doc, validationError))
        {
            SendResponse(doc["id"].GetString(), "error", nullptr,
                "Authentication failed");

            Error(eDLL_T::SERVER, NO_ERROR,
                "TrackerSocket: Message authentication failed: %s\n",
                validationError.c_str());
            return;
        }

        // Queue for processing
        {
            std::lock_guard<std::shared_timed_mutex> lock(m_queueMutex);

            PendingMessage_t msg;
            msg.id = doc["id"].GetString();
            msg.type = doc["type"].GetString();
            msg.rawJson = rawMessage;
            msg.receivedTime = Plat_FloatTime();
            msg.retryCount = 0;

            m_messageQueue.push(msg);

            if (tracker_ws_debug.GetBool())
            {
                Msg(eDLL_T::SERVER, "TrackerSocket: Message queued (type=%s, id=%s)\n",
                    msg.type.c_str(), msg.id.c_str());
            }
        }
    }

    void WebSocketCommandHandler::ProcessMessageQueue()
    {
        std::lock_guard<std::shared_timed_mutex> lock(m_queueMutex);

        while (!m_messageQueue.empty())
        {
            const PendingMessage_t& msg = m_messageQueue.front();

            // Parse the stored JSON
            rapidjson::Document doc;
            doc.Parse(msg.rawJson.c_str());

            if (!doc.HasParseError())
            {
                // Queue async task for command execution
                std::string msgId = msg.id;
                std::string msgType = msg.type;
                std::string rawJson = msg.rawJson;

                std::function<void()> task = [this, msgId, rawJson]() {
                    rapidjson::Document cmdDoc;
                    cmdDoc.Parse(rawJson.c_str());

                    if (!cmdDoc.HasParseError())
                    {
                        DispatchCommand(cmdDoc, msgId);
                    }
                    };

                TaskManager::getInstance().AddTask(task);
            }
            else
            {
                Error(eDLL_T::SERVER, NO_ERROR,
                    "TrackerSocket: Failed to reparse queued message\n");
            }

            m_messageQueue.pop();
        }
    }

    //===========================================================================
    // Command Dispatching (Switch Case - Efficient)
    //===========================================================================

    WebSocketCommandHandler::CommandType_e WebSocketCommandHandler::GetCommandType(
        const std::string& typeStr)
    {
        // Use static hash map for O(1) lookup (if needed for many commands)
        if (typeStr == "kick_player")
            return CommandType_e::KICK_PLAYER;
        else if (typeStr == "ban_player")
            return CommandType_e::BAN_PLAYER;
        else if (typeStr == "unban_player")
            return CommandType_e::UNBAN_PLAYER;
        else if (typeStr == "get_banlist")
            return CommandType_e::GET_BANLIST;
        else if (typeStr == "get_players")
            return CommandType_e::GET_PLAYERS;
        else if (typeStr == "get_config")
            return CommandType_e::GET_CONFIG;
        else if (typeStr == "get_stats")
            return CommandType_e::GET_STATS;
        else if (typeStr == "reload_config")
            return CommandType_e::RELOAD_CONFIG;
        else if (typeStr == "update_config")
            return CommandType_e::UPDATE_CONFIG;
        else if (typeStr == "reload_banlist")
            return CommandType_e::RELOAD_BANLIST;
        else

            return CommandType_e::UNKNOWN;
    }

    void WebSocketCommandHandler::DispatchCommand(const rapidjson::Document& doc,
        const std::string& requestId)
    {
        const std::string& typeStr = doc["type"].GetString();
        const auto& params = doc["params"];

        CommandType_e cmdType = GetCommandType(typeStr);

        switch (cmdType)
        {
        case CommandType_e::KICK_PLAYER:
            HandleKickCommand(params, requestId);
            break;

        case CommandType_e::BAN_PLAYER:
            HandleBanCommand(params, requestId);
            break;

        case CommandType_e::UNBAN_PLAYER:
            HandleUnbanCommand(params, requestId);
            break;

        case CommandType_e::GET_BANLIST:
            HandleGetBanlistCommand(requestId);
            break;

        case CommandType_e::GET_PLAYERS:
            HandleGetPlayersCommand(requestId);
            break;

        case CommandType_e::GET_CONFIG:
            HandleGetConfigCommand(params, requestId);
            break;

        case CommandType_e::GET_STATS:
            HandleGetStatsCommand(requestId);
            break;

        case CommandType_e::RELOAD_CONFIG:
            HandleReloadConfigCommand(requestId);
            break;

        case CommandType_e::UPDATE_CONFIG:
            HandleUpdateConfigCommand(params, requestId);
            break;

        case CommandType_e::RELOAD_BANLIST:
            HandleReloadBanlistCommand(requestId);
            break;

        case CommandType_e::UNKNOWN:
        default:
            SendResponse(requestId, "error", nullptr,
                "Unknown command type");
            Error(eDLL_T::SERVER, NO_ERROR,
                "TrackerSocket: Unknown command type: %s\n", typeStr.c_str());
            break;
        }
    }

    //===========================================================================
    // Command Handlers
    //===========================================================================

    void WebSocketCommandHandler::HandleKickCommand(const rapidjson::Value& params,
        const std::string& requestId)
    {
        if (!params.HasMember("player_name") || !params["player_name"].IsString())
        {
            SendResponse(requestId, "error", nullptr,
                "Missing required field: player_name");
            return;
        }

        std::string playerName = params["player_name"].GetString();
        std::string reason = (params.HasMember("reason") &&
            params["reason"].IsString()) ?
            params["reason"].GetString() : "";

        std::function<void()> task = [this, playerName, reason, requestId]() {
            try {
                g_BanSystem.KickPlayerByName(playerName.c_str(),
                    reason.empty() ? nullptr : reason.c_str());

                rapidjson::Document response;
                response.SetObject();
                auto& alloc = response.GetAllocator();

                rapidjson::Value data(rapidjson::kObjectType);
                data.AddMember("player_kicked", true, alloc);
                data.AddMember("player_name",
                    rapidjson::Value(playerName.c_str(), alloc), alloc);

                SendResponse(requestId, "success", &data,
                    "Player kicked successfully");
            }
            catch (const std::exception& e) {
                SendResponse(requestId, "error", nullptr,
                    std::string("Failed to kick player: ") + e.what());
            }
            };

        TaskManager::getInstance().AddTask(task);

        if (tracker_ws_debug.GetBool())
        {
            Msg(eDLL_T::SERVER, "TrackerSocket: Kick command queued for %s\n",
                playerName.c_str());
        }
    }

    void WebSocketCommandHandler::HandleBanCommand(const rapidjson::Value& params,
        const std::string& requestId)
    {
        if (!params.HasMember("player_name") || !params["player_name"].IsString())
        {
            SendResponse(requestId, "error", nullptr,
                "Missing required field: player_name");
            return;
        }

        std::string playerName = params["player_name"].GetString();
        std::string reason = (params.HasMember("reason") &&
            params["reason"].IsString()) ?
            params["reason"].GetString() : "";

        std::function<void()> task = [this, playerName, reason, requestId]() {
            try {
                g_BanSystem.BanPlayerByName(playerName.c_str(),
                    reason.empty() ? nullptr : reason.c_str());

                rapidjson::Document response;
                response.SetObject();
                auto& alloc = response.GetAllocator();

                rapidjson::Value data(rapidjson::kObjectType);
                data.AddMember("player_banned", true, alloc);
                data.AddMember("player_name",
                    rapidjson::Value(playerName.c_str(), alloc), alloc);

                SendResponse(requestId, "success", &data,
                    "Player banned successfully");

                Msg(eDLL_T::SERVER, "TrackerSocket: Player %s banned\n",
                    playerName.c_str());
            }
            catch (const std::exception& e) {
                SendResponse(requestId, "error", nullptr,
                    std::string("Failed to ban player: ") + e.what());
            }
            };

        TaskManager::getInstance().AddTask(task);
        if (tracker_ws_debug.GetBool())
        {
            Msg(eDLL_T::SERVER, "TrackerSocket: Ban command queued for %s\n",
                playerName.c_str());
        }
    }

    void WebSocketCommandHandler::HandleUnbanCommand(const rapidjson::Value& params,
        const std::string& requestId)
    {
        if (!params.HasMember("player_name") && !params.HasMember("player_id"))
        {
            SendResponse(requestId, "error", nullptr,
                "Missing required field: player_name or player_id");
            return;
        }

        std::string criteria = (params.HasMember("player_name") &&
            params["player_name"].IsString()) ?
            params["player_name"].GetString() :
            params["player_id"].GetString();

        std::function<void()> task = [this, criteria, requestId]() {
            try {
                g_BanSystem.UnbanPlayer(criteria.c_str());

                rapidjson::Document response;
                response.SetObject();
                auto& alloc = response.GetAllocator();

                rapidjson::Value data(rapidjson::kObjectType);
                data.AddMember("player_unbanned", true, alloc);
                data.AddMember("criteria",
                    rapidjson::Value(criteria.c_str(), alloc), alloc);

                SendResponse(requestId, "success", &data,
                    "Player unbanned successfully");

                Msg(eDLL_T::SERVER, "TrackerSocket: Player %s unbanned\n",
                    criteria.c_str());
            }
            catch (const std::exception& e) {
                SendResponse(requestId, "error", nullptr,
                    std::string("Failed to unban player: ") + e.what());
            }
            };

        TaskManager::getInstance().AddTask(task);
        if (tracker_ws_debug.GetBool())
        {
            Msg(eDLL_T::SERVER, "TrackerSocket: Unban command queued for %s\n",
                criteria.c_str());
        }
    }

    void WebSocketCommandHandler::HandleGetBanlistCommand(const std::string& requestId)
    {
        std::function<void()> task = [this, requestId]() {
            try {
                // Load banlist.json file directly
                FileHandle_t pFile = FileSystem()->Open("banlist.json", "rb", "PLATFORM");
                if (!pFile)
                {
                    // No ban file exists - return empty list
                    rapidjson::Document response;
                    response.SetObject();
                    rapidjson::Value entries(rapidjson::kArrayType);
                    SendResponse(requestId, "success", &entries, "Banlist retrieved (empty)");
                    return;
                }

                const ssize_t nFileSize = FileSystem()->Size(pFile);
                if (nFileSize <= 0)
                {
                    FileSystem()->Close(pFile);
                    rapidjson::Document response;
                    response.SetObject();
                    rapidjson::Value entries(rapidjson::kArrayType);
                    SendResponse(requestId, "success", &entries, "Banlist retrieved (empty)");
                    return;
                }

                // Read file into buffer
                const u64 nBufSize = FileSystem()->GetOptimalReadSize(pFile, nFileSize + 2);
                char* const pBuf = (char*)FileSystem()->AllocOptimalReadBuffer(pFile, nBufSize, 0);

                const ssize_t nRead = FileSystem()->ReadEx(pBuf, nBufSize, nFileSize, pFile);
                FileSystem()->Close(pFile);

                if (nRead == 0)
                {
                    FileSystem()->FreeOptimalReadBuffer(pBuf);
                    rapidjson::Document response;
                    response.SetObject();
                    rapidjson::Value entries(rapidjson::kArrayType);
                    SendResponse(requestId, "success", &entries, "Banlist retrieved (empty)");
                    return;
                }

                pBuf[nFileSize] = '\0';

                // Parse JSON
                rapidjson::Document banlistDoc;
                banlistDoc.Parse(pBuf, nRead);
                FileSystem()->FreeOptimalReadBuffer(pBuf);

                if (banlistDoc.HasParseError())
                {
                    Error(eDLL_T::SERVER, NO_ERROR,
                        "TrackerSocket: Failed to parse banlist.json at offset %zu\n",
                        banlistDoc.GetErrorOffset());
                    SendResponse(requestId, "error", nullptr, "Failed to parse ban list file");
                    return;
                }

                // Extract entries from the JSON
                rapidjson::Document response;
                response.SetObject();
                auto& alloc = response.GetAllocator();
                rapidjson::Value entries(rapidjson::kArrayType);

                if (banlistDoc.IsObject() && banlistDoc.HasMember("entries") &&
                    banlistDoc["entries"].IsArray())
                {
                    for (const auto& entry : banlistDoc["entries"].GetArray())
                    {
                        if (entry.IsObject())
                        {
                            rapidjson::Value newEntry(rapidjson::kObjectType);

                            // Copy all relevant fields from entry
                            if (entry.HasMember("nucleusId") && entry["nucleusId"].IsUint64())
                                newEntry.AddMember("nucleusId", entry["nucleusId"].GetUint64(), alloc);

                            if (entry.HasMember("playerName") && entry["playerName"].IsString())
                                newEntry.AddMember("playerName",
                                    rapidjson::Value(entry["playerName"].GetString(), alloc), alloc);

                            if (entry.HasMember("ipAddress") && entry["ipAddress"].IsString())
                                newEntry.AddMember("ipAddress",
                                    rapidjson::Value(entry["ipAddress"].GetString(), alloc), alloc);

                            if (entry.HasMember("banReason") && entry["banReason"].IsString())
                                newEntry.AddMember("banReason",
                                    rapidjson::Value(entry["banReason"].GetString(), alloc), alloc);

                            if (entry.HasMember("banTimestamp") && entry["banTimestamp"].IsInt64())
                                newEntry.AddMember("banTimestamp", entry["banTimestamp"].GetInt64(), alloc);

                            if (entry.HasMember("banExpiryTimestamp") && entry["banExpiryTimestamp"].IsInt64())
                                newEntry.AddMember("banExpiryTimestamp", entry["banExpiryTimestamp"].GetInt64(), alloc);

                            if (entry.HasMember("banType") && entry["banType"].IsInt())
                                newEntry.AddMember("banType", entry["banType"].GetInt(), alloc);

                            entries.PushBack(newEntry, alloc);
                        }
                    }
                }

                SendResponse(requestId, "success", &entries,
                    CFmtStr("Banlist retrieved (%zu bans)", entries.Size()).Get());

                Msg(eDLL_T::SERVER, "TrackerSocket: Banlist sent with %zu entries\n", entries.Size());
            }
            catch (const std::exception& e) {
                SendResponse(requestId, "error", nullptr,
                    std::string("Failed to get banlist: ") + e.what());
                Error(eDLL_T::SERVER, NO_ERROR,
                    "TrackerSocket: Exception in HandleGetBanlistCommand: %s\n", e.what());
            }
            };

        TaskManager::getInstance().AddTask(task);
        if (tracker_ws_debug.GetBool())
        {
            Msg(eDLL_T::SERVER, "TrackerSocket: Get banlist command queued\n");
        }
    }

    void WebSocketCommandHandler::HandleGetPlayersCommand(const std::string& requestId)
    {
        std::function<void()> task = [this, requestId]() {
            try {
                rapidjson::Document response;
                response.SetObject();
                auto& alloc = response.GetAllocator();

                rapidjson::Value players(rapidjson::kArrayType);

                // Get active player list from server
                if (!g_pServer)
                {
                    SendResponse(requestId, "success", &players, "No server running");
                    return;
                }

                const int nMaxClients = g_pServer->GetMaxClients();

                // Iterate through all client slots
                for (int i = 0; i < nMaxClients; i++)
                {
                    CClient* const pClient = g_pServer->GetClient(i);
                    if (!pClient)
                        continue;

                    // Only include connected/spawned players
                    if (!pClient->IsConnected())
                        continue;

                    const CNetChan* const pNetChan = pClient->GetNetChan();
                    if (!pNetChan)
                        continue;

                    // Create player entry
                    rapidjson::Value playerEntry(rapidjson::kObjectType);

                    // Add player information
                    playerEntry.AddMember("user_id", pClient->GetUserID(), alloc);
                    playerEntry.AddMember("nucleus_id", pClient->GetNucleusID(), alloc);
                    playerEntry.AddMember("handle", pClient->GetHandle(), alloc);
                    playerEntry.AddMember("team", pClient->GetTeamNum(), alloc);
                    playerEntry.AddMember("player_name",
                        rapidjson::Value(pNetChan->GetName(), alloc), alloc);
                    playerEntry.AddMember("is_bot", pClient->IsFakeClient(), alloc);
                    playerEntry.AddMember("is_active", pClient->IsActive(), alloc);
                    playerEntry.AddMember("is_spawned", pClient->IsSpawned(), alloc);

                    // Add network address (if available)
                    const netadr_t& remoteAddr = pNetChan->GetRemoteAddress();
                    char adrStr[64];
                    remoteAddr.ToString(adrStr, sizeof(adrStr));
                    playerEntry.AddMember("address",
                        rapidjson::Value(adrStr, alloc), alloc);

                    // Add signon state
                    int nSignonState = static_cast<int>(pClient->GetSignonState());
                    playerEntry.AddMember("signon_state", nSignonState, alloc);

                    players.PushBack(playerEntry, alloc);
                }

                SendResponse(requestId, "success", &players,
                    CFmtStr("Player list retrieved (%zu players)", players.Size()).Get());

                Msg(eDLL_T::SERVER, "TrackerSocket: Player list sent with %zu players\n", players.Size());
            }
            catch (const std::exception& e) {
                SendResponse(requestId, "error", nullptr,
                    std::string("Failed to get players: ") + e.what());
                Error(eDLL_T::SERVER, NO_ERROR,
                    "TrackerSocket: Exception in HandleGetPlayersCommand: %s\n", e.what());
            }
            };

        TaskManager::getInstance().AddTask(task);
        if (tracker_ws_debug.GetBool())
        {
            Msg(eDLL_T::SERVER, "TrackerSocket: Get players command queued\n");
        }
    }

    void WebSocketCommandHandler::HandleGetConfigCommand(const rapidjson::Value& params,
        const std::string& requestId)
    {
        std::vector<std::string> keys;

        if (params.HasMember("keys") && params["keys"].IsArray())
        {
            for (const auto& key : params["keys"].GetArray())
            {
                if (key.IsString())
                {
                    keys.push_back(key.GetString());
                }
            }
        }

        Msg(eDLL_T::SERVER, "TrackerSocket: Config request with %zu keys\n", keys.size());

        std::function<void()> task = [this, keys, requestId]() {
            try {
                rapidjson::Document response;
                response.SetObject();
                auto& alloc = response.GetAllocator();

                rapidjson::Value config(rapidjson::kObjectType);

                for (const auto& key : keys)
                {
                    std::string value = GetSetting(key.c_str());
                    config.AddMember(
                        rapidjson::Value(key.c_str(), alloc),
                        rapidjson::Value(value.c_str(), alloc),
                        alloc);
                }

                SendResponse(requestId, "success", &config,
                    "Config values retrieved");
            }
            catch (const std::exception& e) {
                SendResponse(requestId, "error", nullptr,
                    std::string("Failed to get config: ") + e.what());
            }
            };

        TaskManager::getInstance().AddTask(task);
        if (tracker_ws_debug.GetBool())
        {
            Msg(eDLL_T::SERVER, "TrackerSocket: Get config command queued\n");
        }
    }

    void WebSocketCommandHandler::HandleGetStatsCommand(const std::string& requestId)
    {
        std::function<void()> task = [this, requestId]() {
            try {
                rapidjson::Document response;
                response.SetObject();
                auto& alloc = response.GetAllocator();

                rapidjson::Value stats(rapidjson::kObjectType);
                stats.AddMember("message_count", m_messageCount.load(), alloc);
                stats.AddMember("uptime_seconds", Plat_FloatTime(), alloc);

                SendResponse(requestId, "success", &stats,
                    "Stats retrieved");
            }
            catch (const std::exception& e) {
                SendResponse(requestId, "error", nullptr,
                    std::string("Failed to get stats: ") + e.what());
            }
            };

        TaskManager::getInstance().AddTask(task);
        if (tracker_ws_debug.GetBool())
        {
            Msg(eDLL_T::SERVER, "TrackerSocket: Get stats command queued\n");
        }
    }

    void WebSocketCommandHandler::HandleReloadConfigCommand(const std::string& requestId)
    {
        std::function<void()> task = [this, requestId]() {
            try {
                ReloadConfig("r5rdev_config.json");

                rapidjson::Document response;
                response.SetObject();
                auto& alloc = response.GetAllocator();

                rapidjson::Value data(rapidjson::kObjectType);
                data.AddMember("config_reloaded", true, alloc);

                SendResponse(requestId, "success", &data,
                    "Config reloaded successfully");

                Msg(eDLL_T::SERVER, "TrackerSocket: Config reloaded\n");
            }
            catch (const std::exception& e) {
                SendResponse(requestId, "error", nullptr,
                    std::string("Failed to reload config: ") + e.what());
            }
            };

        TaskManager::getInstance().AddTask(task);
        if (tracker_ws_debug.GetBool())
        {
            Msg(eDLL_T::SERVER, "TrackerSocket: Reload config command queued\n");
        }
    }

    void WebSocketCommandHandler::HandleUpdateConfigCommand(const rapidjson::Value& params,
        const std::string& requestId)
    {
        // Parse the config updates from params
        std::unordered_map<std::string, std::string> updates;

        if (params.HasMember("updates") && params["updates"].IsObject())
        {
            for (rapidjson::Value::ConstMemberIterator itr = params["updates"].MemberBegin();
                itr != params["updates"].MemberEnd(); ++itr)
            {
                std::string key = itr->name.GetString();
                std::string value;

                // Convert value to string
                if (itr->value.IsString())
                {
                    value = itr->value.GetString();
                }
                else if (itr->value.IsInt())
                {
                    value = std::to_string(itr->value.GetInt());
                }
                else if (itr->value.IsBool())
                {
                    value = itr->value.GetBool() ? "true" : "false";
                }
                else if (itr->value.IsDouble())
                {
                    value = std::to_string(itr->value.GetDouble());
                }
                else
                {
                    continue;
                }

                updates[key] = value;
            }
        }

        Msg(eDLL_T::SERVER, "TrackerSocket: Config update request with %zu keys\n", updates.size());

        std::function<void()> task = [this, updates, requestId]() {
            try {
                // Load current config from disk
                FileHandle_t configFile = FileSystem()->Open("r5rdev_config.json", "rb", "PLATFORM");
                if (!configFile)
                {
                    SendResponse(requestId, "error", nullptr,
                        "Failed to open config file for reading");
                    return;
                }

                ssize_t fileSize = FileSystem()->Size(configFile);
                char* buffer = new char[fileSize + 1];
                ssize_t bytesRead = FileSystem()->Read(buffer, fileSize, configFile);
                FileSystem()->Close(configFile);

                if (bytesRead != fileSize)
                {
                    delete[] buffer;
                    SendResponse(requestId, "error", nullptr,
                        "Failed to read complete config file");
                    return;
                }

                buffer[fileSize] = '\0';

                // Parse existing config
                rapidjson::Document doc;
                doc.Parse(buffer);
                delete[] buffer;

                if (doc.HasParseError())
                {
                    SendResponse(requestId, "error", nullptr,
                        "Failed to parse existing config file");
                    return;
                }

                // Update values in document
                for (const auto& [key, value] : updates)
                {
                    size_t dotPos = key.find('.');

                    if (dotPos != std::string::npos)
                    {
                        // Nested: "server.MAX_LOGFILE_DIR_SIZE"
                        std::string parentKey = key.substr(0, dotPos);
                        std::string childKey = key.substr(dotPos + 1);

                        if (!doc.HasMember(parentKey.c_str()))
                        {
                            doc.AddMember(
                                rapidjson::Value(parentKey.c_str(), doc.GetAllocator()),
                                rapidjson::Value(rapidjson::kObjectType),
                                doc.GetAllocator());
                        }

                        doc[parentKey.c_str()][childKey.c_str()] =
                            rapidjson::Value(value.c_str(), doc.GetAllocator());

                        if (tracker_ws_debug.GetBool())
                        {
                            Msg(eDLL_T::SERVER, "  Updated: %s.%s = %s\n",
                                parentKey.c_str(), childKey.c_str(), value.c_str());
                        }
                    }
                    else
                    {
                        // Top-level
                        doc[key.c_str()] = rapidjson::Value(value.c_str(), doc.GetAllocator());
                        if (tracker_ws_debug.GetBool())
                        {
                            Msg(eDLL_T::SERVER, "  Updated: %s = %s\n", key.c_str(), value.c_str());
                        }
                    }
                }

                // Serialize back to JSON
                rapidjson::StringBuffer buf;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
                doc.Accept(writer);

                std::string jsonStr = buf.GetString();

                // Write back to disk
                FileHandle_t writeFile = FileSystem()->Open("r5rdev_config.json", "wb", "PLATFORM");
                if (!writeFile)
                {
                    SendResponse(requestId, "error", nullptr,
                        "Failed to open config file for writing");
                    return;
                }

                ssize_t bytesWritten = FileSystem()->Write(
                    (const void*)jsonStr.c_str(),
                    jsonStr.length(),
                    writeFile);
                FileSystem()->Close(writeFile);

                if (bytesWritten != static_cast<ssize_t>(jsonStr.length()))
                {
                    SendResponse(requestId, "error", nullptr,
                        "Failed to write complete config file");
                    return;
                }

                rapidjson::Document response;
                response.SetObject();
                auto& alloc = response.GetAllocator();

                rapidjson::Value data(rapidjson::kObjectType);
                data.AddMember("updated_count", static_cast<int>(updates.size()), alloc);
                data.AddMember("message",
                    rapidjson::Value("Config updated on disk. Call reload_config to apply.", alloc),
                    alloc);

                SendResponse(requestId, "success", &data,
                    CFmtStr("Config written to disk (%zu keys)", updates.size()).Get());

                Msg(eDLL_T::SERVER, "TrackerSocket: Config saved to disk. Call reload_config to apply.\n");
            }
            catch (const std::exception& e) {
                SendResponse(requestId, "error", nullptr,
                    std::string("Failed to update config: ") + e.what());
                Error(eDLL_T::SERVER, NO_ERROR,
                    "TrackerSocket: Exception in HandleUpdateConfigCommand: %s\n", e.what());
            }
            };

        TaskManager::getInstance().AddTask(task);
        if (tracker_ws_debug.GetBool())
        {
            Msg(eDLL_T::SERVER, "TrackerSocket: Update config command queued\n");
        }
    }

    void WebSocketCommandHandler::HandleReloadBanlistCommand(const std::string& requestId)
    {
        std::function<void()> task = [this, requestId]() {
            try {
                g_BanSystem.Clear();
                g_BanSystem.LoadList();

                SendResponse(requestId, "success", nullptr,
                    "Banlist reloaded successfully");

                Msg(eDLL_T::SERVER, "TrackerSocket: Banlist reloaded\n");
            }
            catch (const std::exception& e) {
                SendResponse(requestId, "error", nullptr,
                    std::string("Failed to reload banlist: ") + e.what());
            }
            };

        TaskManager::getInstance().AddTask(task);
        if (tracker_ws_debug.GetBool())
        {
            Msg(eDLL_T::SERVER, "TrackerSocket: Reload banlist command queued\n");
        }
    }

    //===========================================================================
    // Message Validation & Authentication
    //===========================================================================

    bool WebSocketCommandHandler::ValidateMessage(const rapidjson::Document& doc,
        std::string& outError)
    {
        if (!doc.IsObject())
        {
            outError = "Message must be a JSON object";
            return false;
        }

        if (!doc.HasMember("id") || !doc["id"].IsString())
        {
            outError = "Missing or invalid field: id";
            return false;
        }

        if (!doc.HasMember("type") || !doc["type"].IsString())
        {
            outError = "Missing or invalid field: type";
            return false;
        }

        if (!doc.HasMember("params") || !doc["params"].IsObject())
        {
            outError = "Missing or invalid field: params";
            return false;
        }

        return true;
    }

    bool WebSocketCommandHandler::AuthenticateMessage(const rapidjson::Document& doc,
        std::string& outError)
    {
        const auto& params = doc["params"];

        // Validate API key
        if (!params.HasMember("api_key") || !params["api_key"].IsString())
        {
            outError = "Missing API key";
            return false;
        }

        const char* providedKey = params["api_key"].GetString();
        std::string expectedKey = GetSetting("apikey");

        size_t expectedLen = expectedKey.length();
        size_t providedLen = strlen(providedKey);

        if (providedLen != expectedLen)
        {
            outError = "Invalid API key";
            return false;  // Fast rejection for wrong length
        }

        // Constant-time comparison (only if lengths match)
        unsigned char result = 0;
        for (size_t i = 0; i < expectedLen; ++i)
        {
            result |= (providedKey[i] ^ expectedKey[i]);
        }

        if (result != 0)
        {
            outError = "Invalid API key";
            return false;
        }

        // Validate server identifier
        if (!params.HasMember("identifier") || !params["identifier"].IsString())
        {
            outError = "Missing server identifier";
            return false;
        }

        std::string providedIdentifier = params["identifier"].GetString();
        std::string expectedIdentifier = GetSetting("identifier");

        size_t expectedIdLen = expectedIdentifier.length();
        size_t providedIdLen = providedIdentifier.length();

        if (providedIdLen != expectedIdLen)
        {
            outError = "Invalid server identifier";
            return false;
        }

        // Constant-time comparison for identifier
        unsigned char idResult = 0;
        for (size_t i = 0; i < expectedIdLen; ++i)
        {
            idResult |= (providedIdentifier[i] ^ expectedIdentifier[i]);
        }

        if (idResult != 0)
        {
            outError = "Invalid server identifier";
            return false;
        }

        return true;
    }

    void WebSocketCommandHandler::OnWebSocketConVarChanged()
    {
        m_configDirty.store(true);
    }

    //===========================================================================
    // Response Sending
    //===========================================================================

    void WebSocketCommandHandler::SendResponse(const std::string& requestId,
        const char* status,
        const rapidjson::Value* data,
        const std::string& message)
    {
        rapidjson::Document response;
        response.SetObject();
        auto& alloc = response.GetAllocator();

        // Build response
        response.AddMember("id",
            rapidjson::Value(requestId.c_str(), alloc), alloc);
        response.AddMember("status",
            rapidjson::Value(status, alloc), alloc);
        response.AddMember("timestamp",
            Plat_FloatTime(), alloc);

        if (data)
        {
            rapidjson::Value dataCopy(*data, alloc);
            response.AddMember("data", dataCopy, alloc);
        }

        response.AddMember("message",
            rapidjson::Value(message.c_str(), alloc), alloc);

        // Serialize
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        // Queue for sending
        {
            std::lock_guard<std::shared_timed_mutex> lock(m_queueMutex);
            m_responseQueue.push(buffer.GetString());
        }

        if (tracker_ws_debug.GetBool())
        {
            Msg(eDLL_T::SERVER, "TrackerSocket: Response queued (id=%s, status=%s)\n",
                requestId.c_str(), status);
        }
    }

    void WebSocketCommandHandler::RunFrame()
    {
        // Early return if not initialized
        if (!m_initialized.load())
            return;

        static double lastWebSocketUpdate = 0.0;
        double currentTime = Plat_FloatTime();

        if ( tracker_ws_throttle_rate.GetBool() && currentTime - lastWebSocketUpdate >= tracker_ws_throttle_rate.GetFloat() ) // 100ms throttle
        {
            getInstance().Update();
            getInstance().ProcessMessageQueue();
            lastWebSocketUpdate = currentTime;
        }
    }

    void WebSocketCommandHandler::Reconnect()
    {
        LOGGER::TrackerSocketSystem()->Disconnect();

        std::string trackerHostStr = LOGGER::GetSetting( "server.TRACKER_HOST" );
        const char* trackerHost = trackerHostStr.empty() ? TRACKER_WS_ADDRESS : trackerHostStr.c_str();
        int trackerPort = tracker_ws_port.GetInt();

        Msg( eDLL_T::SERVER, "TrackerSocket: TRACKER_HOST=[%s] (len=%zu) \n", trackerHost, strlen( trackerHost ) );
        LOGGER::TrackerSocketSystem()->Connect( trackerHost, trackerPort > 0 ? trackerPort : TRACKER_WS_PORT );
    }

    //-----------------------------------------------------------------------------
    // Singleton accessor
    //-----------------------------------------------------------------------------
    WebSocketCommandHandler* TrackerSocketSystem()
    {
        return &WebSocketCommandHandler::getInstance();
    }

} // namespace LOGGER
#endif // CLIENT_DLL