#pragma once
#ifndef CLIENT_DLL
#ifndef LOGGER_WEBSOCKET_H
#define LOGGER_WEBSOCKET_H

#include <string>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include <rapidjson/document.h>
#include <filesystem>
#include "filesystem/ifilesystem.h"

//forward declarations
class CWebSocket;

namespace LOGGER
{
    //===========================================================================
    // WebSocketCommandHandler - Handles WebSocket server control commands
    //===========================================================================

    class WebSocketCommandHandler
    {
    public:
        // Singleton access
        static WebSocketCommandHandler& getInstance(); //use TrackerSocketSystem()

        // ===== Connection Management =====
        bool Connect(const char* trackerHostname, int port);
        void Reconnect();
        void Disconnect();
        bool IsConnected() const;
        void RunFrame();
        void Update();  // Call from main update loop
        void Status();
        void Shutdown();

        // ===== Message Reception =====
        void OnMessageReceived(const std::string& rawMessage);
        void ProcessMessageQueue();

        // ===== Message Transmission =====
        void SendResponse(const std::string& requestId, const char* status, const rapidjson::Value* data, const std::string& message);

        // ===== Command Handlers =====
        void HandleKickCommand(const rapidjson::Value& params, const std::string& requestId);
        void HandleBanCommand(const rapidjson::Value& params, const std::string& requestId);
        void HandleUnbanCommand(const rapidjson::Value& params, const std::string& requestId);
        void HandleGetBanlistCommand(const std::string& requestId);
        void HandleGetPlayersCommand(const std::string& requestId);
        void HandleGetConfigCommand(const rapidjson::Value& params, const std::string& requestId);
        void HandleGetStatsCommand(const std::string& requestId);
        void HandleReloadConfigCommand(const std::string& requestId);
        void HandleUpdateConfigCommand(const rapidjson::Value& params, const std::string& requestId);
        void HandleReloadBanlistCommand(const std::string& requestId);


        // ===== Validation & Utilities =====
        bool ValidateMessage(const rapidjson::Document& doc, std::string& outError);
        bool AuthenticateMessage(const rapidjson::Document& doc, std::string& outError);

        // ===== Configuration =====
        void OnWebSocketConVarChanged();

    private:
        // Private constructor (singleton)
        WebSocketCommandHandler();
        ~WebSocketCommandHandler();

        // Delete copy/move constructors
        WebSocketCommandHandler(const WebSocketCommandHandler&) = delete;
        WebSocketCommandHandler& operator=(const WebSocketCommandHandler&) = delete;
        WebSocketCommandHandler(WebSocketCommandHandler&&) = delete;
        WebSocketCommandHandler& operator=(WebSocketCommandHandler&&) = delete;

        // Memory Allocation for performance
        void AllocateAddress(const char* connectionAddress);
        void FreeAddress();

        // ===== Message Queue Structures =====
        struct PendingMessage_t
        {
            std::string id;
            std::string type;
            std::string rawJson;  // Store raw JSON for later parsing
            double receivedTime;
            int retryCount;
        };

        // Command type enumeration for switch dispatch
        enum class CommandType_e
        {
            UNKNOWN = -1,
            KICK_PLAYER = 0,
            BAN_PLAYER = 1,
            UNBAN_PLAYER = 2,
            GET_BANLIST = 3,
            GET_PLAYERS = 4,
            GET_CONFIG = 5,
            GET_STATS = 6,
            RELOAD_CONFIG = 7,
            UPDATE_CONFIG = 8,
            RELOAD_BANLIST = 9
        };

        // ===== Internal Helpers =====
        CommandType_e GetCommandType(const std::string& typeStr);
        void DispatchCommand(const rapidjson::Document& doc, const std::string& requestId);

        //not implemented yet
        /*std::string BuildErrorResponse(const std::string& requestId,const char* errorCode, const char* message);*/

        // ===== Data Members =====
        std::unique_ptr<CWebSocket> m_webSocket;
        std::queue<PendingMessage_t> m_messageQueue;
        std::queue<std::string> m_responseQueue;
        std::shared_timed_mutex m_queueMutex;

        std::string m_serverHostname;
        const char* m_connectedAddress = nullptr; //manually allocated for performance.
        int m_serverPort;
        std::atomic<bool> m_isConnected;
        std::atomic<bool> m_initialized;
        std::atomic<uint64_t> m_messageCount{ 0 };

        double m_lastUpdateTime;
        double m_lastConnectAttempt;
        float m_throttleRate;

        // Configuration caching
        std::string m_cachedApiKey;
        std::atomic<bool> m_configDirty;
        std::vector<char> m_receiveBuffer;
        static constexpr int32_t RECEIVE_BUFFER_SIZE = 262144;
    };

    WebSocketCommandHandler* TrackerSocketSystem();
}

#endif // LOGGER_WEBSOCKET_H
#endif // CLIENT_DLL

static ConVar tracker_ws_enable("tracker_ws_enable", "1", FCVAR_RELEASE, "Enable WebSocket remote command interface (0 = disabled, 1 = enabled)");
static ConVar tracker_ws_port("tracker_ws_port", "9705", FCVAR_RELEASE, "WebSocket server port");
static ConVar tracker_ws_debug("tracker_ws_debug", "0", FCVAR_RELEASE, "Enable WebSocket debug logging (0 = disabled, 1 = enabled)");
static ConVar tracker_ws_use_ssl("tracker_ws_use_ssl", "1", FCVAR_RELEASE, "Use SSL for WebSocket connection (0 = disabled, 1 = enabled)");
static ConVar tracker_ws_lax_ssl("tracker_ws_lax_ssl", "0", FCVAR_RELEASE, "Lax SSL certificate validation (0 = strict, 1 = lax)");
static ConVar tracker_ws_buffer_size("tracker_ws_buffer_size", "262144", FCVAR_RELEASE, "WebSocket buffer size in bytes");
static ConVar tracker_ws_max_retries("tracker_ws_max_retries", "3", FCVAR_RELEASE, "Maximum number of WebSocket connection retries");
static ConVar tracker_ws_retry_time("tracker_ws_retry_time", "5.0", FCVAR_RELEASE, "Time in seconds between WebSocket connection retries. float");
static ConVar tracker_ws_time_out("tracker_ws_time_out", "125", FCVAR_RELEASE, "WebSocket connection timeout in seconds");
static ConVar tracker_ws_keep_alive("tracker_ws_keep_alive", "60", FCVAR_RELEASE, "WebSocket keep-alive interval in seconds");
static ConVar tracker_ws_throttle_rate("tracker_ws_throttle_rate", "0.10", FCVAR_RELEASE, "WebSocket message processing throttle rate in seconds. Default 100ms (0 = no throttling)");
static ConVar tracker_ws_hostname("tracker_ws_hostname", "r5r.dev", FCVAR_RELEASE, "WebSocket server hostname");
static ConVar tracker_ws_tls_version("tracker_ws_tls_version", "-1", FCVAR_RELEASE, "Forces SSL to Transport Layer Security version  ( -1: [default] | 0: [1.0] | 1: [1.1] | 2: [1.2] | 3: [1.3] )");

static ConCommand tracker_ws_restart("tracker_ws_restart", []() { LOGGER::TrackerSocketSystem()->Reconnect(); }, "Restart the WebSocket connection to the remote server.", FCVAR_RELEASE);
static ConCommand tracker_ws_shutdown("tracker_ws_shutdown", []() { LOGGER::TrackerSocketSystem()->Shutdown(); }, "Shutdown the WebSocket connection to the remote server.", FCVAR_RELEASE);
static ConCommand tracker_ws_status("tracker_ws_status", []() { LOGGER::TrackerSocketSystem()->Status(); }, "Display the current status of the WebSocket connection to the remote server.", FCVAR_RELEASE);