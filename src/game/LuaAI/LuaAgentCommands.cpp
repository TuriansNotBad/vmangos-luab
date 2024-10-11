
#include "LuaAgent.h"
#include "LuaAgentMgr.h"
#include "Language.h"
#include "Chat.h"

bool ChatHandler::HandleLuabAddCommand(char* args)
{
	// part from PartyBotMgr.cpp ChatHandler::HandlePartyBotLoadCommand
	Player* pPlayer = GetSession()->GetPlayer();
	if (!pPlayer)
		return false;

	std::string name = ExtractPlayerNameFromLink(&args);
	if (name.empty())
	{
		SendSysMessage(LANG_PLAYER_NOT_FOUND);
		SetSentErrorMessage(true);
		return false;
	}

	int logicID = -1;
	if (!ExtractInt32(&args, logicID))
	{
		SendSysMessage("Incorrect syntax. Expected logic id. Usage: .luab add name logic spec");
		SetSentErrorMessage(true);
		return false;
	}

	std::string spec = "";
	if (char* specCstr = ExtractArg(&args))
		spec = std::string(specCstr);

	LuaAgentMgr::CheckResult result = sLuaAgentMgr.AddAgent(name, pPlayer->GetObjectGuid(), logicID, spec);
	switch (result)
	{
	case LuaAgentMgr::CHAR_DOESNT_EXIST:
		PSendSysMessage("Character %s doesn't exist", name.c_str());
		return true;
	case LuaAgentMgr::CHAR_ACCOUNT_DOESNT_EXIST:
		PSendSysMessage("Character %s has no account", name.c_str());
		return true;
	case LuaAgentMgr::CHAR_ALREADY_LOGGED_IN:
		PSendSysMessage("Character %s already logged in", name.c_str());
		return true;
	case LuaAgentMgr::CHAR_ALREADY_IN_QUEUE:
		PSendSysMessage("Character %s already queued as a bot", name.c_str());
		return true;
	case LuaAgentMgr::CHAR_ALREADY_EXISTS:
		PSendSysMessage("Character %s already exists as a bot", name.c_str());
		return true;
	case LuaAgentMgr::CHAR_OK:
		PSendSysMessage("Character %s added as a bot", name.c_str());
		return true;
	}
	return true;
}


bool ChatHandler::HandleLuabAddPartyCommand(char* args)
{
	Player* owner = GetSession()->GetPlayer();
	if (!owner)
		return false;

	std::string name;
	if (char* nameCstr = ExtractArg(&args))
		name = std::string(nameCstr);
	else
	{
		SendSysMessage("Incorrect syntax. Expected name. Usage: .luab addparty name");
		SetSentErrorMessage(true);
		return false;
	}

	sLuaAgentMgr.AddParty(name, owner->GetObjectGuid());

	return true;
}


bool ChatHandler::HandleLuabRemovePartyCommand(char* args)
{
	Player* owner = GetSession()->GetPlayer();
	if (!owner)
		return false;

	std::string name;
	if (char* nameCstr = ExtractArg(&args))
		name = std::string(nameCstr);
	else
	{
		SendSysMessage("Incorrect syntax. Expected name. Usage: .luab removeparty name");
		SetSentErrorMessage(true);
		return false;
	}

	sLuaAgentMgr.RemoveParty(name, owner->GetObjectGuid());

	return true;
}


bool ChatHandler::HandleLuabRemoveCommand(char* args)
{
	if (Player* selection = GetSelectedPlayer())
		if (LuaAgent* agent = selection->GetLuaAI())
			if (!agent->GetPartyIntelligence())
				sLuaAgentMgr.LogoutAgent(selection->GetObjectGuid());
			else
			{
				SendSysMessage("Unable to remove this bot. Use .luab remparty");
				SetSentErrorMessage(true);
				return false;
			}
	return true;
}


bool ChatHandler::HandleLuabKickBrokenCommand(char* args)
{
	if (Player* selection = GetSelectedPlayer())
		sLuaAgentMgr.LogoutBrokenAgent(selection->GetObjectGuid());
	return true;
}


bool ChatHandler::HandleLuabRemoveAllCommand(char* args)
{
	sLuaAgentMgr.LogoutAllAgents();
	return true;
}


bool ChatHandler::HandleLuabResetCommand(char* args)
{
	sLuaAgentMgr.LuaReload();
	return true;
}


bool ChatHandler::HandleLuabGroupAllCommand(char* args)
{
	sLuaAgentMgr.GroupAll(GetSession()->GetPlayer());
	return true;
}


bool ChatHandler::HandleLuabReviveAllCommand(char* args)
{
	float health = .0f;
	int32 sickness = 0;
	if (!ExtractFloat(&args, health))
		health = 1.0f;
	else if (!ExtractInt32(&args, sickness))
		sickness = 1;
	else if (sickness > 0)
		sickness = 1;
	sLuaAgentMgr.ReviveAll(GetSession()->GetPlayer(), health, sickness == 1);
	return true;
}


bool ChatHandler::HandleLuabCLinePointCommand(char* args)
{
	Player* plyr = GetSession()->GetPlayer();
	sLuaAgentMgr.CLineSaveSeg(G3D::Vector3(plyr->GetPositionX(), plyr->GetPositionY(), plyr->GetPositionZ()), plyr);
	return true;
}


bool ChatHandler::HandleLuabCLineMoveCommand(char* args)
{
	Creature* go = GetSelectedCreature();
	if (!go)
	{
		SendSysMessage("No point selected.");
		SetSentErrorMessage(true);
		return false;
	}
	Player* plyr = GetSession()->GetPlayer();
	sLuaAgentMgr.CLineMoveSeg(G3D::Vector3(plyr->GetPositionX(), plyr->GetPositionY(), plyr->GetPositionZ()), plyr, go->GetObjectGuid());
	return true;
}


bool ChatHandler::HandleLuabCLineRemoveLastCommand(char* args)
{
	Player* plyr = GetSession()->GetPlayer();
	sLuaAgentMgr.CLineDelLastSeg(plyr);
	return true;
}


bool ChatHandler::HandleLuabCLineRemoveCommand(char* args)
{
	Player* plyr = GetSession()->GetPlayer();
	Creature* go = GetSelectedCreature();
	if (!go)
	{
		SendSysMessage("No point selected.");
		SetSentErrorMessage(true);
		return false;
	}
	return true;
}


bool ChatHandler::HandleLuabCLineWriteCommand(char* args)
{
	sLuaAgentMgr.CLineWrite();
	return true;
}


bool ChatHandler::HandleLuabCLineFinishCommand(char* args)
{
	sLuaAgentMgr.CLineFinish(GetSession()->GetPlayer());
	return true;
}


bool ChatHandler::HandleLuabCLineNewLineCommand(char* args)
{
	sLuaAgentMgr.CLineNewLine();
	return true;
}


bool ChatHandler::HandleLuabCLineLoadFromCommand(char* args)
{
	Player* player = GetSession()->GetPlayer();
	sLuaAgentMgr.CLineLoadForEdit(player, player->GetMapId());
	return true;
}


bool ChatHandler::HandleLuabClearBags(char* args)
{
	/*
	Player* player = GetSession()->GetPlayer();
	for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
	{
		Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
		if (pItem && !pItem->IsEquipped())
			player->DestroyItem(INVENTORY_SLOT_BAG_0, i, true);
	}
	//*/
	return true;
}

bool ChatHandler::HandleLuabReloadAgents(char* args)
{
	sLuaAgentMgr.ResetAgents();
	return true;
}
