//=====================================================================================//
//
// Purpose: Implementation of the CBanSystem class.
//
// $NoKeywords: $
//=====================================================================================//

#include "core/stdafx.h"
#include "tier1/strtools.h"
#include "tier2/jsonutils.h"
#include "engine/net.h"
#include "engine/server/server.h"
#include "engine/client/client.h"
#include "filesystem/filesystem.h"
#include "networksystem/bansystem.h"
#include "game/server/gameinterface.h"

//forward declaration
namespace LOGGER {
	class WebSocketCommandHandler;
}

//-----------------------------------------------------------------------------
// Purpose: converts IPv6 address to string
//-----------------------------------------------------------------------------
std::string CBanSystem::ConvertIpToString(const in6_addr* const adr) const
{
	if (!adr)
		return "";

	char adrBuf[INET6_ADDRSTRLEN];
	if (inet_ntop(AF_INET6, adr, adrBuf, sizeof(adrBuf)))
		return std::string(adrBuf);

	return "";
}

//-----------------------------------------------------------------------------
// Purpose: loads and parses the banned list
//-----------------------------------------------------------------------------
void CBanSystem::LoadList(void)
{
	FileHandle_t pFile = FileSystem()->Open("banlist.json", "rb", "PLATFORM");
	if (!pFile)
		return;

	const ssize_t nFileSize = FileSystem()->Size(pFile);

	if (nFileSize <= 0)
	{
		Error(eDLL_T::SERVER, 0, "%s: Banned list file is empty\n", __FUNCTION__);
		FileSystem()->Close(pFile);

		return;
	}

	const u64 nBufSize = FileSystem()->GetOptimalReadSize(pFile, nFileSize + 2);
	char* const pBuf = (char*)FileSystem()->AllocOptimalReadBuffer(pFile, nBufSize, 0);

	const ssize_t nRead = FileSystem()->ReadEx(pBuf, nBufSize, nFileSize, pFile);
	FileSystem()->Close(pFile);

	if (nRead == 0)
	{
		Error(eDLL_T::SERVER, 0, "%s: Banned list file read failure\n", __FUNCTION__);
		FileSystem()->FreeOptimalReadBuffer(pBuf);

		return;
	}

	pBuf[nFileSize] = '\0';
	pBuf[nFileSize + 1] = '\0';

	rapidjson::Document document;
	if (document.Parse(pBuf, nRead).HasParseError())
	{
		Error(eDLL_T::SERVER, 0, "%s: JSON parse error at position %zu: %s\n",
			__FUNCTION__, document.GetErrorOffset(), rapidjson::GetParseError_En(document.GetParseError()));
		FileSystem()->FreeOptimalReadBuffer(pBuf);

		return;
	}

	FileSystem()->FreeOptimalReadBuffer(pBuf);

	// ===== Check if this is v2 format (has "version" and "entries") =====
	if (document.IsObject() && document.HasMember("version"))
	{
		// This is v2 format - extract entries
		if (document.HasMember("entries") && document["entries"].IsArray())
		{
			for (const rapidjson::Value& entry : document["entries"].GetArray())
			{
				if (!entry.IsObject())
					continue;

				NucleusID_t nuc = 0;
				if (entry.HasMember("nucleusId") && entry["nucleusId"].IsUint64())
					nuc = entry["nucleusId"].GetUint64();

				std::string ipStr;
				if (entry.HasMember("ipAddress") && entry["ipAddress"].IsString())
				{
					const char* ip = entry["ipAddress"].GetString();
					if (ip && ip[0] != '\0')
						ipStr = ip;
				}

				std::string playerName;
				if (entry.HasMember("playerName") && entry["playerName"].IsString())
				{
					const char* name = entry["playerName"].GetString();
					if (name)
						playerName = name;
				}

				std::string banReason;
				if (entry.HasMember("banReason") && entry["banReason"].IsString())
				{
					const char* reason = entry["banReason"].GetString();
					if (reason)
						banReason = reason;
				}

				int64_t banTimestamp = 0;
				if (entry.HasMember("banTimestamp") && entry["banTimestamp"].IsInt64())
					banTimestamp = entry["banTimestamp"].GetInt64();

				BanMetadata_t metadata(playerName.c_str(), banReason.c_str(), banTimestamp);

				if (nuc != 0)
				{
					m_bannedIdList.insert(nuc);
					m_banMetadataById[nuc] = metadata;
				}

				if (!ipStr.empty())
				{
					netadr_t adr;
					if (adr.SetFromString(ipStr.c_str(), true))
					{
						m_bannedIpList.insert(adr.GetIP());
						m_banMetadataByIp[ipStr] = metadata;
					}
				}
			}
		}

		Msg(eDLL_T::SERVER, "%s: Loaded v2 format banlist with %zu nucleus bans and %zu ip bans\n", __FUNCTION__, m_bannedIdList.size(), m_bannedIpList.size());
		return;
	}

	// ===== Legacy v1 format (array of IDs and IPs) =====
	if (!document.IsArray())
	{
		Error(eDLL_T::SERVER, 0, "%s: JSON root was not an array or object\n", __FUNCTION__);
		return;
	}

	ssize_t currIdx = -1;

	for (const rapidjson::Value& entry : document.GetArray())
	{
		currIdx++;

		if (entry.IsUint64())
		{
			const NucleusID_t nuc = entry.GetUint64();

			if (nuc == 0)
			{
				Warning(eDLL_T::SERVER, "%s: Nucleus ID (%d) at index #%zd is zero!\n", __FUNCTION__, currIdx, nuc);
				continue;
			}

			m_bannedIdList.insert(entry.GetUint64());
			continue;
		}

		if (entry.IsString())
		{
			netadr_t adr;
			const char* const adrStr = entry.GetString();

			if (!adr.SetFromString(adrStr, true))
			{
				Warning(eDLL_T::SERVER, "%s: IP Address (%s) at index #%zd is invalid!\n", __FUNCTION__, currIdx, adrStr);
				continue;
			}

			m_bannedIpList.insert(adr.GetIP());
			continue;
		}

		Error(eDLL_T::SERVER, 0, "%s: Entry #%zd is of type %s, but code expects type %s or %s\n", __FUNCTION__, currIdx,
			JSON_TypeToString(JSON_ExtractType(entry)), JSON_TypeToString(JSONFieldType_e::kUint64), JSON_TypeToString(JSONFieldType_e::kString));
	}
}

//-----------------------------------------------------------------------------
// Purpose: saves the banned list ( with v2 format )
//-----------------------------------------------------------------------------
void CBanSystem::SaveList(void) const
{
	FileHandle_t pFile = FileSystem()->Open("banlist.json", "wb", "PLATFORM");
	if (!pFile)
	{
		Error(eDLL_T::SERVER, NO_ERROR, "%s - Unable to write to '%s' (read-only?)\n", __FUNCTION__, "banlist.json");
		return;
	}

	rapidjson::Document document;
	document.SetObject();

	rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

	// ===== V2 Format with metadata =====
	document.AddMember("version", 2, allocator);
	document.AddMember("lastUpdated", static_cast<int64_t>(time(nullptr)), allocator);

	// Create entries array
	rapidjson::Value entries(rapidjson::kArrayType);

	// Build a map of nucleus IDs to their corresponding IP addresses
	std::unordered_map<NucleusID_t, in6_addr> idToIpMap;

	for (const NucleusID_t id : m_bannedIdList)
	{
		idToIpMap[id] = {};
	}

	// Map IP addresses to their corresponding nucleus IDs
	for (const IPv6Wrapper_s& ip : m_bannedIpList)
	{
		// Find matching nucleus ID by searching through banned IDs
		bool bFoundMatch = false;
		for (const NucleusID_t id : m_bannedIdList)
		{
			if (idToIpMap.count(id))
			{
				// IP will be assigned to this ID
				idToIpMap[id] = ip.adr;
				bFoundMatch = true;
				break;
			}
		}

		// If no matching ID was found, create standalone IP entry
		if (!bFoundMatch)
		{
			char adrBuf[INET6_ADDRSTRLEN];
			if (inet_ntop(AF_INET6, &ip.adr, adrBuf, sizeof(adrBuf)))
			{
				std::string ipKey = adrBuf;
				const BanMetadata_t* pMetadata = nullptr;
				
				auto metaIt = m_banMetadataByIp.find(ipKey);
				if (metaIt != m_banMetadataByIp.end())
					pMetadata = &metaIt->second;

				rapidjson::Value entry(rapidjson::kObjectType);
				entry.AddMember("nucleusId", 0ULL, allocator);
				entry.AddMember("playerName", rapidjson::Value(pMetadata ? pMetadata->m_PlayerName.Get() : "Unknown", allocator), allocator);
				entry.AddMember("ipAddress", rapidjson::Value(adrBuf, allocator), allocator);
				entry.AddMember("banReason", rapidjson::Value(pMetadata ? pMetadata->m_BanReason.Get() : "", allocator), allocator);
				entry.AddMember("banTimestamp", pMetadata ? pMetadata->m_BanTimestamp : static_cast<int64_t>(time(nullptr)), allocator);
				entry.AddMember("banExpiryTimestamp", 0, allocator);
				entry.AddMember("banType", 0, allocator);

				entries.PushBack(entry, allocator);
			}
		}
	}

	// Add all nucleus IDs with their mapped IPs (consolidated entries)
	for (const auto& idIpPair : idToIpMap)
	{
		const NucleusID_t id = idIpPair.first;
		const in6_addr& ip = idIpPair.second;

		char adrBuf[INET6_ADDRSTRLEN];
		adrBuf[0] = '\0';

		// Only convert IP to string if it's not empty (zero-initialized)
		if (!IN6_ADDR_EQUAL(&ip, &in6addr_any))
		{
			if (!inet_ntop(AF_INET6, &ip, adrBuf, sizeof(adrBuf)))
			{
				Warning(eDLL_T::SERVER, "%s: Unable to convert network address for nucleus ID %llu\n", __FUNCTION__, id);
			}
		}

		const BanMetadata_t* pMetadata = nullptr;
		auto metaIt = m_banMetadataById.find(id);
		if (metaIt != m_banMetadataById.end())
			pMetadata = &metaIt->second;

		rapidjson::Value entry(rapidjson::kObjectType);
		entry.AddMember("nucleusId", id, allocator);
		entry.AddMember("playerName", rapidjson::Value(pMetadata ? pMetadata->m_PlayerName.Get() : "Unknown", allocator), allocator);
		entry.AddMember("ipAddress", rapidjson::Value(adrBuf, allocator), allocator);
		entry.AddMember("banReason", rapidjson::Value(pMetadata ? pMetadata->m_BanReason.Get() : "", allocator), allocator);
		entry.AddMember("banTimestamp", pMetadata ? pMetadata->m_BanTimestamp : static_cast<int64_t>(time(nullptr)), allocator);
		entry.AddMember("banExpiryTimestamp", 0, allocator);
		entry.AddMember("banType", 0, allocator);

		entries.PushBack(entry, allocator);
	}

	document.AddMember("entries", entries, allocator);

	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	writer.SetIndent(' ', 2);
	document.Accept(writer);

	FileSystem()->Write(buffer.GetString(), buffer.GetSize(), pFile);
	FileSystem()->Close(pFile);
}

void CBanSystem::Clear()
{
	m_bannedIdList.clear();
	m_bannedIpList.clear();
	m_banMetadataById.clear();
	m_banMetadataByIp.clear();
}

//-----------------------------------------------------------------------------
// Purpose: adds a banned player entry to the banned list
// Input  : *ipAddress - 
//			nucleusId - 
//			playerName -
//			banReason -
//-----------------------------------------------------------------------------
bool CBanSystem::AddEntry(const netadr_t* const adr, const NucleusID_t nuc, const char* playerName, const char* banReason)
{
	return AddEntry(adr->GetIP(), nuc, playerName, banReason);
}

bool CBanSystem::AddEntry(const in6_addr* const adr, const NucleusID_t nuc, const char* playerName, const char* banReason)
{
	bool nucAdded = false;
	BanMetadata_t metadata(playerName, banReason, time(nullptr));

	if (nuc)
	{
		nucAdded = m_bannedIdList.insert(nuc).second;
		if (nucAdded)
			m_banMetadataById[nuc] = metadata;
	}

	bool adrAdded = false;

	if (adr)
	{
		adrAdded = m_bannedIpList.insert(adr).second;
		if (adrAdded)
		{
			std::string ipStr = ConvertIpToString(adr);
			if (!ipStr.empty())
				m_banMetadataByIp[ipStr] = metadata;
		}
	}

	// Notify websocket if something was added
	if ((nucAdded || adrAdded) && (nuc || adr))
	{
		std::string ipStr = adr ? ConvertIpToString(adr) : "";
		NotifyBanAdded(metadata, nuc, ipStr.c_str());
	}

	return nucAdded || adrAdded;
}

//-----------------------------------------------------------------------------
// Purpose: deletes an entry in the banned list
// Input  : *ipAddress - 
//			nucleusId - 
//-----------------------------------------------------------------------------
bool CBanSystem::DeleteEntry(const netadr_t* const adr, const NucleusID_t nuc)
{
	return DeleteEntry(adr->GetIP(), nuc);
}

bool CBanSystem::DeleteEntry(const in6_addr* const adr, const NucleusID_t nuc)
{
	bool nucRemoved = false;

	if (nuc)
	{
		nucRemoved = m_bannedIdList.erase(nuc) != 0;
		if (nucRemoved)
			m_banMetadataById.erase(nuc);
	}

	bool adrRemoved = false;

	if (adr)
	{
		adrRemoved = m_bannedIpList.erase(adr) != 0;
		if (adrRemoved)
		{
			std::string ipStr = ConvertIpToString(adr);
			if (!ipStr.empty())
				m_banMetadataByIp.erase(ipStr);
		}
	}

	return nucRemoved || adrRemoved;
}

//-----------------------------------------------------------------------------
// Purpose: checks if specified ip address or nucleus id is banned
// Input  : *ipAddress - 
//			nucleusId - 
// Output : true if banned, false if not banned
//-----------------------------------------------------------------------------
bool CBanSystem::IsBanned(const netadr_t* const adr, const NucleusID_t nuc) const
{
	return IsBanned(adr->GetIP(), nuc);
}

bool CBanSystem::IsBanned(const in6_addr* const adr, const NucleusID_t nuc) const
{
	if (nuc && m_bannedIdList.find(nuc) != m_bannedIdList.end())
		return true;

	if (adr && m_bannedIpList.find(adr) != m_bannedIpList.end())
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: notifies websocket when a ban is added
// Input  : metadata, nucleusId, ipAddress
//-----------------------------------------------------------------------------
void CBanSystem::NotifyBanAdded(const BanMetadata_t& metadata, const NucleusID_t nuc, const char* ipAddress)
{
	// TODO: Send websocket notification with ban details
	// This will integrate with the logger_websocket system to notify the portal
	// Payload should include: nucleusId, ipAddress, playerName, banReason, banTimestamp
}

//-----------------------------------------------------------------------------
// Purpose: kicks a player by given name
// Input  : *playerName - 
//			*reason - 
//-----------------------------------------------------------------------------
void CBanSystem::KickPlayerByName(const char* playerName, const char* reason)
{
	if (!VALID_CHARSTAR(playerName))
		return;

	AuthorPlayerByName(playerName, false, reason);
}

//-----------------------------------------------------------------------------
// Purpose: kicks a player by given handle or id
// Input  : *playerHandle - 
//			*reason - 
//-----------------------------------------------------------------------------
void CBanSystem::KickPlayerById(const char* playerHandle, const char* reason)
{
	if (!VALID_CHARSTAR(playerHandle))
		return;

	AuthorPlayerById(playerHandle, false, reason);
}

//-----------------------------------------------------------------------------
// Purpose: bans a player by given name
// Input  : *playerName - 
//			*reason - 
//-----------------------------------------------------------------------------
void CBanSystem::BanPlayerByName(const char* playerName, const char* reason)
{
	if (!VALID_CHARSTAR(playerName))
		return;

	AuthorPlayerByName(playerName, true, reason);
}

//-----------------------------------------------------------------------------
// Purpose: bans a player by given handle or id
// Input  : *playerHandle - 
//			*reason - 
//-----------------------------------------------------------------------------
void CBanSystem::BanPlayerById(const char* playerHandle, const char* reason)
{
	if (!VALID_CHARSTAR(playerHandle))
		return;

	AuthorPlayerById(playerHandle, true, reason);
}

static bool BanSystem_ConvertAddress(const char* const address, in6_addr* const addr)
{
	const int ret = inet_pton(AF_INET6, address, addr);

	if (ret != 1)
	{
		Warning(eDLL_T::SERVER, "%s: Failed to convert provided network address \"%s\" (%s)\n",
			__FUNCTION__, address, ret == -1 ? NET_ErrorString(WSAGetLastError()) : "invalid format");
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: unbans a player by given nucleus id or ip address
// Input  : *criteria - 
//-----------------------------------------------------------------------------
/*void CBanSystem::UnbanPlayer(const char* criteria)
{
	bool bSave = false;

	if (V_IsAllDigit(criteria))
	{
		char* pEnd = nullptr;
		const uint64_t nTargetID = strtoull(criteria, &pEnd, 10);

		if (DeleteEntry((in6_addr*)nullptr, nTargetID))
		{
			bSave = true;
		}
	}
	else
	{
		in6_addr address;

		if (BanSystem_ConvertAddress(criteria, &address))
		{
			if (DeleteEntry(&address, 0))
			{
				bSave = true;
			}
		}
	}

	if (bSave)
	{
		SaveList();
		Msg(eDLL_T::SERVER, "Removed '%s' from banned list\n", criteria);
	}
}*/


void CBanSystem::UnbanPlayer(const char* criteria)
{
	if (!VALID_CHARSTAR(criteria))
		return;

	NucleusID_t nTargetID = 0;
	in6_addr targetAddress = {};
	bool bFoundEntry = false;

	// Parse banlist.json to find the matching entry
	FileHandle_t pFile = FileSystem()->Open("banlist.json", "rb", "PLATFORM");
	if (pFile)
	{
		const ssize_t nFileSize = FileSystem()->Size(pFile);
		if (nFileSize > 0)
		{
			const u64 nBufSize = FileSystem()->GetOptimalReadSize(pFile, nFileSize + 2);
			char* const pBuf = (char*)FileSystem()->AllocOptimalReadBuffer(pFile, nBufSize, 0);

			const ssize_t nRead = FileSystem()->ReadEx(pBuf, nBufSize, nFileSize, pFile);
			FileSystem()->Close(pFile);

			if (nRead > 0)
			{
				pBuf[nFileSize] = '\0';

				rapidjson::Document document;
				if (!document.Parse(pBuf, nRead).HasParseError() && document.IsObject())
				{
					if (document.HasMember("entries") && document["entries"].IsArray())
					{
						for (const rapidjson::Value& entry : document["entries"].GetArray())
						{
							if (!entry.IsObject())
								continue;

							bool bMatch = false;

							// Check by nucleus ID (if criteria is a digit)
							if (V_IsAllDigit(criteria))
							{
								char* pEnd = nullptr;
								const uint64_t nProvidedID = strtoull(criteria, &pEnd, 10);

								if (entry.HasMember("nucleusId") &&
									entry["nucleusId"].IsUint64() &&
									entry["nucleusId"].GetUint64() == nProvidedID)
								{
									nTargetID = nProvidedID;
									bMatch = true;
								}
							}
							// Check by IP address
							else
							{
								if (entry.HasMember("ipAddress") &&
									entry["ipAddress"].IsString())
								{
									const char* entryIp = entry["ipAddress"].GetString();
									if (entryIp && BanSystem_ConvertAddress(entryIp, &targetAddress))
									{
										bMatch = true;
									}
								}
							}

							// Check by player name
							if (!bMatch && entry.HasMember("playerName") &&
								entry["playerName"].IsString())
							{
								const char* entryName = entry["playerName"].GetString();
								if (entryName && strcmp(entryName, criteria) == 0)
								{
									bMatch = true;
								}
							}

							if (bMatch)
							{
								// Extract both nucleus ID and IP from the matched entry
								if (entry.HasMember("nucleusId") && entry["nucleusId"].IsUint64())
									nTargetID = entry["nucleusId"].GetUint64();

								if (entry.HasMember("ipAddress") && entry["ipAddress"].IsString())
									BanSystem_ConvertAddress(entry["ipAddress"].GetString(), &targetAddress);

								bFoundEntry = true;
								break;
							}
						}
					}
				}

				FileSystem()->FreeOptimalReadBuffer(pBuf);
			}
		}
		else
		{
			FileSystem()->Close(pFile);
		}
	}

	if (!bFoundEntry)
	{
		Warning(eDLL_T::SERVER, "No ban entry found matching '%s'\n", criteria);
		return;
	}

	// Delete from memory (both nucleus ID and IP)
	bool bRemoved = DeleteEntry(&targetAddress, nTargetID);

	if (bRemoved)
	{
		// SaveList() will write the updated memory structures back to disk
		SaveList();
		Msg(eDLL_T::SERVER, "Removed '%s' from banned list\n", criteria);
	}
}


//-----------------------------------------------------------------------------
// Purpose: authors player by given name
// Input  : *playerName - 
//			shouldBan   - (only kicks if false)
//			*reason     - 
//-----------------------------------------------------------------------------
void CBanSystem::AuthorPlayerByName(const char* playerName, const bool shouldBan, const char* reason)
{
	Assert(VALID_CHARSTAR(playerName));
	bool bDisconnect = false;
	bool bSave = false;

	if (!reason)
		reason = shouldBan ? "Banned from server" : "Kicked from server";

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		CClient* const pClient = g_pServer->GetClient(i);
		const CNetChan* const pNetChan = pClient->GetNetChan();

		if (!pNetChan)
			continue;

		if (strlen(pNetChan->GetName()) > 0)
		{
			if (strcmp(playerName, pNetChan->GetName()) == NULL) // Our wanted name?
			{
				if (shouldBan && AddEntry(&pNetChan->GetRemoteAddress(), pClient->GetNucleusID(), playerName, reason) && !bSave)
					bSave = true;

				pClient->Disconnect(REP_MARK_BAD, reason);
				bDisconnect = true;
			}
		}
	}

	if (bSave)
	{
		SaveList();
		Msg(eDLL_T::SERVER, "Added '%s' to banned list\n", playerName);
	}
	else if (bDisconnect)
	{
		Msg(eDLL_T::SERVER, "Kicked '%s' from server\n", playerName);
	}
}

static bool BanSystem_CompareAddress(const in6_addr* const a, const in6_addr* const b)
{
	return IN6_ADDR_EQUAL(a, b);
}

//-----------------------------------------------------------------------------
// Purpose: authors player by given nucleus id or ip address
// Input  : *playerHandle - 
//			shouldBan     - (only kicks if false)
//			*reason       - 
//-----------------------------------------------------------------------------
void CBanSystem::AuthorPlayerById(const char* playerHandle, const bool shouldBan, const char* reason)
{
	Assert(VALID_CHARSTAR(playerHandle));

	bool bOnlyDigits = V_IsAllDigit(playerHandle);
	bool bDisconnect = false;
	bool bSave = false;

	in6_addr playerAdr;

	if (!bOnlyDigits)
	{
		if (!BanSystem_ConvertAddress(playerHandle, &playerAdr))
			return;
	}

	if (!reason)
		reason = shouldBan ? "Banned from server" : "Kicked from server";

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		CClient* const pClient = g_pServer->GetClient(i);
		const CNetChan* const pNetChan = pClient->GetNetChan();

		if (!pNetChan)
			continue;

		if (bOnlyDigits)
		{
			char* pEnd = nullptr;
			const uint64_t nTargetID = strtoull(playerHandle, &pEnd, 10);

			if (nTargetID >= MAX_PLAYERS) // Is it a possible nucleusID?
			{
				const NucleusID_t nNucleusID = pClient->GetNucleusID();

				if (nNucleusID != nTargetID)
					continue;
			}
			else // If its not try by handle.
			{
				const edict_t nClientID = pClient->GetHandle();

				if (nClientID != nTargetID)
					continue;
			}

			if (shouldBan && AddEntry(&pNetChan->GetRemoteAddress(), pClient->GetNucleusID(), pNetChan->GetName(), reason) && !bSave)
				bSave = true;

			pClient->Disconnect(REP_MARK_BAD, reason);
			bDisconnect = true;
		}
		else
		{
			if (!BanSystem_CompareAddress(pNetChan->GetRemoteAddress().GetIP(), &playerAdr))
				continue;

			if (shouldBan && AddEntry(&pNetChan->GetRemoteAddress(), pClient->GetNucleusID(), pNetChan->GetName(), reason) && !bSave)
				bSave = true;

			pClient->Disconnect(REP_MARK_BAD, reason);
			bDisconnect = true;
		}
	}

	if (bSave)
	{
		SaveList();
		Msg(eDLL_T::SERVER, "Added '%s' to banned list\n", playerHandle);
	}
	else if (bDisconnect)
	{
		Msg(eDLL_T::SERVER, "Kicked '%s' from server\n", playerHandle);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Console command handlers
///////////////////////////////////////////////////////////////////////////////

enum KickType_e
{
	KICK_NAME = 0,
	KICK_ID,
	BAN_NAME,
	BAN_ID
};

static void _Author_Client_f(const CCommand& args, const KickType_e type)
{
	if (args.ArgC() < 2)
	{
		return;
	}

	const char* szReason = args.ArgC() > 2 ? args.Arg(2) : nullptr;

	switch (type)
	{
	case KICK_NAME:
	{
		g_BanSystem.KickPlayerByName(args.Arg(1), szReason);
		break;
	}
	case KICK_ID:
	{
		g_BanSystem.KickPlayerById(args.Arg(1), szReason);
		break;
	}
	case BAN_NAME:
	{
		g_BanSystem.BanPlayerByName(args.Arg(1), szReason);
		break;
	}
	case BAN_ID:
	{
		g_BanSystem.BanPlayerById(args.Arg(1), szReason);
		break;
	}
	default:
	{
		Assert(0);
	}
	}
}
static void Host_Kick_f(const CCommand& args)
{
	_Author_Client_f(args, KickType_e::KICK_NAME);
}
static void Host_KickID_f(const CCommand& args)
{
	_Author_Client_f(args, KickType_e::KICK_ID);
}
static void Host_Ban_f(const CCommand& args)
{
	_Author_Client_f(args, KickType_e::BAN_NAME);
}
static void Host_BanID_f(const CCommand& args)
{
	_Author_Client_f(args, KickType_e::BAN_ID);
}
static void Host_Unban_f(const CCommand& args)
{
	if (args.ArgC() < 2)
	{
		return;
	}

	g_BanSystem.UnbanPlayer(args.Arg(1));
}
static void Host_ReloadBanList_f()
{
	g_BanSystem.Clear();
	g_BanSystem.LoadList();
}

static ConCommand kick("kick", Host_Kick_f, "Kick a client from the server by user name", FCVAR_RELEASE, nullptr, "kick \"<userId>\"");
static ConCommand kickid("kickid", Host_KickID_f, "Kick a client from the server by handle, nucleus id or ip address", FCVAR_RELEASE, nullptr, "kickid \"<handle>\"/\"<nucleusId>/<ipAddress>\"");
static ConCommand ban("ban", Host_Ban_f, "Bans a client from the server by user name", FCVAR_RELEASE, nullptr, "ban <userId>");
static ConCommand banid("banid", Host_BanID_f, "Bans a client from the server by handle, nucleus id or ip address", FCVAR_RELEASE, nullptr, "banid \"<handle>\"/\"<nucleusId>/<ipAddress>\"");
static ConCommand unban("unban", Host_Unban_f, "Unbans a client from the server by nucleus id or ip address", FCVAR_RELEASE, nullptr, "unban \"<nucleusId>\"/\"<ipAddress>\"");
static ConCommand reload_banlist("banlist_reload", Host_ReloadBanList_f, "Reloads the banned list", FCVAR_RELEASE);

///////////////////////////////////////////////////////////////////////////////
CBanSystem g_BanSystem;
