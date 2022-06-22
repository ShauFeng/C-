#include "includes.h"

#ifdef _WIN64
#define GWL_WNDPROC GWLP_WNDPROC
#endif

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

EndScene oEndScene = NULL;
WNDPROC oWndProc;
static HWND window = NULL;

#include "csgo.hpp"
using namespace hazedumper::netvars;
using namespace hazedumper::signatures;

void InitImGui(LPDIRECT3DDEVICE9 pDevice)
{
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX9_Init(pDevice);
}

bool init = false;


struct Vector4 
{
	float x, y, z, w; 
};
struct Vec3
{
	float x, y, z;
	Vec3 operator+(Vec3 d)
	{
		return { x + d.x, y + d.y, z + d.z };
	}
	Vec3 operator-(Vec3 d)
	{
		return { x - d.x, y - d.y, z - d.z };
	}
	Vec3 operator*(float d)
	{
		return { x * d, y * d, z * d };
	}
	void normalize()
	{
		while (y < -180)
		{
			y = 360;
		}
		while (y > 180)
		{
			y = -360;
		}
		while (x > 89)
		{
			x = 89;
		}
		while (x < -89)
		{
			x = -89;
		}
	}
};
struct Vector2 
{
	float x, y;
};
struct ColorStr
{
	int r, g, b, a;
};
struct BoneMatrix_t
{
	byte pad1[12];
	float x;
	byte pad2[12];
	float y;
	byte pad3[12];
	float z;
};

bool show_main = true;

int screenX = GetSystemMetrics(SM_CXSCREEN);
int screenY = GetSystemMetrics(SM_CXSCREEN);

bool esp = false;

bool boxesp = false;
bool tracers = false;
int tracerBone = 9;
float boxwidth = 0.5;
ColorStr TracerColor;
ColorStr BoxColor;

bool glow = false;
ImColor EnemyColor;
ImColor TeamColor;
int boxThickness = 2;
int backingAlpha = 35;
bool drawBacking = true;
bool antialias_all = true;

bool bhop = false;
bool rcs = false;
float rcs_amount = 0;
bool triggerbot = false;
bool triggerRandomness = false;
int  triggerCustomDelay = 0;
bool tbDelay = false;
bool thirdperson = false;


bool customFOV = false;
int fov = 90;



DWORD clientMod;
DWORD engineMod;
DWORD localPlayer;
int* iShotsFired;
Vec3* viewAngles;
Vec3* aimRecoilPunch;
Vec3 oPunch{ 0, 0, 0 };

bool WorldToScreen(Vec3 pos, Vector2& screen, float matrix[16], int width, int height)
{
		Vector4 clipCoords;
		clipCoords.x = pos.x * matrix[0] + pos.y * matrix[1] + pos.z * matrix[2] + matrix[3];
		clipCoords.y = pos.x * matrix[4] + pos.y * matrix[5] + pos.z * matrix[6] + matrix[7];
		clipCoords.z = pos.x * matrix[8] + pos.y * matrix[9] + pos.z * matrix[10] + matrix[11];
		clipCoords.w = pos.x * matrix[12] + pos.y * matrix[13] + pos.z * matrix[14] + matrix[15];

		if (clipCoords.w < 0.1f)
		{
			return false;
		}

		Vec3 NDC;
		NDC.x = clipCoords.x / clipCoords.w;
		NDC.y = clipCoords.y / clipCoords.w;
		NDC.z = clipCoords.z / clipCoords.w;

		screen.x = (width / 2 * NDC.x) + (NDC.x + width / 2);
		screen.y = -(height / 2 * NDC.y) + (NDC.y + height / 2);

		return true;
}
Vector2 GetBonePosition(uintptr_t Entity, int bone)
{
		uintptr_t BoneMatrix_Base = *(uintptr_t*)(Entity + m_dwBoneMatrix);
		BoneMatrix_t Bone = *(BoneMatrix_t*)(BoneMatrix_Base + sizeof(Bone) * bone);
		Vec3 Location = { Bone.x, Bone.y, Bone.z };
		Vector2 ScreenCoords;
		
		float vMatrix[16];
		DWORD GameModule = (DWORD)GetModuleHandle("client.dll");
		memcpy(&vMatrix, (PBYTE*)(GameModule + dwViewMatrix), sizeof(vMatrix));
		if (WorldToScreen(Location, ScreenCoords, vMatrix, screenX, screenY))
		{
				return ScreenCoords;
		}
		return { 0, 0 };
}


void HackInit()
{
	clientMod = (uintptr_t)GetModuleHandle("client.dll");
	engineMod = (uintptr_t)GetModuleHandle("engine.dll");

	localPlayer = *(uintptr_t*)(clientMod + dwLocalPlayer);

	iShotsFired = (int*)(localPlayer + m_iShotsFired);

	viewAngles = (Vec3*)(*(uintptr_t*)(engineMod + dwClientState) + dwClientState_ViewAngles);

	aimRecoilPunch = (Vec3*)(localPlayer + m_aimPunchAngle);

	TeamColor.Value.w = 1;
	EnemyColor.Value.w = 1;

}

LPDIRECT3DDEVICE9 externalDevice;
void DrawFilledRect(int x, int y, int w, int h, D3DCOLOR col)
{
	D3DRECT rect = { x, y, x + w, y + h };
	externalDevice->Clear(1, &rect, D3DCLEAR_TARGET, col, 0, 0);
}
void DrawLine(int x1, int y1, int x2, int y2, int thickness, bool antialias, D3DCOLOR col)
{
	ID3DXLine* lineL;
	D3DXCreateLine(externalDevice, &lineL);

	D3DXVECTOR2 Line[2];
	Line[0] = D3DXVECTOR2(x1, y1);
	Line[1] = D3DXVECTOR2(x2, y2);

	lineL->SetWidth(thickness);
	lineL->SetAntialias(antialias);
	lineL->Draw(Line, 2, col);
	lineL->Release();
}
void DrawESPBox(int x, int y, int w, int h, int thickness, bool antialias, D3DCOLOR col)
{
		DrawLine(x, y,  x+w, y, thickness, antialias, col);
		DrawLine(x, y+h, x + w, y+h, thickness, antialias, col);
		DrawLine(x, y, x , y+h, thickness, antialias, col);
		DrawLine(x+w, y, x + w, y+h, thickness, antialias, col);
}


long __stdcall hkEndScene(LPDIRECT3DDEVICE9 pDevice)
{
	if (!init)
	{
		HackInit();
		InitImGui(pDevice);
		init = true;
	}

	if (GetAsyncKeyState(VK_INSERT) & 1)
	{
		show_main = !show_main;
	}

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();


	if (show_main)
	{
		ImGui::Begin("Test");
		ImGui::Text("ESP Settings");
		ImGui::Checkbox("ESP", &esp);
		if (esp) 
		{
			ImGui::Checkbox("Box ESP", &boxesp);
			if (boxesp)
			{
				ImGui::Text("Colors");
				ImGui::SliderInt("R", &BoxColor.r, 0, 255);
				ImGui::SliderInt("G", &BoxColor.g, 0, 255);
				ImGui::SliderInt("B", &BoxColor.b, 0, 255);
				ImGui::SliderInt("A", &BoxColor.a, 0, 255);
			}
			ImGui::Separator();
			ImGui::Checkbox("Tracers", &tracers);
			if (tracers)
			{
				ImGui::SliderFloat("Box Width", &boxwidth, 0.00f, 1.00f);
				ImGui::SliderInt("Box Thickness", &boxThickness, 1, 10);
				ImGui::Text("Colors");
				ImGui::SliderInt("R", &BoxColor.r, 0, 255);
				ImGui::SliderInt("G", &BoxColor.g, 0, 255);
				ImGui::SliderInt("B", &BoxColor.b, 0, 255);
				ImGui::SliderInt("A", &BoxColor.a, 0, 255);
				ImGui::Spacing();
				ImGui::SliderInt("Target Bone", &tracerBone, 0, 100);
			}
		}

		/*if (esp)
		{
			ImGui::Checkbox("Glow ESP", &glow);
			if (glow)
			{
				ImGui::ColorPicker4("Enemy Glow Color", (float*)&EnemyColor);
				ImGui::ColorPicker4("Friendly G	low Color", (float*)&TeamColor);
			}
		}*/

		ImGui::Separator();
		ImGui::Checkbox("DirectX Antialiasing", &antialias_all);


		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Checkbox("Bhop", &bhop);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Text("RCS Settings");
		ImGui::Checkbox("No Recoil", &rcs);
		if (rcs)
		{
			ImGui::SliderFloat("No-Recoil Amount", &rcs_amount, 0, 1);
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Checkbox("Custom FOV", &customFOV);
		if (customFOV)
		{
			ImGui::SliderInt("FOV", &fov, 30, 179);
		}

		if (boxesp)
		{
			DWORD GameModule = (DWORD)GetModuleHandle("client.dll");
			DWORD LocalPlayer = *(DWORD*)(GameModule + dwLocalPlayer);
			if (!LocalPlayer)
			{
				while (!LocalPlayer)
				{
					LocalPlayer = *(DWORD*)(GameModule + dwLocalPlayer);
				}
			}
			Vector2 ScreenPosition;
			Vector2 HeadPosition;
			for (int i = 0; i < 32; i++)
			{
				uintptr_t Entity = *(uintptr_t*)(GameModule + dwEntityList + (i * 0x10));
				if (Entity != NULL)
				{
					Vec3 EntityLocation = *(Vec3*)(Entity + m_vecOrigin);
					float vMatrix[16];
					DWORD GameModule = (DWORD)GetModuleHandle("client.dll");
					memcpy(&vMatrix, (PBYTE*)(GameModule + dwViewMatrix), sizeof(vMatrix));
					if (WorldToScreen(EntityLocation, ScreenPosition, vMatrix, screenX, screenY))
					{
						uintptr_t BoneMatrix_Base = *(uintptr_t*)(Entity + m_dwBoneMatrix);
						BoneMatrix_t Bone = *(BoneMatrix_t*)(BoneMatrix_Base + sizeof(Bone) * 9);
						Vec3 Location = { Bone.x, Bone.y, Bone.z + 10 };
						Vector2 ScreenCoords;

						float vMatrix[16];
						DWORD GameModule = (DWORD)GetModuleHandle("client.dll");
						memcpy(&vMatrix, (PBYTE*)(GameModule + dwViewMatrix), sizeof(vMatrix));
						if (WorldToScreen(Location, ScreenCoords, vMatrix, screenX, screenY))
						{
							HeadPosition = ScreenCoords;
							DrawESPBox(
								ScreenPosition.x - (((ScreenPosition.y - HeadPosition.y) * boxwidth) / 2),
								HeadPosition.y,
								(ScreenPosition.y - HeadPosition.y) * boxwidth,
								ScreenPosition.y - HeadPosition.y,
								boxThickness,
								antialias_all,
								D3DCOLOR_ARGB(BoxColor.a, BoxColor.r, BoxColor.g, BoxColor.b)
							);
						}
					}
				}
			}
		}


		/*ImGui::Spacing();
		ImGui::Separator();
		ImGui::Checkbox("Thridperson", &thirdperson);*/

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Text("Triggerbot Settings");
		ImGui::Checkbox("Enabled", &triggerbot);
		if (triggerbot)
		{
				ImGui::Spacing();
				ImGui::Checkbox("Enable Custom Delay", &tbDelay);
				if (tbDelay)
				{
						ImGui::SliderInt("Custom Delay (ms)", &triggerCustomDelay, 0, 250);
				}
				else
				{
					ImGui::Checkbox("Random Delay", &triggerRandomness);
				}
		}

		ImGui::End();
	}

	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

	return oEndScene(pDevice);
}

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	if (true && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;

	return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam)
{
	DWORD wndProcId;
	GetWindowThreadProcessId(handle, &wndProcId);

	if (GetCurrentProcessId() != wndProcId)
		return TRUE; // skip to next window

	window = handle;
	return FALSE; // window found abort search
}

HWND GetProcessWindow()
{
	window = NULL;
	EnumWindows(EnumWindowsCallback, NULL);
	return window;
}

DWORD WINAPI MainThread(LPVOID lpReserved)
{
	bool attached = false;
	do
	{
		if (kiero::init(kiero::RenderType::D3D9) == kiero::Status::Success)
		{
			kiero::bind(42, (void**)& oEndScene, hkEndScene);
			do
				window = GetProcessWindow();
			while (window == NULL);
			oWndProc = (WNDPROC)SetWindowLongPtr(window, GWL_WNDPROC, (LONG_PTR)WndProc);
			attached = true;
		}
	} while (!attached);
	return TRUE;
}

DWORD WINAPI RCSThread(LPVOID lp)
{
	while (true)
	{
		if (rcs )
		{
			Vec3 punchAngle = *aimRecoilPunch * (rcs_amount * 2);
			if (*iShotsFired > 1 && GetAsyncKeyState(VK_LBUTTON))
			{
				Vec3 newAngle = *viewAngles + oPunch - punchAngle;
				newAngle.normalize();

				*viewAngles = newAngle;
			}
			oPunch = punchAngle;
		}
	}
}

DWORD WINAPI TriggerThread(LPVOID lp)
{
		DWORD gameMod = (DWORD)GetModuleHandle("client.dll");
		while (true)
		{
				if (triggerbot)
				{
						DWORD localPlayer = *(DWORD*)(gameMod + dwLocalPlayer);
						int crosshair = *(int*)(localPlayer + m_iCrosshairId);
						int localTeam = *(int*)(localPlayer + m_iTeamNum);
						if (crosshair != 0 && crosshair < 64)
						{
								uintptr_t entity = *(uintptr_t*)(gameMod + dwEntityList + (crosshair - 1) * 0x10);
								int eTeam = *(int*)(entity + m_iTeamNum);
								int eHealth = *(int*)(entity + m_iHealth);

								if (eTeam != localTeam && eHealth > 0 && eHealth < 101)
								{
										if(triggerRandomness)
										{
												Sleep((rand() * 250) + 50);
												*(int*)(gameMod + dwForceAttack) = 5;
												Sleep(20);
												*(int*)(gameMod + dwForceAttack) = 4;
										}
										else
										{
												if(tbDelay)
												{
													Sleep(triggerCustomDelay);
													*(int*)(gameMod + dwForceAttack) = 5;
													Sleep(20);
													*(int*)(gameMod + dwForceAttack) = 4;
												}
												else
												{
													*(int*)(gameMod + dwForceAttack) = 5;
													Sleep(20);
													*(int*)(gameMod + dwForceAttack) = 4;
												}
										}
								}
						}
				}
				Sleep(2);
		}
}

DWORD WINAPI BhopThread(LPVOID lp)
{
	DWORD gameModule = (DWORD)GetModuleHandle("client.dll");
	DWORD localPlayer = *(DWORD*)(gameModule + dwLocalPlayer);
	while (localPlayer == NULL)
	{
		localPlayer = *(DWORD*)(gameModule + dwLocalPlayer);
	}
	while (true)
	{
		if (bhop)
		{
			DWORD flag = *(BYTE*)(localPlayer + m_fFlags);
			if (GetAsyncKeyState(VK_SPACE) && flag & (1 << 0))
			{
				*(DWORD*)(gameModule + dwForceJump) = 6;
			}
		}
	}
}

DWORD WINAPI FOVThread(LPVOID lp)
{
		DWORD gameMod = (DWORD) GetModuleHandle("client.dll");
		DWORD localPlayer = *(DWORD*)(gameMod + dwLocalPlayer);
		if (localPlayer == NULL)
		{
				while (localPlayer == NULL)
				{
						localPlayer = *(DWORD*)(gameMod + dwLocalPlayer);
				}
		}
		while (true)
		{
				if (localPlayer != NULL)
				{
						if(customFOV)
						{
								*(int*)(localPlayer + m_iDefaultFOV) = fov;
						}
				}
		}
}

DWORD WINAPI  ThirdPersonThread(LPVOID lp)
{
	DWORD gameMod = (DWORD)GetModuleHandle("client.dll");
	DWORD localPlayer = NULL;
	while (localPlayer == NULL)
	{
		localPlayer = *(DWORD*)(gameMod + dwLocalPlayer);
	}
	while (true)
	{
		if (thirdperson)
		{
			*(int*)(localPlayer + m_iObserverMode) = 1;
		}
		else
		{
			*(int*)(localPlayer + m_iObserverMode) = 0;
		}
	}
}

DWORD WINAPI GlowThread(LPVOID lp)
{
	DWORD GameModule = (uintptr_t)GetModuleHandle("client.dll");
	uintptr_t GlowObj = *(uintptr_t*)(GameModule + dwGlowObjectManager);

	while (true)
	{
		DWORD localPlayer = *(uintptr_t*)(GameModule + dwLocalPlayer);
		if (glow)
		{
			int LocalTeam = *(int*)(dwLocalPlayer + m_iTeamNum);
			for (int i = 0; i < 64; i++)
			{
				uintptr_t Entity = *(uintptr_t*)(GameModule + dwEntityList + i * 0X10);
				if (Entity != NULL)
				{
					int EntityTeam = *(int*)(Entity + m_iTeamNum);
					int GlowIndex = *(int*)(Entity + m_iGlowIndex);

					if (LocalTeam != EntityTeam)
					{
						*(float*)(GlowObj + ((GlowIndex * 0x38) + 0x4)) = EnemyColor.Value.x;
						*(float*)(GlowObj + ((GlowIndex * 0x38) + 0x8)) = EnemyColor.Value.y;
						*(float*)(GlowObj + ((GlowIndex * 0x38) + 0xC)) = EnemyColor.Value.z;
						*(float*)(GlowObj + ((GlowIndex * 0x38) + 0x10)) = EnemyColor.Value.w;
					}
					else
					{
						*(float*)(GlowObj + ((GlowIndex * 0x38) + 0x4)) = TeamColor.Value.x;
						*(float*)(GlowObj + ((GlowIndex * 0x38) + 0x8)) = TeamColor.Value.y;
						*(float*)(GlowObj + ((GlowIndex * 0x38) + 0xC)) = TeamColor.Value.z;
						*(float*)(GlowObj + ((GlowIndex * 0x38) + 0x10)) = TeamColor.Value.w;
					}
					*(bool*)(GlowObj + ((GlowIndex * 0x38) + 0x24)) = true;
					*(bool*)(GlowObj + ((GlowIndex * 0x38) + 0x25)) = false;
				}
			}
		}
	}
}

BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hMod);
		CreateThread(nullptr, 0, MainThread, hMod, 0, nullptr);
		CreateThread(nullptr, 0, BhopThread, hMod, 0, nullptr);
		CreateThread(nullptr, 0, RCSThread, hMod, 0, nullptr);
		CreateThread(nullptr, 0,FOVThread, hMod, 0, nullptr);
		CreateThread(nullptr, 0, TriggerThread, hMod, 0, nullptr);	
		CreateThread(nullptr, 0, ThirdPersonThread, hMod, 0, nullptr);
		CreateThread(nullptr, 0, GlowThread, hMod, 0, nullptr);
		break;
	case DLL_PROCESS_DETACH:
		kiero::shutdown();
		break;
	}
	return TRUE;
}
	