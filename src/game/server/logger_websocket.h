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
        static WebSocketCommandHandler& getInstance();

        // ===== Connection Management =====
        bool Connect(const char* address, int port);
        void Disconnect();
        bool IsConnected() const;
        void RunFrame();
        void Update();  // Call from main update loop
        void Shutdown();

        // ===== Message Reception =====
        void OnMessageReceived(const std::string& rawMessage);
        void ProcessMessageQueue();

        // ===== Message Transmission =====
        void SendResponse(const std::string& requestId, const char* status,
            const rapidjson::Value* data, const std::string& message);

        // ===== Command Handlers =====
        void HandleKickCommand(const rapidjson::Value& params,
            const std::string& requestId);
        void HandleBanCommand(const rapidjson::Value& params,
            const std::string& requestId);
        void HandleUnbanCommand(const rapidjson::Value& params,
            const std::string& requestId);
        void HandleGetBanlistCommand(const std::string& requestId);
        void HandleGetPlayersCommand(const std::string& requestId);
        void HandleGetConfigCommand(const rapidjson::Value& params,
            const std::string& requestId);
        void HandleGetStatsCommand(const std::string& requestId);
        void HandleReloadConfigCommand(const std::string& requestId);
        void HandleUpdateConfigCommand(const rapidjson::Value& params,
            const std::string& requestId);
        void HandleReloadBanlistCommand(const std::string& requestId);
        

        // ===== Validation & Utilities =====
        bool ValidateMessage(const rapidjson::Document& doc,
            std::string& outError);
        bool AuthenticateMessage(const rapidjson::Document& doc,
            std::string& outError);

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
        void DispatchCommand(const rapidjson::Document& doc,
            const std::string& requestId);

        //not implemented yet?
        /*std::string BuildErrorResponse(const std::string& requestId,
                                       const char* errorCode,
                                       const char* message);*/

                                       // ===== Data Members =====
        std::unique_ptr<CWebSocket> m_webSocket;
        std::queue<PendingMessage_t> m_messageQueue;
        std::queue<std::string> m_responseQueue;  // Responses to send
        std::shared_timed_mutex m_queueMutex;

        std::string m_serverAddress;
        int m_serverPort;
        std::atomic<bool> m_isConnected;
        std::atomic<bool> m_initialized;
        std::atomic<uint64_t> m_messageCount{ 0 };

        double m_lastUpdateTime;
        double m_lastConnectAttempt;

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
