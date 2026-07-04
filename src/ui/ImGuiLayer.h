#pragma once

#include "../core/Types.h"
#include "../skin/TextureCache.h"
#include "font_data.h"
#include <GLES2/gl2.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <string>
#include <vector>

namespace hf::UI {

inline bool   Initialized   = false;
inline ImFont* Font         = nullptr;
inline double LastFrameTime = 0.0;
inline int    EvictCounter  = 0;
inline float  PanelScale    = 1.0f;

inline std::string stripMCCodes(const std::string& text){
	std::string out;
	out.reserve(text.size());
	for(std::size_t i = 0; i < text.size();){
		if((unsigned char)text[i] == 0xC2 && i + 2 < text.size() && (unsigned char)text[i + 1] == 0xA7){
			i += 3;
			continue;
		}
		if((unsigned char)text[i] == 0xA7 && i + 1 < text.size()){
			i += 2;
			continue;
		}
		out += text[i++];
	}
	return out;
}

inline float drawShadowText(ImDrawList* dl, ImVec2 pos, const std::string& text, ImU32 color){
	ImU32 shadow = (color & 0xFF000000)
		| ((((color >> IM_COL32_R_SHIFT) & 0xFF) / 4) << IM_COL32_R_SHIFT)
		| ((((color >> IM_COL32_G_SHIFT) & 0xFF) / 4) << IM_COL32_G_SHIFT)
		| ((((color >> IM_COL32_B_SHIFT) & 0xFF) / 4) << IM_COL32_B_SHIFT);
	dl->AddText(ImVec2(pos.x + 1, pos.y + 1), shadow, text.c_str());
	dl->AddText(ImVec2(pos.x, pos.y), color, text.c_str());
	return ImGui::CalcTextSize(text.c_str()).x;
}

inline void init(){
	if(Initialized){ return; }
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.BackendPlatformName = "imgui_impl_headfetch";
	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 4.0f;
	style.WindowBorderSize = 0.0f;
	ImGui_ImplOpenGL3_Init("#version 100");
	ImFontConfig fontCfg;
	fontCfg.FontDataOwnedByAtlas = false;
	fontCfg.OversampleH = 2;
	fontCfg.OversampleV = 2;
	Font = io.Fonts->AddFontFromMemoryTTF(
		(void*)FontData::MinecraftTTF, (int)FontData::MinecraftTTFSize,
		19.0f, &fontCfg);
	if(!Font){ Font = io.Fonts->AddFontDefault(); }
	Initialized = true;
}

inline void beginFrame(float w, float h){
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(w, h);
	io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	double now = (double)ts.tv_sec + ts.tv_nsec / 1e9;
	io.DeltaTime = LastFrameTime > 0.0 ? (float)(now - LastFrameTime) : (1.0f / 60.0f);
	LastFrameTime = now;
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();
}

inline void endFrame(){
	ImGui::Render();
	GLint vp[4];
	glGetIntegerv(GL_VIEWPORT, vp);
	glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	glViewport(vp[0], vp[1], vp[2], vp[3]);
}

// Android has no reliable "hold Tab" gesture, so a persistent tap-to-toggle
// button replaces it. Always drawn while the HUD is on screen (regardless of
// whether the panel is open), and its screen-space rect is published to
// State::ToggleRect so the touch callback registered in
// hooks/HookManager.cpp can hit-test raw MotionEvent coordinates against it.
inline void drawToggleButton(float sw, float sh, float scale){
	const float size = 34.0f * scale;
	const float margin = 10.0f * scale;
	ImVec2 pos(sw - size - margin, margin);
	{
		std::lock_guard<std::mutex> lock(State::ToggleRectMutex);
		State::ToggleRect = { pos.x, pos.y, pos.x + size, pos.y + size, true };
	}
	bool open = State::panelVisible();
	ImDrawList* dl = ImGui::GetForegroundDrawList();
	ImU32 bg = open ? IM_COL32(70, 140, 255, 200) : IM_COL32(20, 20, 24, 160);
	dl->AddRectFilled(pos, ImVec2(pos.x + size, pos.y + size), bg, 6.0f * scale);
	if(Font){ ImGui::PushFont(Font); }
	const char* label = "P";
	ImVec2 textSize = ImGui::CalcTextSize(label);
	ImVec2 textPos(pos.x + (size - textSize.x) * 0.5f, pos.y + (size - textSize.y) * 0.5f);
	drawShadowText(dl, textPos, label, IM_COL32(255, 255, 255, 255));
	if(Font){ ImGui::PopFont(); }
}

inline void drawList(const std::vector<PlayerInfo>& players, float sw, float sh){
	if(players.empty()){ return; }
	float scale = (sh / 1080.0f) * PanelScale;
	if(scale < 0.5f){ scale = 0.5f; }
	if(scale > 1.5f){ scale = 1.5f; }
	const float HeadSize  = 20.0f * scale;
	const float RowHeight = 22.0f * scale;
	const float RowPadX   = 7.0f  * scale;
	const float NameGap   = 7.0f  * scale;
	const float ColGap    = 18.0f * scale;
	const float Padding   = 7.0f  * scale;
	const float FooterH   = 26.0f * scale;
	if(Font){ ImGui::PushFont(Font); }
	int columns = 1;
	if(players.size() > 20){ columns = 2; }
	if(players.size() > 40){ columns = 3; }
	if(players.size() > 60){ columns = 4; }
	int rows = ((int)players.size() + columns - 1) / columns;
	std::vector<float> colWidths(columns, 0.0f);
	for(std::size_t i = 0; i < players.size(); ++i){
		int col = (int)(i / rows);
		float w = ImGui::CalcTextSize(stripMCCodes(players[i].name).c_str()).x;
		colWidths[col] = std::max(colWidths[col], w);
	}
	for(auto& w : colWidths){ w += RowPadX * 2.0f + HeadSize + NameGap; }
	float gridWidth = 0;
	for(float w : colWidths){ gridWidth += w; }
	gridWidth += (columns - 1) * ColGap;
	char footer[64];
	std::snprintf(footer, sizeof(footer), "%zu players online", players.size());
	float footerWidth = ImGui::CalcTextSize(footer).x;
	float panelW = std::max(gridWidth, footerWidth) + Padding * 2.0f;
	float panelMaxH = sh * 0.85f;
	float gridH = rows * RowHeight + 6.0f * scale;
	float panelH = std::min(gridH, panelMaxH - Padding * 2.0f - FooterH) + Padding * 2.0f + FooterH;
	ImGui::SetNextWindowPos(ImVec2(sw * 0.5f, sh * 0.03f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
	ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_Always);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.03f, 0.03f, 0.04f, 0.26f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Padding, Padding));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
		| ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
	if(ImGui::Begin("##HeadList", nullptr, flags)){
		float scrollH = ImGui::GetContentRegionAvail().y - FooterH;
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		ImGui::BeginChild("##Grid", ImVec2(gridWidth, scrollH), false,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs);
		ImDrawList* dl = ImGui::GetWindowDrawList();
		for(int col = 0; col < columns; ++col){
			if(col > 0){ ImGui::SameLine(0, ColGap); }
			ImGui::BeginGroup();
			float cellW = colWidths[col];
			for(int row = 0; row < rows; ++row){
				std::size_t i = (std::size_t)col * rows + row;
				if(i >= players.size()){ break; }
				ImVec2 rowMin = ImGui::GetCursorScreenPos();
				ImVec2 rowMax(rowMin.x + cellW, rowMin.y + RowHeight - 2.0f * scale);
				dl->AddRectFilled(rowMin, rowMax, IM_COL32(255, 255, 255, 12), 0);
				GLuint tex = Head::find(players[i].uuid);
				ImVec2 headMin(rowMin.x + RowPadX, rowMin.y + (RowHeight - 2.0f * scale - HeadSize) * 0.5f);
				ImVec2 headMax(headMin.x + HeadSize, headMin.y + HeadSize);
				if(tex){ dl->AddImage((ImTextureID)(intptr_t)tex, headMin, headMax); }
				float textH = ImGui::GetFontSize();
				ImVec2 namePos(headMax.x + NameGap, rowMin.y + (RowHeight - 2.0f * scale - textH) * 0.5f);
				drawShadowText(dl, namePos, players[i].name, IM_COL32(255, 255, 255, 255));
				ImGui::SetCursorScreenPos(rowMin);
				ImGui::Dummy(ImVec2(cellW, RowHeight));
			}
			ImGui::EndGroup();
		}
		ImGui::EndChild();
		ImGui::PopStyleVar();
		ImVec2 footerPos(
			ImGui::GetWindowPos().x + (ImGui::GetWindowSize().x - footerWidth) * 0.5f,
			ImGui::GetWindowPos().y + ImGui::GetWindowSize().y - Padding - ImGui::GetFontSize());
		drawShadowText(ImGui::GetWindowDrawList(), footerPos, footer, IM_COL32(191, 191, 191, 255));
	}
	ImGui::End();
	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor();
	if(Font){ ImGui::PopFont(); }
}

} // namespace hf::UI
