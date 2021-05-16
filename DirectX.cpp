﻿#include <chrono>
#include <locale>
#include <codecvt>
#include <algorithm>
#include <filesystem>
#include <thread>
#include <iostream>
#include <fstream>

#include "Utils.h"
#include "DirectX.h"
#include "HTTP.h"

//GetObject is defined in wingdi.h and it causes conflicts with json reader
#undef GetObject
#include <aws/core/utils/json/JsonSerializer.h>

#pragma warning (disable : 4996)

extern bool MakeAuth();

static std::vector<Champ>champSkins;
void GetAllChampions(std::string patch)
{
	std::vector < Champ > temp;
	HTTP http;
	std::string req = http.Request("GET", "http://ddragon.leagueoflegends.com/cdn/" + patch + "/data/en_US/champion.json");
	Aws::String aws_s(req.c_str(), req.size());
	Aws::Utils::Json::JsonValue Info(aws_s);

	auto allObj = Info.View().GetObject("data").GetAllObjects();
	for (auto n : allObj)
	{
		req = http.Request("GET", "http://ddragon.leagueoflegends.com/cdn/" + patch + "/data/en_US/champion/" + n.first.c_str() + ".json");
		Aws::String aws_champ(req.c_str(), req.size());
		Aws::Utils::Json::JsonValue ChampInfo(aws_champ);
		Champ champ;
		champ.name = n.first.c_str();
		auto skinArr = ChampInfo.View().GetObject("data").GetObject(n.first.c_str()).GetObject("skins").AsArray();
		for (size_t i = 0; i < skinArr.GetLength(); ++i)
		{
			auto skinObj = skinArr.GetItem(i).AsObject();
			std::pair<std::string, std::string > skin;
			skin.first = std::string(skinObj.GetString("id").c_str());
			skin.second = std::string(skinObj.GetString("name").c_str());
			champ.skins.emplace_back(skin);
		}
		temp.emplace_back(champ);
	}
	champSkins = temp;
}

void CheckVersion()
{
	HTTP http;
	std::string getLatest = http.Request("GET", "https://api.github.com/repos/KebsCS/KBotExt/releases/latest");
	Aws::String aws_s(getLatest.c_str(), getLatest.size());
	Aws::Utils::Json::JsonValue Info(aws_s);
	std::string latestName = Info.View().AsObject().GetString("tag_name").c_str();
	if (latestName != "1.2.1")
	{
		if (MessageBoxA(0, "Open download website?", "New version available", MB_YESNO) == IDYES)
		{
			ShellExecute(0, 0, L"https://github.com/KebsCS/KBotExt/releases/latest", 0, 0, SW_SHOW);
		}
	}
}

bool Direct3D9Render::DirectXInit(HWND hWnd)
{
	// Setup swap chain
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT createDeviceFlags = 0;
	//createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
		return false;

	if (!CreateRenderTarget())
		return false;

	Renderimgui(hWnd);

	gamePatch = GetCurrentPatch();

	// Check latest app version
	CheckVersion();

	MakeHeader();

	std::thread t{ GetAllChampions, gamePatch };
	t.detach();

	std::thread AutoAcceptThread(&Direct3D9Render::AutoAccept, this);
	AutoAcceptThread.detach();

	return true;
}

void Direct3D9Render::MakeHeader()
{
	LolHeader = "Host: 127.0.0.1:" + std::to_string(clientPort) + "\n" +
		"Connection: keep-alive" + "\n" +
		"Authorization: Basic " + authToken + "\n" +
		"Accept: application/json" + "\n" +
		"Content-Type: application/json" + "\n" +
		"Origin: https://127.0.0.1:" + std::to_string(clientPort) + "\n" +
		"User-Agent: Mozilla/5.0 (Windows NT 6.2; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) LeagueOfLegendsClient/11.3.356.7268 (CEF 74) Safari/537.36" + "\n" +
		"X-Riot-Source: rcp-fe-lol-social" + "\n" +
		"Referer: https://127.0.0.1:" + std::to_string(clientPort) + "/index.html" + "\n" +
		"Accept-Encoding: gzip, deflate, br" + "\n" +
		"Accept-Language: en-US,en;q=0.9";
}

// Helper to display a little (?) mark which shows a tooltip when hovered.
// In your own code you may want to display an actual icon if you are using a merged icon fonts (see docs/FONTS.md)
void Direct3D9Render::HelpMarker(const char* desc)
{
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void Direct3D9Render::ArrowButtonDisabled(const char* id, ImGuiDir dir)
{
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	ImGui::ArrowButton(id, dir);
	ImGui::PopStyleVar();
}

void Direct3D9Render::StartFrame()
{
	// Start the Dear ImGui frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void Direct3D9Render::EndFrame()
{
	// Rendering
	ImVec4 clear_color = ImVec4(0, 0, 0, 255.f);
	ImGui::EndFrame();
	ImGui::Render();
	g_pd3dDeviceContext->OMSetRenderTargets(1, &g_pd3dRenderTargetView, NULL);
	g_pd3dDeviceContext->ClearRenderTargetView(g_pd3dRenderTargetView, (float*)&clear_color);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	g_pSwapChain->Present(1, 0); // Present with vsync
	//g_pSwapChain->Present(0, 0); // Present without vsync
}

float processTimeMs = 0;

std::string Direct3D9Render::GetCurrentPatch()
{
	HTTP http;
	std::string req = http.Request("GET", "https://ddragon.leagueoflegends.com/api/versions.json");
	Aws::String aws_s(req.c_str(), req.size());
	Aws::Utils::Json::JsonValue Info(aws_s);
	auto reqObj = Info.View().AsArray();

	return std::string(reqObj.GetItem(0).AsString().c_str());
}

void Direct3D9Render::AutoAccept()
{
	bool found = false;
	while (true)
	{
		if (bAutoAccept && !FindWindowA(0, "League of Legends (TM) Client"))
		{
			HTTP http;
			std::string req = http.Request("POST", "https://127.0.0.1/lol-matchmaking/v1/ready-check/accept", "", LolHeader, "", "", clientPort);
			//pressed accept
			if (req.empty())
			{
				found = true;
			}
			if (found && ((bInstalock && instalockID) || !std::string(instantMessage).empty()))
			{
				std::string lobby = http.Request("GET", "https://127.0.0.1/lol-champ-select/v1/session", "", LolHeader, "", "", clientPort);

				if (lobby.find("errorCode") == std::string::npos)
				{
					if (instalockID && bInstalock)
					{
						for (int i = 0; i < 10; i++)
							std::string lock = http.Request("PATCH", "https://127.0.0.1/lol-champ-select/v1/session/actions/" + std::to_string(i),
								R"({"completed":true,"championId":)" + std::to_string(instalockID) + "}", LolHeader, "", "", clientPort);
					}
					if (!std::string(instantMessage).empty())
					{
						Aws::String aws_s(lobby.c_str(), lobby.size());
						Aws::Utils::Json::JsonValue Info(aws_s);

						std::string lobbyID = std::string(Info.View().GetObject("chatDetails").GetString("chatRoomName").c_str());
						lobbyID = lobbyID.substr(0, lobbyID.find("@"));
						std::string param = "https://127.0.0.1/lol-chat/v1/conversations/" + lobbyID + R"(%40champ-select.eu1.pvp.net/messages)";
						std::string error = "errorCode";
						while (error.find("errorCode") != std::string::npos)
						{
							std::this_thread::sleep_for(std::chrono::milliseconds(50));
							error = http.Request("POST", param, R"({"body":")" + std::string(instantMessage) + R"("})", LolHeader, "", "", clientPort);
						}
					}

					found = false;
				}
			}
			if (!found)
				std::this_thread::sleep_for(std::chrono::milliseconds(RandomInt(1000, 1500)));
			else
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		else
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
}

void Direct3D9Render::GameTab()
{
	if (ImGui::BeginTabItem("Game"))
	{
		HTTP http;
		static std::string req;
		std::string custom;
		static int gameID = 0;
		ImGui::Text("Games:");
		ImGui::Columns(4, 0, false);
		if (ImGui::Button("Blind pick"))
			gameID = BlindPick;

		if (ImGui::Button("Draft pick"))
			gameID = DraftPick;

		if (ImGui::Button("Solo/Duo"))
			gameID = SoloDuo;

		if (ImGui::Button("Flex"))
			gameID = Flex;

		ImGui::NextColumn();

		if (ImGui::Button("ARAM"))
			gameID = ARAM;

		if (ImGui::Button("ARURF"))
			gameID = ARURF;

		/*if (ImGui::Button("URF"))
			gameID = 318;*/

		ImGui::NextColumn();

		if (ImGui::Button("TFT Normal"))
			gameID = TFTNormal;

		if (ImGui::Button("TFT Ranked"))
			gameID = TFTRanked;

		if (ImGui::Button("TFT Hyper Roll"))
			gameID = TFTHyperRoll;

		if (ImGui::Button("TFT Tutorial"))
			gameID = TFTTutorial;

		ImGui::NextColumn();

		if (ImGui::Button("Practice Tool"))
		{
			custom = R"({"customGameLobby":{"configuration":{"gameMode":"PRACTICETOOL","gameMutator":"","gameServerRegion":"","mapId":11,"mutators":{"id":1},"spectatorPolicy":"AllAllowed","teamSize":1},"lobbyName":"KBot","lobbyPassword":null},"isCustom":true})";
		}

		static bool fill = false;
		if (ImGui::Button("Practice Tool 5v5"))
		{
			custom = R"({"customGameLobby":{"configuration":{"gameMode":"PRACTICETOOL","gameMutator":"","gameServerRegion":"","mapId":11,"mutators":{"id":1},"spectatorPolicy":"AllAllowed","teamSize":5},"lobbyName":"KBot","lobbyPassword":null},"isCustom":true})";
			fill = true;
		}

		if (ImGui::Button("Clash"))
			gameID = Clash;

		ImGui::Columns(1);

		ImGui::Separator();

		ImGui::Columns(4, 0, false);

		if (ImGui::Button("Tutorial 1"))
			gameID = Tutorial1;

		if (ImGui::Button("Tutorial 2"))
			gameID = Tutorial2;

		if (ImGui::Button("Tutorial 3"))
			gameID = Tutorial3;

		ImGui::NextColumn();

		if (ImGui::Button("Intro Bots"))
			gameID = IntroBots;

		if (ImGui::Button("Beginner Bots"))
			gameID = BeginnerBots;

		if (ImGui::Button("Intermediate Bots"))
			gameID = IntermediateBots;

		ImGui::NextColumn();

		if (ImGui::Button("Custom Blind"))
			custom = R"({"customGameLobby":{"configuration":{"gameMode":"CLASSIC","gameMutator":"","gameServerRegion":"","mapId":11,"mutators":{"id":1},"spectatorPolicy":"AllAllowed","teamSize":5},"lobbyName":"KBot","lobbyPassword":null},"isCustom":true})";

		if (ImGui::Button("Custom ARAM"))
			custom = R"({"customGameLobby":{"configuration":{"gameMode":"ARAM","gameMutator":"","gameServerRegion":"","mapId":12,"mutators":{"id":1},"spectatorPolicy":"AllAllowed","teamSize":5},"lobbyName":"KBot","lobbyPassword":null},"isCustom":true})";

		//"id" 1- blind 2- draft -4 all random 6- tournament draft

		ImGui::Columns(1);
		ImGui::Separator();

		static int inputGameID = 0;
		ImGui::InputInt("##inputGameID:", &inputGameID, 1, 100);
		ImGui::SameLine();
		if (ImGui::Button("Submit"))
		{
			gameID = inputGameID;
		}

		if (gameID != 0 || !custom.empty())
		{
			std::string result;
			if (custom.empty())
			{
				result = R"({"queueId":)" + std::to_string(gameID) + "}";
			}
			else
			{
				result = custom;
				custom = "";
			}
			req = http.Request("POST", "https://127.0.0.1/lol-lobby/v2/lobby", (result), LolHeader, "", "", clientPort);
			if (req.find("errorCode") != std::string::npos)
				MessageBoxA(0, req.c_str(), 0, 0);

			/*	{
					for (int i = 0; i < 10001; i++)
					{
						std::string res = R"({"customGameLobby":{"configuration":{"gameMode":"CLASSIC","gameMutator":"","gameServerRegion":"","mapId":11,"mutators":{"id":)" + std::to_string(i) + R"(},"spectatorPolicy":"AllAllowed","teamSize":5},"lobbyName":"KBot","lobbyPassword":null},"isCustom":true})";
						std::string xdd= http.Request("POST", "https://127.0.0.1/lol-lobby/v2/lobby", (res), LolHeader, "", "", clientPort);
						if (xdd.find("errorCode") == std::string::npos)
							std::cout << i << std::endl;
						std::this_thread::sleep_for(std::chrono::milliseconds(10));
					}
				}*/

			gameID = 0;
		}
		if (fill)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(300));
			std::vector<int>champIDs = { 22, 18, 33,12,10,21,62,89,44,51,96,54,81,98,30,122,11,13,69 };
			for (int i = 0; i < 4; i++)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				std::string addBlue = R"({"botDifficulty":"MEDIUM","championId":)" + std::to_string(RandomInt(0, champIDs.size() - 1)) + R"(,"teamId":"100"})";
				req = http.Request("POST", "https://127.0.0.1/lol-lobby/v1/lobby/custom/bots", addBlue, LolHeader, "", "", clientPort);
			}
			for (int i = 0; i < 5; i++)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				std::string addRed = R"({"botDifficulty":"MEDIUM","championId":)" + std::to_string(RandomInt(0, champIDs.size() - 1)) + R"(,"teamId":"200"})";
				req = http.Request("POST", "https://127.0.0.1/lol-lobby/v1/lobby/custom/bots", addRed, LolHeader, "", "", clientPort);
			}
			fill = false;
		}
		ImGui::Separator();

		if (ImGui::Button("Start queue"))
		{
			req = http.Request("POST", "https://127.0.0.1/lol-lobby/v2/lobby/matchmaking/search", "", LolHeader, "", "", clientPort);
		}
		ImGui::SameLine();
		if (ImGui::Button("Dodge"))
		{
			req = http.Request("POST", R"(https://127.0.0.1/lol-login/v1/session/invoke?destination=lcdsServiceProxy&method=call&args=["","teambuilder-draft","quitV2",""])", "", LolHeader, "", "", clientPort);
		}

		ImGui::Checkbox("Auto accept", &bAutoAccept);

		ImGui::Text("Instant message:");
		ImGui::InputText("##inputInstantMessage", instantMessage, IM_ARRAYSIZE(instantMessage));
		ImGui::Checkbox("Instalock", &bInstalock);
		if (ImGui::CollapsingHeader("Instalock champ"))
		{
			for (auto min : champsMinimal)
			{
				if (!min.owned)
					continue;

				char bufchamp[128];
				sprintf_s(bufchamp, "##Select %s", min.alias.c_str());
				ImGui::Text("%s", min.alias.c_str());
				ImGui::SameLine();
				ImGui::RadioButton(bufchamp, &instalockID, min.id);
			}
		}

		ImGui::Separator();

		static std::string boostMessage;
		if (ImGui::Button("Boost"))
		{
			if (MessageBoxA(0, "Are you sure? It will consume RP if you have enough for a boost", 0, MB_OKCANCEL) == IDOK)
			{
				boostMessage = http.Request("POST", R"(https://127.0.0.1/lol-login/v1/session/invoke?destination=lcdsServiceProxy&method=call&args=["","teambuilder-draft","activateBattleBoostV1",""])", "", LolHeader, "", "", clientPort);
			}
		}
		ImGui::SameLine();
		ImGui::Text("ARAM/ARURF Boost, use only if you don't have enough RP for boost");

		ImGui::TextWrapped("%s", req.c_str());
		ImGui::TextWrapped("%s", boostMessage.c_str());
		ImGui::EndTabItem();
	}
}

void Direct3D9Render::ProfileTab()
{
	if (ImGui::BeginTabItem("Profile"))
	{
		HTTP http;
		static char statusText[1024 * 16];
		ImGui::Text("Status:");
		ImGui::InputTextMultiline("##source", (statusText), IM_ARRAYSIZE(statusText), ImVec2(400, 100), ImGuiInputTextFlags_AllowTabInput);
		if (ImGui::Button("Submit status"))
		{
			std::string result = "{\"statusMessage\":\"" + std::string(statusText) + "\"}";

			size_t nPos = 0;
			while (nPos != std::string::npos)
			{
				nPos = result.find("\n", nPos);
				if (nPos != std::string::npos)
				{
					result.erase(result.begin() + nPos);
					result.insert(nPos, "\\n");
				}
			}
			std::string req = http.Request("PUT", "https://127.0.0.1/lol-chat/v1/me", (result), LolHeader, "", "", clientPort);
			if (req.find("errorCode") != std::string::npos)
				MessageBoxA(0, req.c_str(), 0, 0);
		}

		ImGui::SameLine();
		static int availability = 0;
		static int lastavailability = 0;
		ImGui::RadioButton("Online", &availability, 0); ImGui::SameLine();
		ImGui::RadioButton("Mobile", &availability, 1); ImGui::SameLine();
		ImGui::RadioButton("Away", &availability, 2); ImGui::SameLine();
		ImGui::RadioButton("Offline", &availability, 3);

		if (availability != lastavailability)
		{
			lastavailability = availability;
			std::string body = R"({"availability":")";
			switch (availability)
			{
			case 0:
				body += "online";
				break;
			case 1:
				body += "mobile";
				break;
			case 2:
				body += "away";
				break;
			case 3:
				body += "offline";
				break;
			}
			body += "\"}";
			http.Request("PUT", "https://127.0.0.1/lol-chat/v1/me", body, LolHeader, "", "", clientPort);
		}

		ImGui::Separator();

		ImGui::Text("Rank:");
		static int rank = 0;
		ImGui::RadioButton("Iron", &rank, 0); ImGui::SameLine();
		ImGui::RadioButton("Bronze", &rank, 1); ImGui::SameLine();
		ImGui::RadioButton("Silver", &rank, 2); ImGui::SameLine();
		ImGui::RadioButton("Gold", &rank, 3); ImGui::SameLine();
		ImGui::RadioButton("Platinum", &rank, 4); ImGui::SameLine();
		ImGui::RadioButton("Diamond", &rank, 5); ImGui::SameLine();
		ImGui::RadioButton("Master", &rank, 6); ImGui::SameLine();
		ImGui::RadioButton("GrandMaster", &rank, 7); ImGui::SameLine();
		ImGui::RadioButton("Challenger", &rank, 8);

		static int tier = 0;
		ImGui::RadioButton("I", &tier, 0); ImGui::SameLine();
		ImGui::RadioButton("II", &tier, 1); ImGui::SameLine();
		ImGui::RadioButton("III", &tier, 2); ImGui::SameLine();
		ImGui::RadioButton("IV", &tier, 3); ImGui::SameLine();
		ImGui::RadioButton("None", &tier, 4);

		static int queue = 0;
		ImGui::RadioButton("Solo/Duo", &queue, 0); ImGui::SameLine();
		ImGui::RadioButton("Flex 5v5", &queue, 1); ImGui::SameLine();
		ImGui::RadioButton("Flex 3v3", &queue, 2); ImGui::SameLine();
		ImGui::RadioButton("TFT", &queue, 3); ImGui::SameLine();
		ImGui::RadioButton("()", &queue, 4);

		if (ImGui::Button("Submit##rank"))
		{
			std::string body = R"({"lol":{"rankedLeagueQueue":")";
			switch (queue)
			{
			case 0:
				body += "RANKED_SOLO_5x5";
				break;
			case 1:
				body += "RANKED_FLEX_SR";
				break;
			case 2:
				body += "RANKED_FLEX_TT";
				break;
			case 3:
				body += "RANKED_TFT";
				break;
			case 4:
				body += "";
				break;
			}

			body += R"(","rankedLeagueTier":")";

			switch (rank)
			{
			case 0:
				body += "IRON";
				break;
			case 1:
				body += "BRONZE";
				break;
			case 2:
				body += "SILVER";
				break;
			case 3:
				body += "GOLD";
				break;
			case 4:
				body += "PLATINUM";
				break;
			case 5:
				body += "DIAMOND";
				break;
			case 6:
				body += "MASTER";
				break;
			case 7:
				body += "GRANDMASTER";
				break;
			case 8:
				body += "CHALLENGER";
				break;
			}

			body += R"(","rankedLeagueDivision":")";

			switch (tier)
			{
			case 0:
				body += "I";
				break;
			case 1:
				body += "II";
				break;
			case 2:
				body += "III";
				break;
			case 3:
				body += "IV";
				break;
			case 4:
				body += "";
				break;
			}

			body += R"("}})";

			http.Request("PUT", "https://127.0.0.1/lol-chat/v1/me", body, LolHeader, "", "", clientPort);
		}

		ImGui::SameLine();
		if (ImGui::Button("Empty##rank"))
		{
			http.Request("PUT", "https://127.0.0.1/lol-chat/v1/me", R"({"lol":{"rankedLeagueQueue":"","rankedLeagueTier":"","rankedLeagueDivision":""}})", LolHeader, "", "", clientPort);
		}

		ImGui::Separator();

		static int iconID;
		ImGui::Text("Icon:");
		ImGui::InputInt("##Icon:", &iconID, 1, 100);
		ImGui::SameLine();
		if (ImGui::Button("Submit##icon"))
		{
			std::string result = R"({"profileIconId":)" + std::to_string(iconID) + "}";
			std::string req = http.Request("PUT", "https://127.0.0.1/lol-summoner/v1/current-summoner/icon", (result), LolHeader, "", "", clientPort);
			if (req.find("errorCode") != std::string::npos)
			{
				MessageBoxA(0, req.c_str(), 0, 0);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Submit 2##icon"))
		{
			std::string result = R"({"icon":)" + std::to_string(iconID) + "}";
			std::string req = http.Request("PUT", "https://127.0.0.1/lol-chat/v1/me/", (result), LolHeader, "", "", clientPort);
			if (req.find("errorCode") != std::string::npos)
			{
				MessageBoxA(0, req.c_str(), 0, 0);
			}
		}

		static int backgroundID;
		ImGui::Text("Background:");

		ImGui::InputInt("##Background:", &backgroundID, 1, 100);
		ImGui::SameLine();
		if (ImGui::Button("Submit##background"))
		{
			std::string result = R"({"key":"backgroundSkinId","value":)" + std::to_string(backgroundID) + "}";
			std::string req = http.Request("POST", "https://127.0.0.1/lol-summoner/v1/current-summoner/summoner-profile/", (result), LolHeader, "", "", clientPort);
			//MessageBoxA(0, req.c_str(), 0, 0);
		}

		if (ImGui::CollapsingHeader("Backgrounds"))
		{
			if (champSkins.empty())
			{
				ImGui::Text("Skin data still downloading");
			}
			else
			{
				for (auto c : champSkins)
				{
					if (ImGui::TreeNode(c.name.c_str()))
					{
						for (auto s : c.skins)
						{
							if (ImGui::Button(s.second.c_str()))
							{
								std::string result = R"({"key":"backgroundSkinId","value":)" + s.first + "}";
								std::string req = http.Request("POST", "https://127.0.0.1/lol-summoner/v1/current-summoner/summoner-profile/", (result), LolHeader, "", "", clientPort);
							}
						}
						ImGui::TreePop();
					}
				}
			}
		}

		ImGui::EndTabItem();
	}
}

void Direct3D9Render::SessionTab()
{
	if (ImGui::BeginTabItem("Session"))
	{
		HTTP http;
		if (sessionOpen)
			session = http.Request("GET", "https://127.0.0.1/lol-login/v1/session", "", LolHeader, "", "", clientPort);

		Aws::String aws_s(session.c_str(), session.size());
		Aws::Utils::Json::JsonValue Info(aws_s);

		auto reqObj = Info.View().AsObject();

		ImGui::Text("accountId: %s", std::to_string(reqObj.GetInt64("accountId")).c_str());
		ImGui::Text("connected: %d", reqObj.GetBool("connected"));
		ImGui::TextWrapped("idToken: %s", reqObj.GetString("idToken").c_str());
		ImGui::Text("isInLoginQueue: %d", reqObj.GetBool("isInLoginQueue"));
		ImGui::Text("isNewPlayer: %d", reqObj.GetBool("isNewPlayer"));
		ImGui::Text("puuid: %s", reqObj.GetString("puuid").c_str());
		ImGui::Text("state: %s", reqObj.GetString("state").c_str());
		ImGui::Text("summonerId: %d", reqObj.GetInteger("summonerId"));
		ImGui::Text("userAuthToken: %d", reqObj.GetInteger("userAuthToken"));
		ImGui::Text("username: %s", reqObj.GetString("username").c_str());

		if (ImGui::Button("Copy to clipboard##sessionTab"))
		{
			utils->CopyToClipboard(session);
		}

		//ImGui::TextWrapped(req.c_str());
		sessionOpen = false;
		ImGui::EndTabItem();
	}
	else
		sessionOpen = true;
}

void Direct3D9Render::InfoTab()
{
	if (ImGui::BeginTabItem("Info"))
	{
		HTTP http;
		static bool display = false;
		static std::string req;
		static char playerName[50];
		static std::string accID;
		static int summID;
		static std::string summName;
		ImGui::Text("Input player name:");
		ImGui::InputText("##inputPlayerName", playerName, IM_ARRAYSIZE(playerName));
		ImGui::SameLine();

		if (ImGui::Button("Submit##playerName"))
		{
			req = http.Request("GET", "https://127.0.0.1/lol-summoner/v1/summoners?name=" + std::string(playerName), "", LolHeader, "", "", clientPort);
			display = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("puuid##playerName"))
		{
			req = http.Request("GET", "https://127.0.0.1/lol-summoner/v1/summoners-by-puuid-cached/" + std::string(playerName), "", LolHeader, "", "", clientPort);
			display = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("summId##playerName"))
		{
			req = http.Request("GET", "https://127.0.0.1/lol-summoner/v1/summoners/" + std::string(playerName), "", LolHeader, "", "", clientPort);
			display = true;
		}
		if (req.find("errorCode") != std::string::npos)
		{
			ImGui::TextWrapped("%s", req.c_str());
		}
		else if (display)
		{
			Aws::String aws_s(req.c_str(), req.size());
			Aws::Utils::Json::JsonValue Info(aws_s);

			auto reqObj = Info.View().AsObject();
			accID = std::to_string(reqObj.GetInt64("accountId")).c_str();
			ImGui::Text("accountId: %s", accID.c_str());
			ImGui::TextWrapped("displayName: %s", reqObj.GetString("displayName").c_str());
			summName = reqObj.GetString("internalName").c_str();
			ImGui::TextWrapped("internalName: %s", summName.c_str());
			ImGui::Text("nameChangeFlag: %d", reqObj.GetInteger("nameChangeFlag"));
			ImGui::Text("percentCompleteForNextLevel: %d", reqObj.GetInteger("percentCompleteForNextLevel"));
			ImGui::Text("profileIconId: %d", reqObj.GetInteger("profileIconId"));
			ImGui::Text("puuid: %s", reqObj.GetString("puuid").c_str());
			summID = reqObj.GetInteger("summonerId");
			ImGui::Text("summonerId: %d", summID);
			ImGui::Text("summonerLevel: %d", reqObj.GetInteger("summonerLevel"));
			ImGui::Text("unnamed: %d", reqObj.GetInteger("unnamed"));
			ImGui::Text("xpSinceLastLevel: %d", reqObj.GetInteger("xpSinceLastLevel"));
			ImGui::Text("xpUntilNextLevel: %d", reqObj.GetInteger("xpUntilNextLevel"));

			auto rerollPointsObj = reqObj.GetObject("rerollPoints");
			ImGui::Text("currentPoints: %d", rerollPointsObj.GetInteger("currentPoints"));
			ImGui::Text("maxRolls: %d", rerollPointsObj.GetInteger("maxRolls"));
			ImGui::Text("numberOfRolls: %d", rerollPointsObj.GetInteger("numberOfRolls"));
			ImGui::Text("pointsCostToRoll: %d", rerollPointsObj.GetInteger("pointsCostToRoll"));
			ImGui::Text("pointsToReroll: %d", rerollPointsObj.GetInteger("pointsToReroll"));
		}
		if (ImGui::Button("Invite to lobby##infoTab"))
		{
			std::string invite = R"([{"toSummonerId":)" + accID + R"(}])";
			http.Request("POST", "https://127.0.0.1/lol-lobby/v2/lobby/invitations", invite, LolHeader, "", "", clientPort);
			invite = R"([{"toSummonerId":)" + std::to_string(summID) + R"(}])";
			http.Request("POST", "https://127.0.0.1/lol-lobby/v2/lobby/invitations", invite, LolHeader, "", "", clientPort);
		}
		ImGui::SameLine();
		if (ImGui::Button("Invite to friends##infoTab"))
		{
			std::string invite = R"({"name":")" + summName + R"("})";
			http.Request("POST", "https://127.0.0.1/lol-chat/v1/friend-requests", invite, LolHeader, "", "", clientPort);
		}
		ImGui::SameLine();
		if (ImGui::Button("Add to block list##infoTab"))
		{
			std::string body = R"({ "summonerId":)" + std::to_string(summID) + "}";
			http.Request("POST", "https://127.0.0.1/lol-chat/v1/blocked-players", body, LolHeader, "", "", clientPort);
		}

		if (ImGui::Button("Copy to clipboard##infoTab"))
		{
			utils->CopyToClipboard(req);
		}

		ImGui::EndTabItem();
	}
}

void Direct3D9Render::ChampsTab()
{
	if (ImGui::BeginTabItem("Champs"))
	{
		HTTP http;
		static std::string req;
		static std::string req2;
		static bool added = false;
		static int iChampsOwned = 0;
		if (champsOpen)
		{
			session = http.Request("GET", "https://127.0.0.1/lol-login/v1/session", "", LolHeader, "", "", clientPort);
			Aws::String aws_s(session.c_str(), session.size());
			Aws::Utils::Json::JsonValue Info(aws_s);

			auto reqObj = Info.View().AsObject();
			int summonerId = reqObj.GetInteger("summonerId");

			req = http.Request("GET", "https://127.0.0.1/lol-collections/v1/inventories/" + std::to_string(summonerId) + "/champion-mastery", "", LolHeader, "", "", clientPort);

			req2 = http.Request("GET", "https://127.0.0.1/lol-champions/v1/inventories/" + std::to_string(summonerId) + "/champions-minimal", "", LolHeader, "", "", clientPort);

			added = false;
			iChampsOwned = 0;
			champsMinimal.clear();
			champsMastery.clear();
		}
		if (req.find("errorCode") != std::string::npos || req2.find("errorCode") != std::string::npos || req == "[]")
		{
			ImGui::TextWrapped("%s", req.c_str());

			ImGui::TextWrapped("%s", req2.c_str());
		}
		else
		{
			if (!added)
			{
				Aws::String aws_s2(req2.c_str(), req2.size());
				Aws::Utils::Json::JsonValue Info2(aws_s2);
				auto req2Obj = Info2.View().AsArray();
				for (size_t i = 0; i < req2Obj.GetLength(); ++i)
				{
					auto champObj = req2Obj.GetItem(i).AsObject();
					ChampMinimal champ;

					champ.active = champObj.GetInteger("active");
					champ.alias = champObj.GetString("alias").c_str();
					champ.banVoPath = champObj.GetString("banVoPath").c_str();
					champ.baseLoadScreenPath = champObj.GetString("baseLoadScreenPath").c_str();
					champ.botEnabled = champObj.GetInteger("botEnabled");
					champ.chooseVoPath = champObj.GetString("chooseVoPath").c_str();
					champ.freeToPlay = champObj.GetInteger("freeToPlay");
					champ.id = champObj.GetInteger("id");
					champ.name = champObj.GetString("name").c_str();
					auto ownershipObj = champObj.GetObject("ownership");
					champ.freeToPlayReward = ownershipObj.GetInteger("freeToPlayReward");
					champ.owned = ownershipObj.GetInteger("owned");
					if (champ.owned)
						iChampsOwned++;
					champ.purchased = std::to_string(champObj.GetInt64("purchased")).c_str();
					champ.rankedPlayEnabled = champObj.GetInteger("rankedPlayEnabled");
					//auto rolesObj = champObj.GetObject("roles"); //todo
					champ.squarePortraitPath = champObj.GetString("squarePortraitPath").c_str();
					champ.stingerSfxPath = champObj.GetString("stingerSfxPath").c_str();
					champ.title = champObj.GetString("title").c_str();

					champsMinimal.emplace_back(champ);
				}

				Aws::String aws_s(req.c_str(), req.size());
				Aws::Utils::Json::JsonValue Info(aws_s);

				auto reqObj = Info.View().AsArray();
				for (size_t i = 0; i < reqObj.GetLength(); ++i)
				{
					auto champObj = reqObj.GetItem(i).AsObject();

					//todo
					//get bought time from timestamp
					//input player name

					ChampMastery champ;
					champ.championId = champObj.GetInteger("championId");
					champ.championLevel = champObj.GetInteger("championLevel");
					champ.championPoints = champObj.GetInteger("championPoints");
					champ.championPointsSinceLastLevel = champObj.GetInteger("championPointsSinceLastLevel");
					champ.championPointsUntilNextLevel = champObj.GetInteger("championPointsUntilNextLevel");
					champ.chestGranted = champObj.GetInteger("chestGranted");
					champ.formattedChampionPoints = champObj.GetString("formattedChampionPoints").c_str();
					champ.formattedMasteryGoal = champObj.GetString("formattedMasteryGoal").c_str();
					champ.highestGrade = champObj.GetInteger("highestGrade");
					champ.lastPlayTime = std::to_string(champObj.GetInt64("lastPlayTime")).c_str();
					champ.playerId = std::to_string(champObj.GetInt64("playerId")).c_str();
					champ.tokensEarned = champObj.GetInteger("tokensEarned");

					champsMastery.emplace_back(champ);
				}
				//sort alphabetcally
				std::sort(champsMinimal.begin(), champsMinimal.end(), [](const ChampMinimal& lhs, const ChampMinimal& rhs) {
					return lhs.name < rhs.name;
					});
				added = true;
			}

			ImGui::Text("Champions owned: %d", iChampsOwned);
			for (auto min : champsMinimal)
			{
				if (!min.owned)
					continue;

				ImGui::Separator();
				ImGui::Text("name: %s", min.name.c_str());
				int64_t t = std::stoll(min.purchased);
				t /= 1000;
				char buffer[50];
				strftime(buffer, 100, "%Y-%m-%d %H:%M:%S", localtime(&t));
				ImGui::Text("purchased: %s", buffer);
				ImGui::Text("id: %d", min.id);
				for (auto man : champsMastery)
				{
					if (min.id == man.championId)
					{
						ImGui::Text("championLevel: %d", man.championLevel);
						ImGui::Text("championPoints: %d", man.championPoints);
						ImGui::Text("championPointsSinceLastLevel: %d", man.championPointsSinceLastLevel);
						ImGui::Text("championPointsUntilNextLevel: %d", man.championPointsUntilNextLevel);
						ImGui::Text("chestGranted: %d", man.chestGranted);
						ImGui::Text("formattedChampionPoints: %s", man.formattedChampionPoints.c_str());
						ImGui::Text("formattedMasteryGoal: %s", man.formattedMasteryGoal.c_str());
						ImGui::Text("highestGrade: %d", man.highestGrade);
						ImGui::Text("lastPlayTime: %s", man.lastPlayTime.c_str());
						ImGui::Text("playerId: %s", man.playerId.c_str());
						ImGui::Text("tokensEarned: %d", man.tokensEarned);

						break;
					}
				}
			}
		}

		//ImGui::TextWrapped(req.c_str());
		champsOpen = false;
		ImGui::EndTabItem();
	}
	else
		champsOpen = true;
}

void Direct3D9Render::LaunchOldClient()
{
	if (!std::filesystem::exists("C:/Riot Games/League of Legends/LoL Companion"))
	{
		std::filesystem::create_directory("C:/Riot Games/League of Legends/LoL Companion");
	}
	if (!std::filesystem::exists("C:/Riot Games/League of Legends/LoL Companion/system.yaml"))
	{
		std::ifstream infile("C:/Riot Games/League of Legends/system.yaml");
		std::ofstream outfile("C:/Riot Games/League of Legends/LoL Companion/system.yaml");
		std::string content = "";
		int i;

		for (i = 0; infile.eof() != true; i++)
			content += infile.get();

		infile.close();
		size_t pos = content.find("riotclient:");
		content = content.substr(0, pos + 11);

		outfile << content;
		outfile.close();
	}

	if (::FindWindowA(0, "League of Legends"))
	{
		HTTP http;
		http.Request("POST", "https://127.0.0.1/process-control/v1/process/quit", "", LolHeader, "", "", clientPort);

		// wait for client to close (maybe theres a better method of doing that)
		std::this_thread::sleep_for(std::chrono::milliseconds(4500));
	}

	ShellExecute(NULL, L"open", L"\"C:\\Riot Games\\League of Legends\\LeagueClient.exe\"", L"--system-yaml-override=\"C:\\Riot Games\\League of Legends\\LoL Companion\\system.yaml\"", NULL, SW_SHOWNORMAL);
}

void Direct3D9Render::SkinsTab()
{
	if (ImGui::BeginTabItem("Skins"))
	{
		HTTP http;
		static std::string req;
		static Aws::Utils::Json::JsonValue Info;
		if (skinsOpen)
		{
			req = http.Request("GET", "https://127.0.0.1/lol-inventory/v2/inventory/CHAMPION_SKIN", "", LolHeader, "", "", clientPort);
			Aws::String aws_s(req.c_str(), req.size());
			Info = aws_s;
		}

		auto skinsArr = Info.View().AsArray();

		ImGui::Text("Skins owned: %d", skinsArr.GetLength());

		for (size_t i = 0; i < skinsArr.GetLength(); ++i)
		{
			ImGui::Separator();
			auto skinObj = skinsArr.GetItem(i).AsObject();
			int itemId = skinObj.GetInteger("itemId");
			for (auto champ : champSkins)
			{
				for (auto skin : champ.skins)
				{
					if (std::to_string(itemId) == skin.first)
					{
						ImGui::Text("Name: %s", skin.second.c_str());
					}
				}
			}

			ImGui::Text("inventoryType: %s", skinObj.GetString("inventoryType").c_str());
			ImGui::Text("itemId: %d", itemId);
			ImGui::Text("ownershipType: %s", skinObj.GetString("ownershipType").c_str());
			auto payloadObj = skinObj.GetObject("payload");
			ImGui::Text("isVintage: %d", payloadObj.GetInteger("isVintage"));
			std::string purchaseDateFormatted = skinObj.GetString("purchaseDate").c_str();
			purchaseDateFormatted.insert(4, ".");
			purchaseDateFormatted.insert(7, ".");
			purchaseDateFormatted.insert(10, " ");
			ImGui::Text("purchaseDate: %s", purchaseDateFormatted.c_str());
			ImGui::Text("quantity: %d", skinObj.GetInteger("quantity"));
			ImGui::Text("uuid: %s", skinObj.GetString("uuid").c_str());
		}

		skinsOpen = false;
		ImGui::EndTabItem();
	}
	else skinsOpen = true;
}

void Direct3D9Render::LootTab()
{
	if (ImGui::BeginTabItem("Loot"))
	{
		HTTP http;
		static std::string req;
		static std::string getLoot;
		static Aws::Utils::Json::JsonValue Info;
		if (lootOpen)
		{
			getLoot = http.Request("GET", "https://127.0.0.1/lol-loot/v1/player-loot-map", "", LolHeader, "", "", clientPort);
			Aws::String aws_s(getLoot.c_str(), getLoot.size());
			Info = aws_s;
		}

		auto lootArr = Info.View().AsObject().GetAllObjects();

		//GET /lol-loot/v1/player-display-categories
		//["CHEST","CHAMPION","SKIN","COMPANION","ETERNALS","EMOTE","WARDSKIN","SUMMONERICON"]

		//craft keys
		//lol-loot/v1/recipes/MATERIAL_key_fragment_forge/craft?repeat=1
		//["MATERIAL_key_fragment"]

		//disenchant skins
		//lol-loot/v1/recipes/SKIN_RENTAL_disenchant/craft?repeat=1
		//["CHAMPION_SKIN_RENTAL_36002"]

		//disenchant eternals
		//lol-loot/v1/recipes/STATSTONE_SHARD_DISENCHANT/craft?repeat=1
		//["STATSTONE_SHARD_66600058"]

		//disenchant wards
		//lol-loot/v1/recipes/WARDSKIN_RENTAL_disenchant/craft?repeat=1
		//["WARD_SKIN_RENTAL_199"]

		//disenchant champ shards
		//lol-loot/v1/recipes/CHAMPION_RENTAL_disenchant/craft?repeat=1
		//["CHAMPION_RENTAL_22"]

		if (ImGui::Button("Craft Key"))
			req = http.Request("POST", "https://127.0.0.1/lol-loot/v1/recipes/MATERIAL_key_fragment_forge/craft?repeat=1", "[\"MATERIAL_key_fragment\"]", LolHeader, "", "", clientPort);

		if (ImGui::Button("Open Chest"))
			req = http.Request("POST", "https://127.0.0.1/lol-loot/v1/recipes/CHEST_generic_OPEN/craft?repeat=1", R"(["CHEST_generic","MATERIAL_key"])", LolHeader, "", "", clientPort);

		if (ImGui::Button("Open Mastery Chest"))
			req = http.Request("POST", "https://127.0.0.1/lol-loot/v1/recipes/CHEST_champion_mastery_OPEN/craft?repeat=1", R"(["CHEST_champion_mastery","MATERIAL_key"])", LolHeader, "", "", clientPort);

		if (ImGui::Button("Disenchant all champion shards"))
		{
			if (MessageBoxA(0, "Are you sure?", 0, MB_OKCANCEL) == IDOK)
			{
				int i = 0;
				for (auto a : lootArr)
				{
					std::string name = a.first.c_str();
					if (name.find("CHAMPION_RENTAL") != std::string::npos)
					{
						std::string body = "[\"" + name + "\"]";
						http.Request("POST", "https://127.0.0.1/lol-loot/v1/recipes/CHAMPION_RENTAL_disenchant/craft?repeat=1", body, LolHeader, "", "", clientPort);
						i++;
					}
				}
				req = "Disenchanted " + std::to_string(i) + " champion shards";
			}
		}

		ImGui::TextWrapped("%s", req.c_str());

		lootOpen = false;
		ImGui::EndTabItem();
	}
	else
		lootOpen = true;
}

void Direct3D9Render::MiscTab()
{
	if (ImGui::BeginTabItem("Misc"))
	{
		static std::string miscReq;
		HTTP http;

		ImGui::Columns(2, 0, false);

		if (ImGui::Button("Launch another client"))
		{
			ShellExecute(NULL, NULL, L"C:\\Riot Games\\Riot Client\\RiotClientServices.exe", L"--launch-product=league_of_legends --launch-patchline=live --allow-multiple-clients", NULL, SW_SHOWNORMAL);
		}

		ImGui::NextColumn();

		if (ImGui::Button("Launch legacy client"))
		{
			if (!std::filesystem::exists("C:/Riot Games/League of Legends/"))
			{
				//todo typing in lol path
				miscReq = "League isnt installed in default path";
			}
			else
			{
				LaunchOldClient();
			}
		}

		ImGui::Columns(1);

		if (ImGui::Button("Restart UX"))
		{
			miscReq = http.Request("POST", "https://127.0.0.1/riotclient/kill-and-restart-ux", "", LolHeader, "", "", clientPort);
			if (miscReq.find("failed") != std::string::npos)
			{
				MakeAuth();
				Direct3D9.MakeHeader();
				miscReq = "Rehooked to new league client";
			}
		}

		if (ImGui::Button("Close client"))
			miscReq = http.Request("POST", "https://127.0.0.1/process-control/v1/process/quit", "", LolHeader, "", "", clientPort);

		ImGui::Separator();

		if (ImGui::Button("Free Tristana + Riot Girl skin"))
			miscReq = http.Request("POST", "https://127.0.0.1/lol-login/v1/session/invoke?destination=inventoryService&method=giftFacebookFan&args=[]", "", LolHeader, "", "", clientPort);

		if (ImGui::Button("Remove all friends"))
		{
			if (MessageBoxA(0, "Are you sure?", 0, MB_OKCANCEL) == IDOK)
			{
				std::string getFriends = http.Request("GET", "https://127.0.0.1/lol-chat/v1/friends", "", LolHeader, "", "", clientPort);
				Aws::String aws_s(getFriends.c_str(), getFriends.size());
				Aws::Utils::Json::JsonValue Info(aws_s);

				auto friendArr = Info.View().AsArray();
				size_t friendArrSize = friendArr.GetLength();
				for (size_t i = 0; i < friendArrSize; ++i)
				{
					auto friendObj = friendArr.GetItem(i).AsObject();

					std::string req = "https://127.0.0.1/lol-chat/v1/friends/" + std::string(friendObj.GetString("pid").c_str());
					http.Request("DELETE", req, "", LolHeader, "", "", clientPort);
				}
				miscReq = "Deleted " + std::to_string(friendArrSize) + " friends";
			}
		}

		ImGui::TextWrapped(miscReq.c_str());
		ImGui::EndTabItem();
	}
}

void Direct3D9Render::CustomTab()
{
	if (ImGui::BeginTabItem("Custom"))
	{
		HTTP http;

		static char method[50];
		ImGui::Text("Method:");
		ImGui::InputText("##inputMethod", method, IM_ARRAYSIZE(method));

		static char urlText[1024 * 16];
		ImGui::Text("URL:");
		ImGui::InputTextMultiline("##inputUrl", urlText, IM_ARRAYSIZE(urlText), ImVec2(600, 20));

		static char requestText[1024 * 16];
		ImGui::Text("Request:");
		ImGui::InputTextMultiline("##requestText", (requestText), IM_ARRAYSIZE(requestText), ImVec2(600, 100), ImGuiInputTextFlags_AllowTabInput);

		static std::string customHeader = LolHeader;
		static int customPort = clientPort;

		if (ImGui::CollapsingHeader("Custom Port/Header"))
		{
			static char inputPort[64] = "";
			ImGui::Text("Port:");
			ImGui::InputText("##inputPort", inputPort, 64, ImGuiInputTextFlags_CharsDecimal);
			std::string sPort = std::string(inputPort);
			if (!sPort.empty())
				customPort = std::stoi(sPort);
			else
				customPort = -1;

			static char inputHeader[1024 * 16];
			ImGui::Text("Header:");
			ImGui::InputTextMultiline("##inputHeader", (inputHeader), IM_ARRAYSIZE(inputHeader), ImVec2(600, 100), ImGuiInputTextFlags_AllowTabInput);
			std::string sHeader = std::string(inputHeader);
			customHeader = sHeader;
		}
		else
		{
			customHeader = LolHeader;
			customPort = clientPort;
		}

		static std::string req;
		if (ImGui::Button("Send custom request##customTab"))
		{
			std::string sURL = std::string(urlText);

			if (sURL.find("https://127.0.0.1") == std::string::npos)
			{
				if (sURL.find("https://") == std::string::npos && sURL.find("http://") == std::string::npos)
				{
					if (sURL[0] != '/')
						sURL.insert(0, "/");
					sURL.insert(0, "https://127.0.0.1");
				}
			}
			req = http.Request(method, sURL, requestText, customHeader, "", "", customPort);
		}
		ImGui::Text("Result:");
		ImGui::SameLine();
		if (ImGui::Button("Copy to clipboard##customTab"))
		{
			utils->CopyToClipboard(req);
		}
		ImGui::TextWrapped(req.c_str());

		/*static int i = 25360;
		while (true)
		{
			std::string mmemam = "https://127.0.0.1/lol-clash/v1/tournament/"+std::to_string(i)+"/create-roster";
			std::string nawasd = http.Request("POST"), mmemam, R"({"name":"","shortName":"","iconId":0})", LolHeader, "", "", clientPort);
			if (nawasd.find("INVALID") == std::string::npos)
			{
				std::cout << nawasd << std::endl;
			}
			std::cout << i << std::endl;

			i++;
		}*/

		ImGui::EndTabItem();
	}
}

void Direct3D9Render::InvokeTab()
{
	if (ImGui::BeginTabItem("Invoke"))
	{
		HTTP http;

		static char destination[1024 * 16];
		ImGui::Text("Destination:");
		ImGui::InputTextMultiline("##inputDestination", destination, IM_ARRAYSIZE(destination), ImVec2(600, 20));

		static char method[1024 * 16];
		ImGui::Text("Method:");
		ImGui::InputTextMultiline("##inputMethod", method, IM_ARRAYSIZE(method), ImVec2(600, 20));

		static char args[1024 * 16];
		ImGui::Text("Args:");
		ImGui::InputTextMultiline("##inputArgs", args, IM_ARRAYSIZE(args), ImVec2(600, 20));

		static std::string result;
		if (ImGui::Button("Submit"))
		{
			std::string req = "https://127.0.0.1/lol-login/v1/session/invoke?destination=" + std::string(destination) + "&method=" + std::string(method) + "&args=[" + std::string(args) + "]";
			result = http.Request("POST", req, "", LolHeader, "", "", clientPort);
		}

		ImGui::Text("Result:");
		ImGui::SameLine();

		if (ImGui::Button("Copy to clipboard##invokeTab"))
		{
			utils->CopyToClipboard(result);
		}
		ImGui::TextWrapped(result.c_str());

		ImGui::EndTabItem();
	}
}

int Direct3D9Render::Render()
{
	auto timeBefore = std::chrono::high_resolution_clock::now();

	char buf[128];
	sprintf_s(buf, ("KBotExt by kebs#9546 - %s \t %s ###AnimatedTitle"), gamePatch.c_str(), champSkins.empty() ? "Downloading skin data..." : "");

	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(685, 462), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = /*ImGuiWindowFlags_NoTitleBar |*/ ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
	ImGui::Begin(buf, (bool*)0, flags);// , ImGuiWindowFlags_AlwaysAutoResize);

	ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_Reorderable;
	if (ImGui::BeginTabBar("TabBar", tab_bar_flags))
	{
		if (!closedClient)
		{
			GameTab();

			ProfileTab();

			SessionTab();

			InfoTab();

			ChampsTab();

			SkinsTab();

			LootTab();

			MiscTab();

			CustomTab();

			InvokeTab();
		}
		else
		{
			static std::string closedReq;
			//ImGui::Text("Client closed"));

			ImGui::Columns(2, 0, false);

			if (ImGui::Button("Launch client"))
			{
				ShellExecute(NULL, NULL, L"C:\\Riot Games\\Riot Client\\RiotClientServices.exe", L"--launch-product=league_of_legends --launch-patchline=live", NULL, SW_SHOWNORMAL);
			}

			ImGui::NextColumn();

			if (ImGui::Button("Launch legacy client"))
			{
				if (!std::filesystem::exists("C:/Riot Games/League of Legends/"))
				{
					//todo typing in lol path
					//miscReq = "League isnt installed in default path");
				}
				else
				{
					LaunchOldClient();
				}
			}
			ImGui::Columns(1);

			ImGui::Separator();

			static char username[50];
			ImGui::Text("Username:");
			ImGui::InputText("##inputUsername", username, IM_ARRAYSIZE(username));

			static char password[50];
			ImGui::Text("Password:");
			ImGui::InputText("##inputPassword", password, IM_ARRAYSIZE(password), ImGuiInputTextFlags_Password);

			if (ImGui::Button("Login"))
			{
				HTTP http;
				std::string LoginHeader = "Host: 127.0.0.1:" + std::to_string(loginPort) + "\n" +
					"Connection: keep-alive" + "\n" +
					"Authorization: Basic " + loginToken + "\n" +
					"Accept: application/json" + "\n" +
					"Content-Type: application/json" + "\n" +
					"Origin: https://127.0.0.1:" + std::to_string(loginPort) + "\n" +
					"User-Agent: Mozilla/5.0 (Windows NT 6.2; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) LeagueOfLegendsClient/11.3.356.7268 (CEF 74) Safari/537.36" + "\n" +
					"Referer: https://127.0.0.1:" + std::to_string(loginPort) + "/index.html" + "\n" +
					"Accept-Encoding: gzip, deflate, br" + "\n" +
					"Accept-Language: en-US,en;q=0.8";

				std::string loginBody = R"({"username":")" + std::string(username) + R"(","password":")" + std::string(password) + R"(","persistLogin":false})";
				closedReq = http.Request("PUT", "https://127.0.0.1/rso-auth/v1/session/credentials", loginBody, LoginHeader, "", "", loginPort);
			}
			ImGui::SameLine();
			if (ImGui::Button("Save"))
			{
				std::ofstream accFile;
				accFile.open("accounts.txt", std::ios_base::app);
				accFile << username << ":" << password << std::endl;

				accFile.close();
			}

			ImGui::Separator();

			std::fstream accFile("accounts.txt");
			std::vector<std::string> vAccounts;
			std::string tempAcc;
			while (accFile >> tempAcc)
			{
				vAccounts.emplace_back(tempAcc);
			}
			for (std::string& acc : vAccounts)
			{
				std::string username = acc.substr(0, acc.find(":"));
				std::string password = acc.substr(acc.find(":") + 1);
				if (ImGui::Button(username.c_str()))
				{
					HTTP http;
					std::string LoginHeader = "Host: 127.0.0.1:" + std::to_string(loginPort) + "\n" +
						"Connection: keep-alive" + "\n" +
						"Authorization: Basic " + loginToken + "\n" +
						"Accept: application/json" + "\n" +
						"Content-Type: application/json" + "\n" +
						"Origin: https://127.0.0.1:" + std::to_string(loginPort) + "\n" +
						"User-Agent: Mozilla/5.0 (Windows NT 6.2; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) LeagueOfLegendsClient/11.3.356.7268 (CEF 74) Safari/537.36" + "\n" +
						"Referer: https://127.0.0.1:" + std::to_string(loginPort) + "/index.html" + "\n" +
						"Accept-Encoding: gzip, deflate, br" + "\n" +
						"Accept-Language: en-US,en;q=0.8";

					std::string loginBody = R"({"username":")" + std::string(username) + R"(","password":")" + std::string(password) + R"(","persistLogin":false})";
					closedReq = http.Request("PUT", "https://127.0.0.1/rso-auth/v1/session/credentials", loginBody, LoginHeader, "", "", loginPort);
				}
				ImGui::SameLine();
				std::string deleteButton = "Delete##" + acc;
				if (ImGui::Button(deleteButton.c_str()))
				{
					acc = "";
					std::ofstream accFile1;
					accFile1.open("accounts.txt");
					for (std::string acc1 : vAccounts)
					{
						std::string username1 = acc1.substr(0, acc1.find(":"));
						std::string password1 = acc1.substr(acc1.find(":") + 1);
						if (acc1 != "")
							accFile1 << username1 << ":" << password1 << std::endl;
					}
					accFile1.close();
				}
			}
			accFile.close();

			ImGui::TextWrapped(closedReq.c_str());
		}

		/*
		POST /lol-honor-v2/v1/honor-player/
		{"gameId":5090113555,"honorCategory":"HEART","summonerId":2162807374746240}

		*/

		ImGui::EndTabBar();
	}

	//ImGui::ShowDemoWindow();

	ImGui::End();

	std::chrono::duration<float, std::milli> timeDuration = std::chrono::high_resolution_clock::now() - timeBefore;
	processTimeMs = timeDuration.count();

	return 1;
}

void Direct3D9Render::Shutdown()
{
	CleanupRenderTarget();
	if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

bool Direct3D9Render::CreateRenderTarget()
{
	ID3D11Resource* pBackBuffer;
	if (S_OK != g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)))
		return false;
	if (S_OK != g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pd3dRenderTargetView))
		return false;
	pBackBuffer->Release();
	return true;
}

void Direct3D9Render::CleanupRenderTarget()
{
	if (g_pd3dRenderTargetView) { g_pd3dRenderTargetView->Release(); g_pd3dRenderTargetView = NULL; }
}

void Direct3D9Render::Renderimgui(HWND hWnd)
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	// Setup Dear ImGui style
	MenuInit();

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
}

void Direct3D9Render::MenuInit()
{
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;

	ImGuiStyle& style = ImGui::GetStyle();

	//Main
	style.WindowPadding = ImVec2(4.f, 4.f);
	style.FramePadding = ImVec2(3.f, 3.f);
	style.ItemSpacing = ImVec2(5.f, 5.f);
	style.ItemInnerSpacing = ImVec2(5.f, 5.f);
	style.TouchExtraPadding = ImVec2(0.f, 0.f);
	style.ScrollbarSize = 15.f;
	style.GrabMinSize = 15.f;
	//Borders
	style.WindowBorderSize = 1.f;
	style.ChildBorderSize = 1.f;
	style.PopupBorderSize = 1.f;
	style.FrameBorderSize = 1.f;
	style.TabBorderSize = 1.f;
	//Rounding
	style.WindowRounding = 0.f;
	style.ChildRounding = 0.f;
	style.FrameRounding = 0.f;
	style.PopupRounding = 0.f;
	style.ScrollbarRounding = 0.f;
	style.GrabRounding = 0.f;
	style.LogSliderDeadzone = 5.f;
	style.TabRounding = 0.f;
	//Alignment
	style.WindowTitleAlign = ImVec2(0.f, 0.f);
	style.WindowMenuButtonPosition = 0;
	style.ColorButtonPosition = 1;
	style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
	style.SelectableTextAlign = ImVec2(0.5f, 0.5f);
	//AntiAliasing
	style.AntiAliasedLines = false;
	style.AntiAliasedLinesUseTex = false;
	style.AntiAliasedFill = false;

	ImVec4* colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.01f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
	colors[ImGuiCol_Border] = ImVec4(1.00f, 1.00f, 1.00f, 0.20f);
	colors[ImGuiCol_BorderShadow] = ImVec4(1.00f, 1.00f, 1.00f, 0.04f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.01f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.39f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.39f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.01f, 0.00f, 0.00f, 0.51f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.39f, 0.39f, 0.39f, 0.39f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 0.50f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.63f);
	colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.39f, 0.39f, 0.39f, 0.39f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.39f, 0.39f, 0.39f, 0.39f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.63f);
	colors[ImGuiCol_Header] = ImVec4(0.39f, 0.39f, 0.39f, 0.39f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	colors[ImGuiCol_HeaderActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.63f);
	colors[ImGuiCol_Separator] = ImVec4(0.39f, 0.39f, 0.39f, 0.39f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.63f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.39f, 0.39f, 0.39f, 0.39f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.63f);
	colors[ImGuiCol_Tab] = ImVec4(0.39f, 0.39f, 0.39f, 0.39f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	colors[ImGuiCol_TabActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.63f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}