#include <Windows.h>
#include <Psapi.h>
#include <Xinput.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <fstream>
#include <string>
#include <sstream>
#include <random>
#include <chrono>

#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "Xinput.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

#define ADDR_VehicleState 0x007BC51C
#define CONFIG_FILE "ScarfaceEX_Configuration.ini"
#define README_FILE "ScarfaceEX_Instructions.txt"
//std::ofstream debugFile("ScarfaceEX_Debug.txt", std::ios::app);

typedef void* CharacterObject;
typedef void (__cdecl *Run_Script_Function)(const char*, int, int, int, int);
typedef const char* (__cdecl *CVM_Get_Main_Character_Package_Function)();
typedef int (__cdecl *Global_Sound_Get_Current_Function)();
typedef int (__cdecl *DB_Get_Reputation_Level_Function)();
typedef int (__cdecl *MGM_Get_Visibility_Points_Function)();
typedef int (__thiscall *PlayAnimationFunc)(CharacterObject*, unsigned int*, int, int);

struct Dodges_Phone_Calls_Blocker_Struct
{
	unsigned long blockedTimer;
	bool blockedFlag;
	
	void Reset()
	{
		blockedTimer = GetTickCount();
		blockedFlag = false;
	}
}
DPCBS_Struct;

// Script Loader for Radical Entertainment Script Interpreter
std::string Load_Script(const char* fileName)
{
	std::ifstream file(fileName);
	
	if (!file)
		return "return;";
	
	std::ostringstream scriptBuffer;
	scriptBuffer << file.rdbuf();
	std::string result = scriptBuffer.str();
	
	return result;
}

// Loaded Scripts
std::string External_Scripts = Load_Script("ScarfaceEX_External_Scripts.txt");

// Characters Speaking Inside Vehicles Tracker
static const int MAX_SPEAKERS = 64;

struct Characters_Speaking_Inside_Vehicles_Structure
{
	bool lastVocalMS;
		 
	void Reset()
	{
		lastVocalMS = false;
	}
}
CSIVS_Struct[MAX_SPEAKERS];

// Character Switching Weapons Tracker
static const int MAX_CHARACTERS = 64;

struct Characters_Changing_Weapons_Structure
{
	int lastWeapon;
	
	void Reset()
	{
		lastWeapon = 0;
	}
}
CHWS_Struct[MAX_CHARACTERS];

// Random Values' Start-Up Seed Generation
std::mt19937& Get_Random_Seed() 
{
    thread_local std::mt19937 randomSeed([]() 
	{
        unsigned long currentSeed = static_cast<unsigned long>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        
		return std::mt19937(currentSeed);
    }());
    
	return randomSeed;
}

// Random Values Generation
unsigned long Random_Quote_Delays()
{
	unsigned long randomQuoteDelays = std::uniform_int_distribution<unsigned long>(5555UL, 7777UL)(Get_Random_Seed());
	
	return randomQuoteDelays;
}

// 1st Person Camera Mode
struct Vector 
{
    float X, 
		  Y, 
		  Z;
    
	Vector() : X(0), Y(0), Z(0) 
	{
	}
    
	Vector(float x, float y, float z) : X(x), Y(y), Z(z) 
	{
	}
    
	Vector operator+(const Vector& other) const 
	{
        return Vector(X + other.X, Y + other.Y, Z + other.Z);
    }
    
	Vector operator*(float scalar) const 
	{
        return Vector(X * scalar, Y * scalar, Z * scalar);
    }
};

struct Matrix 
{
    float M[4][4];
    
    Vector GetForward() const 
	{ 
		return Vector(M[2][0], M[2][1], M[2][2]); 
	}
    
	Vector GetRight() const 
	{ 
		return Vector(M[0][0], M[0][1], M[0][2]); 
	}
    
	Vector GetUp() const 
	{ 
		return Vector(M[1][0], M[1][1], M[1][2]); 
	}
};

// 1st Person Camera Configuration
struct FPSCameraConfig 
{
    bool enabled, 
		 footOnly;
	
	int bonePosition;
	
	// Y-Offset \ X-Offset \ Z-Offset respectively
    float heightAdjust, 
		  sideAdjust, 
		  distanceAdjust; 
} 
g_FPSConfig;

// Dodges & Evades Configuration
struct Config 
{
    // Keyboard & Mouse Bindings
	int frontKey, 
		leftKey, 
		rightKey, 
		backKey, 
		leftPeekShootKey, 
		rightPeekShootKey,
		copBribeKey,
		gangBribeKey,
		backupKey; 								 			   
    
	// Microsoft Xinput Controller Bindings
	WORD frontButton, 
		 leftButton, 
		 rightButton, 
		 backButton, 
		 leftPeekShootButton, 
		 rightPeekShootButton,
		 copBribeButton,
		 gangBribeButton,
		 backupButton;

	// Direct-Input Controller Bindings
    int diFrontButton, 
		diLeftButton, 
		diRightButton, 
		diBackButton, 
		diLeftPeekShootButton, 
		diRightPeekShootButton,
		diCopBribeButton,
		diGangBribeButton,
		diBackupButton; 			   
	
	// Animation Names
    std::string frontAnimation, 
				leftAnimation, 
				rightAnimation, 
				backAnimation, 
				leftPeekShootAnimation, 
				rightPeekShootAnimation, 
				waterVehicleDriver, 
				waterVehiclePassenger, 
				landVehicleDriver, 
				landVehicleDriverReverse, 
				waterVehicleIdles, 
				landVehicleIdles, 
				landVehicleDamageDriver, 
				landVehicleDamageDriverReverse, 
				waterVehicleDamageDriver, 
				landVehicleDamagePassenger, 
				waterVehicleDamagePassenger, 
				Damage_50_Calibers, 
				HealthRecovery,
				SwitchingWeapons,
				landVehicleDriverTalking,
				landVehicleDriverReverseTalking,
				landVehiclePassengerTalking,
				waterVehicleDriverTalking,
				waterVehiclePassengerTalking,
				copBribeAnimation,
				gangBribeAnimation,
				backupAnimation;
    
	// Press Mode
	int pressMode;

	// Multi-Press Timing Window \ Cooldown Between Dodges [Milliseconds]
    unsigned long pressWindow, 
				  cooldown;																					   
} 
g_Config;

// Vehicle Animation State Tracking
struct VehicleAnimState
{
    bool idle, 
		 active, 
		 wasInVehicle, 
		 wasInputActive;
	
	unsigned long entryTime, 
				  inputReleasedAt;
	
    void Reset()
    {
        idle = false;
        active = false;
		wasInVehicle = false;
		wasInputActive = false;
        entryTime = 0;
        inputReleasedAt = 0;
    }
}
g_VehicleAnim;

// NPC Vehicle Animation Tracking
unsigned long g_LastNPCAnimCheck = 0;
static const unsigned long NPC_ANIM_INTERVAL = 732UL;
static const int MAX_NPCS = 64; 

// Pattern Scanning
uintptr_t FindPattern(const char* pattern, const char* mask) 
{
    MODULEINFO modInfo;
    GetModuleInformation(GetCurrentProcess(), GetModuleHandle(NULL), &modInfo, sizeof(MODULEINFO));
    
	uintptr_t base = (uintptr_t)modInfo.lpBaseOfDll;
    size_t size = modInfo.SizeOfImage, 
		   patternLen = strlen(mask);
    
    for (size_t i = 0; i < size - patternLen; i++) 
	{
        bool found = true;
        
		for (size_t j = 0; j < patternLen; j++) 
		{
            if (mask[j] != '?' && pattern[j] != *(char*)(base + i + j)) 
			{
                found = false;
                break;
            }
        }
        
		if (found) 
			return base + i;
    }
    
	return 0;
}

void ReadCall(uintptr_t callAddr, uintptr_t* outAddr) 
{
    int offset = *(int*)(callAddr + 1);
    *outAddr = callAddr + 5 + offset;
}

// Hashing Function
unsigned int Hash(char* input) 
{
    if (!input) 
		return 0;
    
	char curChar = *input;
    
	if (!curChar) 
		return 0;
    
	int v4 = 0;
    
	do 
	{
        int v5 = (65599 * v4) & 0x7FFFFFFF, 
			temp = curChar;
        
		if (curChar < 'a') 
			temp = curChar + ' ';
        
		v4 = temp ^ v5;
        curChar = *(++input);
    } 
	while (*input);
    
	return v4 | 0x80000000;
}

// Current Locomotion Ring Type
int Current_Locomotion_Ring_Type(CharacterObject* character)
{
	int currentLocomotionRingType = 0;
	
	__try
	{
		currentLocomotionRingType = *(int*)((uintptr_t)character + 0x1B0 + 0xB0);
	}
	
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		currentLocomotionRingType = -1;
	}
	
	return currentLocomotionRingType;
}

// Characters' Weapon State Detector
int Get_Weapon_State(CharacterObject* npc)
{
	__try
	{
		int weaponState = *(int*)((uintptr_t)npc + 0x2AC + 0xC);
		return weaponState;
	}
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return 0; 
	}
} 

// Character's Driving Direction Detector
int Land_Vehicle_Reverse_Driving(CharacterObject* characters)
{
	__try
	{
		int reverseDirection = *(int*)((uintptr_t)characters + 0x2E8 + 0x8);
		return (reverseDirection == 3) ? 1 : 0;
	}
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return 0; 
	}
}

// Characters' 50 Calibers Weapon State Detector
int Get_50_Calibers(CharacterObject* npc)
{
	__try
	{
		int calibers50 = *(int*)((uintptr_t)npc + 0x2E8 + 0x8);
		return (calibers50 == 6) ? 1 : 0;
	}
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return 0; 
	}
}

// Vehicle Damage Animation Tracking
struct DamageAnimState
{
    unsigned long lastTimer;
	int lastHealth;  
    bool animPlayed,
		 damageTaken; 

    void Reset()
    {
        lastTimer = GetTickCount();
		lastHealth = -1; 
        animPlayed = false;
		damageTaken = false;
    }
};

static const int MAX_NPC_DAMAGE = 64;
DamageAnimState g_NPCDamageStates[MAX_NPC_DAMAGE];

// 50 Caliber Damage Animation Tracking
static const int MAX_NPC_50CAL = 64;
DamageAnimState g_NPC50CalStates[MAX_NPC_50CAL];

struct NPCAnimState
{
    float lastX, 
		  lastZ;
    
	bool idlePlayed, 
		 activePlayed;

    void Reset()
    {
		lastX = 0.0f;
        lastZ = 0.0f;
        idlePlayed = false;
        activePlayed = false;
    }
}
g_NPCStates[MAX_NPCS];

// Press Tracking Structure
struct PressTracker 
{
    unsigned long lastPressTime;
    int pressCount;
    
    void Reset() 
	{
        lastPressTime = 0;
        pressCount = 0;
    }
    
    bool CheckPress(unsigned long now, unsigned long window) 
	{
        if (now - lastPressTime > window)
            pressCount = 1;
        
		else
            pressCount++;
        
		lastPressTime = now;
        
		return true;
    }
    
    bool ShouldTrigger(int requiredPresses)
	{
        return pressCount >= requiredPresses;
	}
};

// Separate Bindings' Trackers
struct DirectionalTrackers 
{
    PressTracker front, 
				 left, 
				 right, 
				 back, 
				 leftPeekShoot, 
				 rightPeekShoot,
				 copBribe,
				 gangBribe,
				 backup;
} 
g_Trackers;

// Global Variables
bool External_Scripts_Initialised = false;
unsigned long g_LastDodgeTime = 0;
uintptr_t g_PlayerPointerAddress = 0, 
		  g_CVManagerAddress = 0;

PlayAnimationFunc g_PlayAnimationFunc = NULL;
CVM_Get_Main_Character_Package_Function CVM_Get_Main_Character_Package_Pointer = (CVM_Get_Main_Character_Package_Function)0x004f4d60;
Run_Script_Function Run_Script_Pointer = NULL;
uintptr_t Run_Script_Address = 0;
Global_Sound_Get_Current_Function Global_Sound_Get_Current_Pointer = (Global_Sound_Get_Current_Function)0x0049a420;
DB_Get_Reputation_Level_Function DB_Get_Reputation_Level_Pointer = (DB_Get_Reputation_Level_Function)0x00433cb0;
MGM_Get_Visibility_Points_Function MGM_Get_Visibility_Points_Pointer = (MGM_Get_Visibility_Points_Function)0x00642230; 

// Keyboard \ Mouse \ Microsoft Xinput Controller \ Direct-Input Controller States Tracker; Direct-Input Flag
bool g_KeyboardState[9] = {false, false, false, false, false, false, false, false, false},
	 g_ControllerState[9] = {false, false, false, false, false, false, false, false, false},
	 g_DirectInputState[9] = {false, false, false, false, false, false, false, false, false}, 
	 g_DirectInputEnabled = false;

// DirectInput Globals
LPDIRECTINPUT8 g_pDI = NULL;
LPDIRECTINPUTDEVICE8 g_pJoystick = NULL;

// Thread Control
volatile bool g_Running = true;
HANDLE g_InputThreadHandle = NULL;

// First Person Camera Globals
typedef void (__thiscall *SetPositionFunc)(void* camera, Vector* pos);
typedef Vector* (__thiscall *GetBonePositionFunc)(CharacterObject* character, int boneId);
SetPositionFunc g_OriginalSetPosition = NULL;
GetBonePositionFunc g_GetBonePosition = NULL;

// Visibility Points Tracker
int MGM_Get_Visibility_Points_Wrapper()
{
	if (!MGM_Get_Visibility_Points_Pointer)
		return -1;
	
	__try
	{
		return MGM_Get_Visibility_Points_Pointer();
	}
	
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		return -1;
	}
}

// Reputation Level Tracker
int DB_Get_Reputation_Level_Wrapper()
{
	if (!DB_Get_Reputation_Level_Pointer)
		return -1;
	
	__try
	{
		return DB_Get_Reputation_Level_Pointer();
	}
	
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		return -1;
	}
}

// Global_Sound_Tracker
int GlobalSoundGetCurrentMs()
{
    if (!Global_Sound_Get_Current_Pointer)
        return -1;

    __try
    {
        return Global_Sound_Get_Current_Pointer();
    }

    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}

// Run Script Pointer Locator
bool Initialise_Run_Script()
{
    const char* pattern = "\xE8\x00\x00\x00\x00\x8B\x4C\x24\x28\x51\x6A\x4C\xE8\x00\x00\x00\x00\x83\xC4\x1C\x85\xC0\x5F\x5E";
    const char* mask = "x????xxxxxxxx????xxxxxxx";
	uintptr_t addr = FindPattern(pattern, mask);

    if (!addr)
        return false;

    Run_Script_Address = addr;
    uintptr_t Function_Address = 0;
    ReadCall(Run_Script_Address, &Function_Address);
	Run_Script_Pointer = (Run_Script_Function)Function_Address;

    return (Run_Script_Pointer != NULL);
} 

// Run Script Wrapper
void RunScript(const char* script)
{
    if (!script || !Run_Script_Pointer)
			return;

    __try
    {
        Run_Script_Pointer(script, 0, 0, 0, -1);
    }

    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

// CVM_Get_Main_Character_Package Wrapper
const char* CVM_Get_Main_Character_Package_Wrapper()
{
    if (!CVM_Get_Main_Character_Package_Pointer)
        return "";

    __try
    {
        return CVM_Get_Main_Character_Package_Pointer();
    }

    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return "";
    }
}

// String Trimmer
std::string Trim(const std::string& str) 
{
    size_t first = str.find_first_not_of(" \t\r\n");
    
	if (first == std::string::npos) 
		return "";
    
	size_t last = str.find_last_not_of(" \t\r\n");
    
	return str.substr(first, last - first + 1);
}

// String -> Integer Conversion
int ParseInt(const std::string& str, int defaultValue) 
{
    try 
	{
        return std::stoi(str);
    } 
	
	catch (...) 
	{
        return defaultValue;
    }
}

// String -> Float Conversion
float ParseFloat(const std::string& str, float defaultValue) 
{
    try 
	{
        return std::stof(str);
    } 
	
	catch (...) 
	{
        return defaultValue;
    }
}

// Microsoft Xinput Bindings Parsing
WORD ParseButton(const std::string& name) 
{
    if (name == "A") 
		return XINPUT_GAMEPAD_A;
    
	if (name == "B") 
		return XINPUT_GAMEPAD_B;
    
	if (name == "X") 
		return XINPUT_GAMEPAD_X;
    
	if (name == "Y") 
		return XINPUT_GAMEPAD_Y;
    
	if (name == "LB") 
		return XINPUT_GAMEPAD_LEFT_SHOULDER;
    
	if (name == "RB") 
		return XINPUT_GAMEPAD_RIGHT_SHOULDER;
    
	if (name == "L3") 
		return XINPUT_GAMEPAD_LEFT_THUMB;
    
	if (name == "R3") 
		return XINPUT_GAMEPAD_RIGHT_THUMB;
    
	if (name == "UP") 
		return XINPUT_GAMEPAD_DPAD_UP;
    
	if (name == "DOWN") 
		return XINPUT_GAMEPAD_DPAD_DOWN;
    
	if (name == "LEFT") 
		return XINPUT_GAMEPAD_DPAD_LEFT;
    
	if (name == "RIGHT") 
		return XINPUT_GAMEPAD_DPAD_RIGHT;
    
	if (name == "START") 
		return XINPUT_GAMEPAD_START;
    
	if (name == "BACK") 
		return XINPUT_GAMEPAD_BACK;
    
	return XINPUT_GAMEPAD_RIGHT_THUMB; // Default
}

// Parse key name to Virtual Key Code
int ParseKey(const std::string& name) {
    
	// Single Character Keys : A - Z \ 0-9
    if (name.length() == 1) 
	{
        char c = toupper(name[0]);
        
		if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
			return c;
    }
	
	// Shifted Symbol Keys
	if (name == "!") 
		return '1';                          
    
	if (name == "@") 
		return '2';
    
	if (name == "#") 
		return '3';
    
	if (name == "$" || name == "₹") 
		return '4';
    
	if (name == "%") 
		return '5';
    
	if (name == "^") 
		return '6';
    
	if (name == "&") 
		return '7';
    
	if (name == "*") 
		return '8';
    
	if (name == "(") 
		return '9';
    
	if (name == ")") 
		return '0';
	
	// Punctuation & Symbol Keys
    if (name == "[" || name == "{") 
		return VK_OEM_4;
    
	if (name == "]" || name == "}") 
		return VK_OEM_6;
    
	if (name == ";" || name == ":") 
		return VK_OEM_1;
    
	if (name == "'" || name == "\"") 
		return VK_OEM_7;
    
	if (name == "," || name == "<") 
		return VK_OEM_COMMA;
    
	if (name == "." || name == ">") 
		return VK_OEM_PERIOD;
    
	if (name == "/" || name == "?") 
		return VK_OEM_2;
    
	if (name == "`" || name == "~") 
		return VK_OEM_3;
    
	if (name == "-" || name == "_") 
		return VK_OEM_MINUS;
    
	if (name == "=" || name == "+") 
		return VK_OEM_PLUS;
    
	if (name == "\\" || name == "|") 
		return VK_OEM_5;
    
    // Function keys
    if (name == "F1") 
		return VK_F1;
    
	if (name == "F2") 
		return VK_F2;
    
	if (name == "F3") 
		return VK_F3;
    
	if (name == "F4") 
		return VK_F4;
    
	if (name == "F5") 
		return VK_F5;
    
	if (name == "F6") 
		return VK_F6;
    
	if (name == "F7") 
		return VK_F7;
    
	if (name == "F8") 
		return VK_F8;
    
	if (name == "F9") 
		return VK_F9;
    
	if (name == "F10") 
		return VK_F10;
    
	if (name == "F11") 
		return VK_F11;
    
	if (name == "F12") 
		return VK_F12;
    
    // Arrow keys
    if (name == "UP") 
		return VK_UP;
    
	if (name == "DOWN") 
		return VK_DOWN;
    
	if (name == "LEFT") 
		return VK_LEFT;
    
	if (name == "RIGHT") 
		return VK_RIGHT;
    
    // Modifier keys
    if (name == "SHIFT") 
		return VK_SHIFT;
    
	if (name == "LSHIFT") 
		return VK_LSHIFT;
    
	if (name == "RSHIFT") 
		return VK_RSHIFT;
    
	if (name == "CTRL") 
		return VK_CONTROL;
    
	if (name == "LCTRL") 
		return VK_LCONTROL;
    
	if (name == "RCTRL") 
		return VK_RCONTROL;
    
	if (name == "ALT") 
		return VK_MENU;
    
	if (name == "LALT") 
		return VK_LMENU;
    
	if (name == "RALT") 
		return VK_RMENU;
    
    // Special keys
    if (name == "SPACE") 
		return VK_SPACE;
    
	if (name == "ENTER") 
		return VK_RETURN;
    
	if (name == "TAB") 
		return VK_TAB;
    
	if (name == "BACKSPACE") 
		return VK_BACK;
    
	if (name == "DELETE") 
		return VK_DELETE;
    
	if (name == "INSERT") 
		return VK_INSERT;
    
	if (name == "HOME") 
		return VK_HOME;
    
	if (name == "END") 
		return VK_END;
    
	if (name == "PAGEUP") 
		return VK_PRIOR;
    
	if (name == "PAGEDOWN") 
		return VK_NEXT;
    
	if (name == "ESC") 
		return VK_ESCAPE;
    
	if (name == "CAPSLOCK") 
		return VK_CAPITAL;
    
    // Numpad keys
    if (name == "NUMPAD0") 
		return VK_NUMPAD0;
    
	if (name == "NUMPAD1") 
		return VK_NUMPAD1;
    
	if (name == "NUMPAD2") 
		return VK_NUMPAD2;
    
	if (name == "NUMPAD3") 
		return VK_NUMPAD3;
    
	if (name == "NUMPAD4") 
		return VK_NUMPAD4;
    
	if (name == "NUMPAD5") 
		return VK_NUMPAD5;
    
	if (name == "NUMPAD6") 
		return VK_NUMPAD6;
    
	if (name == "NUMPAD7") 
		return VK_NUMPAD7;
    
	if (name == "NUMPAD8") 
		return VK_NUMPAD8;
    
	if (name == "NUMPAD9") 
		return VK_NUMPAD9;
    
    // Mouse buttons
    if (name == "MOUSE1") 
		return VK_LBUTTON;
    
	if (name == "MOUSE2") 
		return VK_RBUTTON;
    
	if (name == "MOUSE3") 
		return VK_MBUTTON;
    
	if (name == "MOUSE4") 
		return VK_XBUTTON1;
    
	if (name == "MOUSE5") 
		return VK_XBUTTON2;
    
    return 'V'; // Default
}

// Configuration Initialisation
void LoadConfig() 
{
    // Dodges & Evades Defaults
    g_Config.frontKey = 'V';
    g_Config.leftKey = 'A';
    g_Config.rightKey = 'D';
	g_Config.backKey = 'C';
	g_Config.leftPeekShootKey = 'Q';
	g_Config.rightPeekShootKey = 'E';
	g_Config.copBribeKey = 'Y';
	g_Config.gangBribeKey = 'U';
	g_Config.backupKey = 'I';
    
    g_Config.frontButton = XINPUT_GAMEPAD_A;
    g_Config.leftButton = XINPUT_GAMEPAD_B; 
    g_Config.rightButton = XINPUT_GAMEPAD_X;
	g_Config.backButton = XINPUT_GAMEPAD_Y;
	g_Config.leftPeekShootButton = XINPUT_GAMEPAD_LEFT_SHOULDER;
	g_Config.rightPeekShootButton = XINPUT_GAMEPAD_RIGHT_SHOULDER;
	g_Config.copBribeButton = XINPUT_GAMEPAD_LEFT_THUMB;
	g_Config.gangBribeButton = XINPUT_GAMEPAD_RIGHT_THUMB;
	g_Config.backupButton = XINPUT_GAMEPAD_DPAD_UP;
    
    g_Config.diFrontButton = 0;
    g_Config.diLeftButton = 1;
    g_Config.diRightButton = 2;
    g_Config.diBackButton = 3;
    g_Config.diLeftPeekShootButton = 4;
    g_Config.diRightPeekShootButton = 5;
	g_Config.diCopBribeButton = 6;
	g_Config.diGangBribeButton = 7;
	g_Config.diBackupButton = 8;
    
    g_Config.frontAnimation = "GEN_Action_Evade_Dive_N";
    g_Config.leftAnimation = "Left_Dive_Dodge_Roll_West";
    g_Config.rightAnimation = "Right_Dive_Dodge_Roll_East";
	g_Config.backAnimation = "Proximity_Weapon_Attack";
	g_Config.leftPeekShootAnimation = "Left_Peek_Shoot";
	g_Config.rightPeekShootAnimation = "Right_Peek_Shoot";
	g_Config.HealthRecovery = "Health_Recovery";
	g_Config.SwitchingWeapons = "Switching_Weapons";
	g_Config.copBribeAnimation = "Cop_Bribe";
	g_Config.gangBribeAnimation = "Gang_Bribe";
	g_Config.backupAnimation = "Backup";
	
	// Vehicles
	g_Config.waterVehicleDriver = "Reset_Boat_Steer_Generic";
	g_Config.waterVehiclePassenger = "Reset_Pass_Sit";
	g_Config.waterVehicleIdles = "Water_Vehicles_Idles";            
	g_Config.waterVehicleDamageDriver = "Driver_Water_Vehicle_Damage";
	g_Config.waterVehicleDamagePassenger = "Passenger_Water_Vehicle_Damage";
	g_Config.landVehicleDriver = "Reset_Steer_Forward";
	g_Config.landVehicleDriverReverse = "Reset_Steer_Reverse";
	g_Config.landVehicleIdles = "Land_Vehicles_Idles";   
	g_Config.landVehicleDamageDriver = "Driver_Land_Vehicle_Damage";
	g_Config.landVehicleDamageDriverReverse = "Driver_Land_Vehicle_Damage_Reverse";
	g_Config.landVehicleDamagePassenger = "Passenger_Land_Vehicle_Damage";
	g_Config.Damage_50_Calibers = "Damage_50_Calibers";
	g_Config.landVehicleDriverTalking = "Land_Vehicles_Driver_Talking";
	g_Config.landVehicleDriverReverseTalking = "Land_Vehicles_Driver_Reverse_Talking";
	g_Config.landVehiclePassengerTalking = "Land_Vehicles_Passenger_Talking";
	g_Config.waterVehicleDriverTalking = "Water_Vehicles_Driver_Talking";
	g_Config.waterVehiclePassengerTalking = "Water_Vehicles_Passenger_Talking";
    
    g_Config.pressMode = 2;
    g_Config.pressWindow = 210;
    g_Config.cooldown = 753;
    
    // 1st Person Camera Defaults
    g_FPSConfig.enabled = false;
	g_FPSConfig.footOnly = false;
	g_FPSConfig.bonePosition = 13;
    g_FPSConfig.heightAdjust = 0.0f;
    g_FPSConfig.sideAdjust = 0.0f;
    g_FPSConfig.distanceAdjust = 0.0f;
	
	// Read-Me File Creation
	std::ifstream file_(README_FILE);
	
	if (!file_.is_open())
	{
        std::ofstream outFile(README_FILE);
		outFile << "Scarface: The World Is Yours-\n\n\t";
		outFile << "ScarfaceEX->\n\n\t\t";
		outFile << "Keyboard & Mouse Controls:\n\n\t\t\t";
		outFile << "Available Inputs-\n\n\t\t\t\t";
		outFile << "0 <-> 9\n\t\t\t\t";
		outFile << "A <-> Z\n\t\t\t\t";
		outFile << "F1 <-> F12\n\t\t\t\t";
		outFile << "MOUSE1 <-> MOUSE5\n\t\t\t\t";
		outFile << "NUMPAD0 <-> NUMPAD9\n\t\t\t\t";
		outFile << "SHIFT\n\t\t\t\t";
		outFile << "CTRL\n\t\t\t\t";
		outFile << "ALT\n\t\t\t\t";
		outFile << "SPACE\n\t\t\t\t";
		outFile << "ENTER\n\t\t\t\t";
		outFile << "TAB\n\t\t\t\t";
		outFile << "UP\n\t\t\t\t";
		outFile << "DOWN\n\t\t\t\t";
		outFile << "LEFT\n\t\t\t\t";
		outFile << "RIGHT\n\t\t\t\t";
		outFile << "PAGEUP\n\t\t\t\t";
		outFile << "PAGEDOWN\n\t\t\t\t";
		outFile << "HOME\n\t\t\t\t";
		outFile << "END\n\t\t\t\t";
		outFile << "ESC\n\t\t\t\t";
		outFile << "INSERT\n\t\t\t\t";
		outFile << "DELETE\n\t\t\t\t";
		outFile << "[\n\t\t\t\t";
		outFile << "]\n\t\t\t\t";
		outFile << ";\n\t\t\t\t";
		outFile << "'\n\t\t\t\t";
		outFile << ",\n\t\t\t\t";
		outFile << ".\n\t\t\t\t";
		outFile << "/\n\t\t\t\t";
		outFile << "`\n\t\t\t\t";
		outFile << "-\n\t\t\t\t";
		outFile << "=\n\t\t\t\t";
		outFile << "\\\n\t\t\t\t";
		outFile << "{\n\t\t\t\t";
		outFile << "}\n\t\t\t\t";
		outFile << ":\n\t\t\t\t";
		outFile << "<\n\t\t\t\t";
		outFile << ">\n\t\t\t\t";
		outFile << "?\n\t\t\t\t";
		outFile << "~\n\t\t\t\t";
		outFile << "_\n\t\t\t\t";
		outFile << "+\n\t\t\t\t";
		outFile << "|\n\t\t\t\t";
		outFile << "\"\n\t\t\t\t";
		outFile << "\!\n\t\t\t\t";
		outFile << "\@\n\t\t\t\t";
		outFile << "\#\n\t\t\t\t";
		outFile << "\₹ OR $\n\t\t\t\t";
		outFile << "\%\n\t\t\t\t";
		outFile << "\^\n\t\t\t\t";
		outFile << "\&\n\t\t\t\t";
		outFile << "\*\n\t\t\t\t";
		outFile << "\(\n\t\t\t\t";
		outFile << "\)\n\n\t\t";
		outFile << "Xbox Controller:\n\n\t\t\t";
        outFile << "Available Inputs-\n\n\t\t\t\t";
		outFile << "A\n\t\t\t\t";
		outFile << "B\n\t\t\t\t";
		outFile << "X\n\t\t\t\t";
		outFile << "Y\n\t\t\t\t";
		outFile << "LB\n\t\t\t\t";
		outFile << "RB\n\t\t\t\t";
		outFile << "L3\n\t\t\t\t";
		outFile << "R3\n\t\t\t\t";
		outFile << "UP\n\t\t\t\t";
		outFile << "DOWN\n\t\t\t\t";
		outFile << "LEFT\n\t\t\t\t";
		outFile << "RIGHT\n\t\t\t\t";
		outFile << "START\n\t\t\t\t";
		outFile << "BACK\n\n\t\t";
		outFile << "Direct Input Controller:\n\n\t\t\t";
		outFile << "Available Inputs-\n\n\t\t\t\t";
		outFile << "0 <-> 31\n\t\t\t\t";
		outFile << "Notes->\n\n\t\t\t\t\t";
		outFile << "Model-dependent button mapping.\n\t\t\t\t\t";
		outFile << "Button inputs only.\n\n\t\t";
		outFile << "Animations:\n\n\t\t\t";
		outFile << "Available Animations-\n\n\t\t\t\t";
		outFile << "Check 'packages/zo4/Animation.p3d'.\n\n\t\t";
		outFile << "Behavior:\n\n\t\t\t";
		outFile << "Press_Mode- Required sequential inputs.\n\t\t\t";
		outFile << "Press_Window (milliseconds)- Input deadline.\n\t\t\t";
		outFile << "Cooldown (milliseconds)- Success input block time.\n\n\t\t";
		outFile << "First Person Camera:\n\n\t\t\t";
		outFile << "Foot_Only- Activation scope.\n\t\t\t";
		outFile << "Bone_Position- Camera anchor bone.\n\t\t\t";
		outFile << "Available Bones-\n\n\t\t\t\t";
		outFile << "0-> Motion_Root\n\t\t\t\t";
		outFile << "1-> Balance_Root\n\t\t\t\t";
		outFile << "2-> Character_Root\n\t\t\t\t";
		outFile << "3-> Pelvis\n\t\t\t\t";
		outFile << "4-> Hip_L\n\t\t\t\t";
		outFile << "5-> Knee_L\n\t\t\t\t";
		outFile << "6-> Ankle_L\n\t\t\t\t";
		outFile << "7-> Hip_R\n\t\t\t\t";
		outFile << "8-> Knee_R\n\t\t\t\t";
		outFile << "9-> Ankle_R\n\t\t\t\t";
		outFile << "10-> Spine_1\n\t\t\t\t";
		outFile << "11-> Spine_2\n\t\t\t\t";
		outFile << "12-> Neck\n\t\t\t\t";
		outFile << "13-> Head\n\t\t\t\t";
		outFile << "14-> Clavicle_L\n\t\t\t\t";
		outFile << "15-> Shoulder_L\n\t\t\t\t";
		outFile << "16-> Elbow_L\n\t\t\t\t";
		outFile << "17-> Forarm_L\n\t\t\t\t";
		outFile << "18-> Wrist_L\n\t\t\t\t";
		outFile << "19-> Clavicle_R\n\t\t\t\t";
		outFile << "20-> Shoulder_R\n\t\t\t\t";
		outFile << "21-> Elbow_R\n\t\t\t\t";
		outFile << "22-> Forarm_R\n\t\t\t\t";
		outFile << "23-> Wrist_R\n\t\t\t\t";
		outFile << "24-> right_hand_attach\n\t\t\t\t";
		outFile << "25-> Rage_Joint\n\n\t\t\t";
		outFile << "Height_Adjust- Vertical Offset\n\t\t\t";
		outFile << "Side_Adjust- Horizontal Offset\n\t\t\t";
		outFile << "Distance_Adjust- Front / Rear Offset";
		outFile.close();
	}
    
    // Configuration File Creation
	std::ifstream file(CONFIG_FILE);
    
	if (!file.is_open()) 
	{
        std::ofstream outFile(CONFIG_FILE);
		outFile << "[Keyboard_Mouse_Controls]\n\n";
		outFile << "Backup_Key=I\n";
		outFile << "Cop_Bribe_Key=Y\n";
		outFile << "Gang_Bribe_Key=U\n";
		outFile << "Front_Key=V\n";
		outFile << "Left_Key=A\n";
		outFile << "Right_Key=D\n";
		outFile << "Back_Key=C\n";
		outFile << "Left_Peek_Shoot_Key=Q\n";
		outFile << "Right_Peek_Shoot_Key=E\n\n";
		outFile << "[Xbox_Controller]\n\n";
		outFile << "Backup_Button=UP\n";
		outFile << "Cop_Bribe_Button=L3\n";
		outFile << "Gang_Bribe_Button=R3\n";
		outFile << "Front_Button=A\n";
		outFile << "Left_Button=B\n";
		outFile << "Right_Button=X\n";
		outFile << "Back_Button=Y\n";
		outFile << "Left_Peek_Shoot_Button=LB\n";
		outFile << "Right_Peek_Shoot_Button=RB\n\n";
		outFile << "[Direct_Input_Controller]\n\n";
		outFile << "Front_Button=0\n";
		outFile << "Left_Button=1\n";
		outFile << "Right_Button=2\n";
		outFile << "Back_Button=3\n";
		outFile << "Left_Peek_Shoot_Button=4\n";
		outFile << "Right_Peek_Shoot_Button=5\n";
		outFile << "Cop_Bribe_Button=6\n";
		outFile << "Gang_Bribe_Button=7\n";
		outFile << "Backup_Button=8\n\n";
		outFile << "[Animations]\n\n";
		outFile << "Backup_Animation=Backup\n";
		outFile << "Cop_Bribe_Animation=Cop_Bribe\n";
		outFile << "Gang_Bribe_Animation=Gang_Bribe\n";
		outFile << "Front_Animation=GEN_Action_Evade_Dive_N\n";
		outFile << "Left_Animation=Left_Dive_Dodge_Roll_West\n";
		outFile << "Right_Animation=Right_Dive_Dodge_Roll_East\n";
		outFile << "Back_Animation=Proximity_Weapon_Attack\n";
		outFile << "Left_Peek_Shoot_Animation=Left_Peek_Shoot\n";
        outFile << "Right_Peek_Shoot_Animation=Right_Peek_Shoot\n";
		outFile << "Switching_Weapons=Switching_Weapons\n";
		outFile << "Health_Recovery=Health_Recovery\n\n";
		outFile << "[Behavior]\n\n";
		outFile << "Press_Mode=2\n";
		outFile << "Press_Window=210\n";
		outFile << "Cooldown=753\n\n";
		outFile << "[Vehicles]\n\n";
		outFile << "Land_Vehicles_Driver_Talking=Land_Vehicles_Driver_Talking\n";
		outFile << "Land_Vehicles_Driver_Reverse_Talking=Land_Vehicles_Driver_Reverse_Talking\n";
		outFile << "Land_Vehicles_Passenger_Talking=Land_Vehicles_Passenger_Talking\n";
		outFile << "Water_Vehicles_Driver_Talking=Water_Vehicles_Driver_Talking\n";
		outFile << "Water_Vehicles_Passenger_Talking=Water_Vehicles_Passenger_Talking\n";
		outFile << "Water_Vehicle_Passenger=Reset_Pass_Sit\n";
		outFile << "Water_Vehicle_Driver=Reset_Boat_Steer_Generic\n";
		outFile << "Land_Vehicle_Driver=Reset_Steer_Forward\n";
		outFile << "Land_Vehicle_Driver_Reverse=Reset_Steer_Reverse\n";
		outFile << "Water_Vehicle_Idles=Water_Vehicles_Idles\n";
		outFile << "Land_Vehicle_Idles=Land_Vehicles_Idles\n";
		outFile << "Driver_Land_Vehicle_Damage=Driver_Land_Vehicle_Damage\n";
		outFile << "Driver_Land_Vehicle_Damage_Reverse=Driver_Land_Vehicle_Damage_Reverse\n";
		outFile << "Driver_Water_Vehicle_Damage=Driver_Water_Vehicle_Damage\n";
		outFile << "Passenger_Land_Vehicle_Damage=Passenger_Land_Vehicle_Damage\n";
		outFile << "Passenger_Water_Vehicle_Damage=Passenger_Water_Vehicle_Damage\n";
		outFile << "Damage_50_Calibers=Damage_50_Calibers\n\n";
		outFile << "[First_Person_Camera]\n\n";
		outFile << "Enabled=0\n";
		outFile << "Foot_Only=0\n";
		outFile << "Bone_Position=13\n";
		outFile << "Height_Adjust=0.0\n";
		outFile << "Side_Adjust=0.0\n";
		outFile << "Distance_Adjust=0.0";
        outFile.close();
        return;
    }
    
    std::string line, 
				section;
    
	while (std::getline(file, line)) 
	{
        line = Trim(line);
        
		if (line.empty() || line[0] == ';') 
			continue;        
        
		if (line[0] == '[') 
		{
            section = line.substr(1, line.find(']') - 1);
            continue;
        }
        
        size_t pos = line.find('=');
        
		if (pos == std::string::npos) 
			continue;
        
        std::string key = Trim(line.substr(0, pos)), 
					value = Trim(line.substr(pos + 1));
		
		// Inputs Parsing
        if (section == "Keyboard_Mouse_Controls") 
		{
            if (key == "Front_Key") 
				g_Config.frontKey = ParseKey(value);
            
			if (key == "Left_Key") 
				g_Config.leftKey = ParseKey(value);
            
			if (key == "Right_Key") 
				g_Config.rightKey = ParseKey(value);
            
			if (key == "Back_Key") 
				g_Config.backKey = ParseKey(value);
            
			if (key == "Left_Peek_Shoot_Key") 
				g_Config.leftPeekShootKey = ParseKey(value);
            
			if (key == "Right_Peek_Shoot_Key") 
				g_Config.rightPeekShootKey = ParseKey(value);
            
			if (key == "Cop_Bribe_Key") 
				g_Config.copBribeKey = ParseKey(value);
            
			if (key == "Gang_Bribe_Key") 
				g_Config.gangBribeKey = ParseKey(value);
            
			if (key == "Backup_Key") 
				g_Config.backupKey = ParseKey(value);
        }
        
		if (section == "Xbox_Controller") 
		{
            if (key == "Front_Button") 
				g_Config.frontButton = ParseButton(value);
            
			if (key == "Left_Button") 
				g_Config.leftButton = ParseButton(value);
            
			if (key == "Right_Button") 
				g_Config.rightButton = ParseButton(value);
			
			if (key == "Back_Button") 
				g_Config.backButton = ParseButton(value);
            
			if (key == "Left_Peek_Shoot_Button") 
				g_Config.leftPeekShootButton = ParseButton(value);
			
			if (key == "Right_Peek_Shoot_Button") 
				g_Config.rightPeekShootButton = ParseButton(value);
            
			if (key == "Cop_Bribe_Button") 
				g_Config.copBribeButton = ParseButton(value);
			
			if (key == "Gang_Bribe_Button") 
				g_Config.gangBribeButton = ParseButton(value);
			
			if (key == "Backup_Button") 
				g_Config.backupButton = ParseButton(value);
        }
        
		if (section == "Direct_Input_Controller") 
		{
            if (key == "Front_Button") 
				g_Config.diFrontButton = ParseInt(value, 0);
            
			if (key == "Left_Button") 
				g_Config.diLeftButton = ParseInt(value, 1);
            
			if (key == "Right_Button") 
				g_Config.diRightButton = ParseInt(value, 2);
            
			if (key == "Back_Button") 
				g_Config.diBackButton = ParseInt(value, 3);
            
			if (key == "Left_Peek_Shoot_Button") 
				g_Config.diLeftPeekShootButton = ParseInt(value, 4);
            
			if (key == "Right_Peek_Shoot_Button") 
				g_Config.diRightPeekShootButton = ParseInt(value, 5);
            
			if (key == "Cop_Bribe_Button") 
				g_Config.diCopBribeButton = ParseInt(value, 6);
            
			if (key == "Gang_Bribe_Button") 
				g_Config.diGangBribeButton = ParseInt(value, 7);
            
			if (key == "Backup_Button") 
				g_Config.diBackupButton = ParseInt(value, 8);
        }
        
		if (section == "Animations") 
		{
            if (key == "Front_Animation" && !value.empty()) 
				g_Config.frontAnimation = value;
            
			if (key == "Left_Animation" && !value.empty()) 
				g_Config.leftAnimation = value;
            
			if (key == "Right_Animation" && !value.empty()) 
				g_Config.rightAnimation = value;
            
			if (key == "Back_Animation" && !value.empty()) 
				g_Config.backAnimation = value;
            
			if (key == "Left_Peek_Shoot_Animation" && !value.empty()) 
				g_Config.leftPeekShootAnimation = value;
            
			if (key == "Right_Peek_Shoot_Animation" && !value.empty()) 
				g_Config.rightPeekShootAnimation = value;
            
			if (key == "Health_Recovery" && !value.empty()) 
				g_Config.HealthRecovery = value; 
            
			if (key == "Switching_Weapons" && !value.empty()) 
				g_Config.SwitchingWeapons = value; 
            
			if (key == "Cop_Bribe_Animation" && !value.empty()) 
				g_Config.copBribeAnimation = value; 
            
			if (key == "Gang_Bribe_Animation" && !value.empty()) 
				g_Config.gangBribeAnimation = value; 
            
			if (key == "Backup_Animation" && !value.empty()) 
				g_Config.backupAnimation = value; 
        }
		
		if (section == "Behavior") 
		{
            if (key == "Press_Mode") 
				g_Config.pressMode = ParseInt(value, 1);
            
			if (key == "Press_Window") 
				g_Config.pressWindow = ParseInt(value, 300);
            
			if (key == "Cooldown") 
				g_Config.cooldown = ParseInt(value, 500);
        }
        
		if (section == "First_Person_Camera") 
		{
            if (key == "Enabled") 
				g_FPSConfig.enabled = (ParseInt(value, 0) != 0);
            
			if (key == "Foot_Only") 
				g_FPSConfig.footOnly = (ParseInt(value, 0) != 0);
            
			if (key == "Bone_Position") 
				g_FPSConfig.bonePosition = ParseInt(value, 13);
            
			if (key == "Height_Adjust") 
				g_FPSConfig.heightAdjust = ParseFloat(value, 0.0f);
            
			if (key == "Side_Adjust") 
				g_FPSConfig.sideAdjust = ParseFloat(value, 0.0f);
            
			if (key == "Distance_Adjust") 
				g_FPSConfig.distanceAdjust = ParseFloat(value, 0.0f);
        }
		
		if (section == "Vehicles")
		{
			if (key == "Water_Vehicle_Passenger" && !value.empty()) 
				g_Config.waterVehiclePassenger = value;
			
			if (key == "Water_Vehicle_Driver" && !value.empty()) 
				g_Config.waterVehicleDriver = value;
			
			if (key == "Land_Vehicle_Driver" && !value.empty()) 
				g_Config.landVehicleDriver = value;
			
			if (key == "Land_Vehicle_Driver_Reverse" && !value.empty()) 
				g_Config.landVehicleDriverReverse = value;
			
			if (key == "Water_Vehicle_Idles" && !value.empty()) 
				g_Config.waterVehicleIdles = value;
			
			if (key == "Land_Vehicle_Idles" && !value.empty()) 
				g_Config.landVehicleIdles = value;

			if (key == "Driver_Land_Vehicle_Damage" && !value.empty())
				g_Config.landVehicleDamageDriver = value;

			if (key == "Driver_Land_Vehicle_Damage_Reverse" && !value.empty())
				g_Config.landVehicleDamageDriverReverse = value;

			if (key == "Driver_Water_Vehicle_Damage" && !value.empty())
				g_Config.waterVehicleDamageDriver = value;

			if (key == "Passenger_Land_Vehicle_Damage" && !value.empty())
				g_Config.landVehicleDamagePassenger = value;

			if (key == "Passenger_Water_Vehicle_Damage" && !value.empty())
				g_Config.waterVehicleDamagePassenger = value;

			if (key == "Damage_50_Calibers" && !value.empty())
				g_Config.Damage_50_Calibers = value;
		}
    }
    file.close();
    
    // Validation
    if (g_Config.pressMode < 1) 
		g_Config.pressMode = 1;
    
	if (g_Config.pressWindow < 100) 
		g_Config.pressWindow = 100;
    
	if (g_Config.cooldown < 100) 
		g_Config.cooldown = 100;
} 

// 'CharacterObject :: RequestAnimation()' Execution Denied
bool IsInVehicle() 
{
    __try 
	{
        int vehicleState = *(int*)ADDR_VehicleState;
        return (vehicleState != 0);
    } 
	
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{
        return false;
    }
} 

// "RED VISIBILITY" Tracker
bool MGM_Visibility_Is_Red()
{
    bool result = false;

    __try
    {
        __asm
        {
            mov eax, 0x00642260
            call eax
            mov result, al
        }
    }

    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        result = false;
    }

    return result;
} 

// "FUCKED VISIBILITY" Tracker
bool MGM_Visibility_Is_Fucked()
{
    bool result = false;

    __try
    {
        __asm
        {
            mov eax, 0x00642270
            call eax
            mov result, al
        }
    }

    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        result = false;
    }

    return result;
} 

// Main Character Alive \ Dead Tracker
bool Main_Character_Death_Status()
{
    bool result = false;

    __try
    {
        __asm
        {
            mov eax, 0x004F4E70
            call eax
            mov result, al
        }
    }

    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return result;
} 

// Non-Interactive Sequence Tracker
bool Is_NIS_Active()
{
    bool result = false;

    __try
    {
        __asm
        {
            mov eax, 0x00441620
            call eax
            mov result, al
        }
    }

    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return result;
}

// Pause Menu Tracker
bool Is_Game_Paused_HUD()
{
    bool result = false;

    __try
    {
        __asm
        {
            mov eax, 0x005C02F0
            call eax
            mov result, al
        }
    }

    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return result;
} 

// Screen Transition Tracker
bool Get_Screen_Effects_Transition_Blacked_Out()
{
    bool result = false;

    __try
    {
        __asm
        {
            mov eax, 0x005D27E0
            call eax
            mov result, al
        }
    }

    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return result;
}

// Mission Tracker
bool Get_Mission_Active()
{
    bool result = false;

    __try
    {
        __asm
        {
            mov eax, 0x006420C0
            call eax
            mov result, al
        }
    }

    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return result;
}

// Player Teleportation Tracker
bool Get_Is_Teleporting()
{
    bool result = false;

    __try
    {
        __asm
        {
            mov eax, 0x004F5A90
            call eax
            mov result, al
        }
    }

    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return result;
}

// Antonio Ramuindo Montana \ Assassin \ Driver \ Enforcer Pointers
bool InitPlayerPointer() 
{
    const char* pattern = "\x8B\x0D\x00\x00\x00\x00\x85\xC9\x74\x09\xE8\x00\x00\x00\x00\x84\xC0\x75\x5F\x83\x7C\x24\x00\x00\x7E\x43"; 
	const char* mask = "xx????xxxxx????xxxxxxx??xx";
    uintptr_t addr = FindPattern(pattern, mask);
    
	if (!addr) 
		return false;
    
    g_PlayerPointerAddress = *(uintptr_t*)(addr + 2);
    
	return true;
} 

// Non-Playable Characters' Pointers
bool InitCVManager()
{
    const char* pattern = "\x8B\x15\x00\x00\x00\x00\x8B\x42\x40\x0F\xBF\x48\x38\x33\xFF\x85\xC9\x89\x44\x24\x14";
    const char* mask = "xx????xxxxxxxxxxxxxxx";
    uintptr_t addr = FindPattern(pattern, mask);

    if (!addr)
        return false;

    g_CVManagerAddress = *(uintptr_t*)(addr + 2);

    return true;
}

// 'RequestAnimation()' Pointer
bool InitPlayAnimation() 
{
    const char* pattern = "\x8B\xCE\xE8\x00\x00\x00\x00\x8D\x4C\x24\x54";
	const char* mask = "xxx????xxxx";
    uintptr_t addr = FindPattern(pattern, mask);
    
	if (!addr) 
		return false;
    
    uintptr_t funcAddr = 0;
    ReadCall(addr + 2, &funcAddr);    
	g_PlayAnimationFunc = (PlayAnimationFunc)funcAddr;
    
	return true;
}

// Character Bones' Pointer
bool InitGetBonePosition() 
{
    // Pattern For 'GetBonePosition' call [push 0Dh = g_FPSConfig.bonePosition; Call GetBonePosition]
    const char* pattern = "\x6A\x0D\xE8\x00\x00\x00\x00\x8B\x08\x8B\x50\x04\x8B\x40\x08\x89\x4C\x24\x08\x89\x4C\x24\x44\x8D\x4C\x24\x14\x51";
	const char* mask = "xxx????xxxxxxxxxxxxxxxxxxxx";
    uintptr_t addr = FindPattern(pattern, mask);
    
	if (!addr) 
	{
        // Alternative Pattern Without Result Handling
        const char* pattern2 = "\x6A\x0D\xE8\x00\x00\x00\x00";
		const char* mask2 = "xxx????";
        addr = FindPattern(pattern2, mask2);
        
		if (!addr) 
			return false;
    }
    
    uintptr_t funcAddr = 0;
    ReadCall(addr + 2, &funcAddr);
	g_GetBonePosition = (GetBonePositionFunc)funcAddr;
    
	return true;
}

// 'Camera :: SetPosition()' Pointer
bool InitCameraSetPosition() 
{
    const char* pattern = "\x8B\xC1\x8B\x4C\x24\x04\x8B\x11\x89\x50\x64\x8B\x51\x04\x89\x50\x68\x8B\x49\x08\x89\x48\x6C\x8B\x88\x00\x00\x00\x00\x8B\x11\x83\xC0\x34\x89\x44\x24\x04\xFF\x62\x64"; 
	const char* mask = "xxxxxxxxxxxxxxxxxxxxxxxxx????xxxxxxxxxxxx";
    uintptr_t addr = FindPattern(pattern, mask);
    
	if (!addr) 
		return false;
    
    g_OriginalSetPosition = (SetPositionFunc)addr;
    
	return true;
}

// Player Pointer Initialisation
CharacterObject* GetPlayer() 
{
    if (!g_PlayerPointerAddress) 
		return NULL;
    
	__try 
	{
        CharacterObject** pp = (CharacterObject**)g_PlayerPointerAddress;
        return pp ? *pp : NULL;
    }
	
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return NULL; 
	}
}

// Loading Tracker
bool Loading_Complete(CharacterObject* player)
{
	if (!player)
        return false;

    bool result = false;

    __try
    {
        __asm
        {
            push player
            mov eax, 0x0047B770
            call eax
            add esp, 4
            mov result, al
        }
    }
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return false; 
	}

    return result;
} 

// Is the character dead?
bool Dead_Body_Tracker(CharacterObject* character)
{
	bool deadBodyTracker = false;
	
	__try
	{
		deadBodyTracker = *(int*)((uintptr_t)character + 0x112);
	}
	
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		deadBodyTracker = false;
	}
	
	return deadBodyTracker;
}

// Gun Aiming Tracker
bool Gun_Up_State(CharacterObject* character)
{
	bool gunUpState = false;
	
	__try
	{
		gunUpState = *(int*)((uintptr_t)character + 0x380 + 0x4);
	}
	
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		gunUpState = false;
	}
	
	return gunUpState;
}

// Damage Weapon Detector
int Damage_Weapon(CharacterObject* character)
{
	__try
    {
        int damageWeapon = *(int*)((uintptr_t)character + 0x17C);
        return damageWeapon;
    }
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return 0; 
	}
} 

// Crouch State Detector
int Crouch_State(CharacterObject* character)
{
	__try
    {
        int crouchState = *(int*)((uintptr_t)character + 0x2A0 + 0x4);
        return crouchState;
    }
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return 0; 
	}
} 

// Current Drawn Weapon Detector
int Get_Current_Drawn_Weapon(CharacterObject* character)
{
    if (!character)
        return -1;

    int result = -1;

    __try
    {
        __asm
        {
            push character
            mov eax, 0x005882C0
            call eax
            add esp, 4
            mov result, eax
        }
    }

    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }

    return result;
}  

// Animation Request ID
int Get_Animation_Request_ID(CharacterObject* character)
{
    if (!character)
        return -1;

    int result = -1;

    __try
    {
        __asm
        {
            push character
            mov eax, 0x005874C0
            call eax
            add esp, 4
            mov result, eax
        }
    }

    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }

    return result;
} 

// CVManager Singleton Resolver
void* GetCVManager()
{
    if (!g_CVManagerAddress)
        return nullptr;

    __try
    {
        return *(void**)(g_CVManagerAddress);
    }
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return nullptr; 
	}
}

// CVManager :: Characters' List
void** GetNPCList(int* outCount)
{
    __try
    {
        void* mgr = GetCVManager();

        if (!mgr)
        {
            *outCount = 0;
            return nullptr;
        }

        // CVManager::GetCharacters() = *(GameSet<CharacterObject>**)(this + 0x40)
        void* gameSet = *(void**)((uintptr_t)mgr + 64);

        if (!gameSet)
        {
            *outCount = 0;
            return nullptr;
        }

        *outCount = *(short*)((uintptr_t)gameSet + 56); // GameSet::count
        return *(void***)((uintptr_t)gameSet + 48);     // GameSet::pData
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        *outCount = 0;
        return nullptr;
    }
} 

// Characters' Vehicle Animation Type Detector
int Vehicle_Animation(CharacterObject* character)
{
    __try
    {
        int pilotState = *(int*)((uintptr_t)character + 0x2E8 + 0xC);
        return pilotState;
    }
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return 0; 
	}
} 

// Characters' Vehicle Shooting Detector
int Vehicle_Shooting(CharacterObject* character)
{
    __try
    {
        int pilotState = *(int*)((uintptr_t)character + 0x2E8 + 0x8);
        return (pilotState == 1 || pilotState == 5) ? 1 : 0;
    }
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return 0; 
	}
} 

// Characters' Vehicle Intention Detector
int Actual_Vehicle_Intention(CharacterObject* character)
{
    __try
    {
        int pilotState = *(int*)((uintptr_t)character + 0x2E8);
        return (pilotState > 2 && pilotState < 6) ? 1 : 0;
    }
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return 0; 
	}
} 

// Characters' Passenger Vehicle State Detector
int Get_Vehicle_Passenger_State(CharacterObject* npc)
{
    __try
    {
        int pilotState = *(int*)((uintptr_t)npc + 0x2E8);
        return (pilotState == 4) ? 1 : 0;
    }
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return 0; 
	}
} 

// Characters' Vehicle State Detector
int Get_Vehicle_State(CharacterObject* npc)
{
    __try
    {
        int pilotState = *(int*)((uintptr_t)npc + 0x2E8);
        return (pilotState == 3) ? 1 : 0;
    }
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return 0; 
	}
}

// Characters' Vehicle Types' Detector
bool NPC_IsInCar(CharacterObject* npc)
{
    if (!npc)
        return false;

    bool result = false;

    __try
    {
        __asm
        {
            push npc
            mov eax, 0x005873E0
            call eax
            add esp, 4
            mov result, al
        }
    }
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return false; 
	}

    return result;
}

bool Rage_Mode_Active(CharacterObject* npc)
{
    if (!npc)
        return false;

    bool result = false;

    __try
    {
        __asm
        {
            push npc
            mov eax, 0x0058ABB0
            call eax
            add esp, 4
            mov result, al
        }
    }
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return false; 
	}

    return result;
}

bool NPC_IsInBoat(CharacterObject* npc)
{
    if (!npc)
        return false;

    bool result = false;

    __try
    {
        __asm
        {
            push npc
            mov eax, 0x00587400
            call eax
            add esp, 4
            mov result, al
        }
    }
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return false; 
	}

    return result;
}

// Current Voice Timespan of Characters
int Current_Voice_MS(CharacterObject* character)
{
    if (!character)
        return -1;

    int result = -1;

    __try
    {
        __asm
        {
            push character
            mov eax, 0x00587280
            call eax
			add esp, 4
            mov result, eax
        }
    }
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return -1; 
	}

    return result;
}

int Wall_Cover_Available(CharacterObject* character)
{
    if (!character)
        return -1;

    int result = -1;

    __try
    {
        __asm
        {
            mov ecx, character
            mov eax, 0x005879F0
            call eax
            mov result, eax
        }
    }
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return -1; 
	}

    return result;
}

// CharacterObject :: GetHealth()
int GetCharacterHealth(CharacterObject* character)
{
    if (!character)
        return -1;

    int result = -1;

    __try
    {
        __asm
        {
            mov ecx, character
            mov eax, 0x006E51D0
            call eax
            mov result, eax
        }
    }
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return -1; 
	}

    return result;
}

// Player Camera Matrix Initialisation
Matrix* GetCharacterMatrix(CharacterObject* character) 
{
    if (!character) 
		return NULL;
    
	__try 
	{
        return (Matrix*)((uintptr_t)character + 0x34);
    } 
	
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return NULL; 
	}
}

// 'CharacterObject :: RequestAnimation()' Initialisation
int PlayAnimation(CharacterObject* player, const std::string& animName, int priority) 
{
    if (!player || !g_PlayAnimationFunc) 
		return 0;
    
    // Converting std::string To char* For Hash Function
    char animBuffer[256];
    strncpy_s(animBuffer, animName.c_str(), sizeof(animBuffer) - 1);
	animBuffer[sizeof(animBuffer) - 1] = '\0';
    unsigned int hash = Hash(animBuffer);
    
    __try 
	{
        return g_PlayAnimationFunc(player, &hash, priority, 0);
    } 
	
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{
        return 0;
    }
}

// 'CharacterObject :: RequestAnimation()' Triggers Under Appropriate Conditions
void Dodge(CharacterObject* player, const std::string& animName) 
{
    std::string currentPackage = CVM_Get_Main_Character_Package_Wrapper();
	bool ActionMap = (IsInVehicle() || NPC_IsInBoat(player) || NPC_IsInCar(player)),
		 CopBribe = false,
		 GangBribe = false,
		 Backup = false,
		 Support = false,
		 phoneCallsTriggerValid[6] = {false, false, false, false, false, false},
		 backupByAssassinDriverEnforcerValid[7] = {false, false, false, false, false, false, false},
		 dodgesTriggerValid = false;
		 
	phoneCallsTriggerValid[0] = (!Get_Mission_Active() && (Current_Voice_MS(player) == -1) && !DPCBS_Struct.blockedFlag);
	phoneCallsTriggerValid[1] = (phoneCallsTriggerValid[0] && (Get_Animation_Request_ID(player) == 0) && (GlobalSoundGetCurrentMs() == -1));
	phoneCallsTriggerValid[2] = (phoneCallsTriggerValid[1] && !MGM_Visibility_Is_Red() && !MGM_Get_Visibility_Points_Wrapper() && !MGM_Visibility_Is_Fucked());
	phoneCallsTriggerValid[3] = (phoneCallsTriggerValid[2] && (DB_Get_Reputation_Level_Wrapper() >= 2) && (currentPackage != "MCP_Assassin"));
	phoneCallsTriggerValid[4] = (phoneCallsTriggerValid[3] && (currentPackage != "MCP_Driver") && (currentPackage != "MCP_Enforcer") && !Gun_Up_State(player));
	phoneCallsTriggerValid[5] = (phoneCallsTriggerValid[4] && !Get_Current_Drawn_Weapon(player) && !Crouch_State(player));
	
	backupByAssassinDriverEnforcerValid[0] = ((Current_Voice_MS(player) == -1) && (Get_Animation_Request_ID(player) == 0) && (GlobalSoundGetCurrentMs() == -1));
	backupByAssassinDriverEnforcerValid[1] = (backupByAssassinDriverEnforcerValid[0] && !MGM_Visibility_Is_Red() && !MGM_Get_Visibility_Points_Wrapper());
	backupByAssassinDriverEnforcerValid[2] = (backupByAssassinDriverEnforcerValid[1] && !MGM_Visibility_Is_Fucked() && !Get_Current_Drawn_Weapon(player));
	backupByAssassinDriverEnforcerValid[3] = (backupByAssassinDriverEnforcerValid[2] && !Crouch_State(player) && !Gun_Up_State(player));
	backupByAssassinDriverEnforcerValid[4] = ((currentPackage == "MCP_Assassin") || (currentPackage == "MCP_Driver") || (currentPackage == "MCP_Enforcer"));
	backupByAssassinDriverEnforcerValid[5] = (backupByAssassinDriverEnforcerValid[3] && backupByAssassinDriverEnforcerValid[4] && !DPCBS_Struct.blockedFlag);
	backupByAssassinDriverEnforcerValid[6] = (backupByAssassinDriverEnforcerValid[5] && !phoneCallsTriggerValid[5]);
	
	CopBribe = ((animName == "Cop_Bribe") && phoneCallsTriggerValid[5]);
	GangBribe = ((animName == "Gang_Bribe") && phoneCallsTriggerValid[5]);
	Backup = ((animName == "Backup") && phoneCallsTriggerValid[5]);
	Support = ((animName == "Backup") && backupByAssassinDriverEnforcerValid[6]);
	
	dodgesTriggerValid = ((animName != "Cop_Bribe") && (animName != "Gang_Bribe") && (animName != "Backup"));
	
	if (ActionMap)
		return;
	
	if (CopBribe)
	{
		RunScript("Phone_Calls(\"Cop_Bribe\"); return;");	
		DPCBS_Struct.blockedTimer = GetTickCount();
		DPCBS_Struct.blockedFlag = true;
	}
	
	if (GangBribe)
	{
		RunScript("Phone_Calls(\"Gang_Bribe\"); return;");	
		DPCBS_Struct.blockedTimer = GetTickCount();
		DPCBS_Struct.blockedFlag = true;
	}
	
	if (Backup)
	{
		RunScript("Phone_Calls(\"Backup\"); return;");	
		DPCBS_Struct.blockedTimer = GetTickCount();
		DPCBS_Struct.blockedFlag = true;
	}
	
	if (Support)
	{
		RunScript("Phone_Calls(\"Support\"); return;");
		DPCBS_Struct.blockedTimer = GetTickCount();
		DPCBS_Struct.blockedFlag = true;
	}
	
	if (dodgesTriggerValid)
		PlayAnimation(player, animName, 67);
}

// 1st Person Camera Triggers for Player Set Conditions
void __fastcall Camera_SetPositionHooked(void* camera, void* edx, Vector* pos) 
{
    if (g_FPSConfig.enabled) 
	{
        CharacterObject* player = GetPlayer();
        
        if (player && g_GetBonePosition) 
		{
            if (g_FPSConfig.footOnly && IsInVehicle()) 
			{
                __try 
				{
                    *(Vector*)((int)camera + 100) = *pos;
                    (*(void(__thiscall**)(int, int))(**(int**)((int)camera + 132) + 100))(*(int*)((int)camera + 132), (int)camera + 52);
                } 
				
				__except(EXCEPTION_EXECUTE_HANDLER) 
				{
				}
                
				return;
            }
            
			__try 
			{
                Matrix* charMatrix = GetCharacterMatrix(player);
                
                if (charMatrix) 
				{
                    Vector* headPos = NULL;
                    
					__asm
					{
                        mov ecx, player
                        push g_FPSConfig.bonePosition
                        call g_GetBonePosition
                        mov headPos, eax
                    }
                    
                    if (headPos) 
					{
                        Vector fpView;
                        fpView.X = headPos->X;
                        fpView.Y = headPos->Y;
                        fpView.Z = headPos->Z;
                        fpView.Y += g_FPSConfig.heightAdjust;
                        Vector forward = charMatrix->GetForward();
                        fpView = fpView + (forward * g_FPSConfig.distanceAdjust);
                        Vector right = charMatrix->GetRight();
                        fpView = fpView + (right * g_FPSConfig.sideAdjust);
                        *pos = fpView;
                    }
                }
            } 
			
			__except(EXCEPTION_EXECUTE_HANDLER) 
			{
			}
        }
    }
    
	__try 
	{
        *(Vector*)((int)camera + 100) = *pos;
        (*(void(__thiscall**)(int, int))(**(int**)((int)camera + 132) + 100))(*(int*)((int)camera + 132), (int)camera + 52);
    } 
	
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{
	}
}

// 1st Person Camera Hook Initialisation
bool InstallCameraHook() 
{
    if (!g_OriginalSetPosition) 
		return false;
    
	uintptr_t source = (uintptr_t)g_OriginalSetPosition, 
			  target = (uintptr_t)&Camera_SetPositionHooked;
    
	int relativeOffset = (int)(target - source - 5);
    DWORD oldProtect;
    
	if (!VirtualProtect((LPVOID)source, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
		return false;
    
    *(BYTE*)(source) = 0xE9;
    *(int*)(source + 1) = relativeOffset;
    VirtualProtect((LPVOID)source, 5, oldProtect, &oldProtect);
    
    return true;
}

// Dodges & Evades Input Parsing
void CheckKeyboardKey(CharacterObject* player, int key, int index, PressTracker& tracker, const std::string& animation, unsigned long now) 
{
    bool pressed = (GetAsyncKeyState(key) & 0x8000) != 0;
    
    if (pressed && !g_KeyboardState[index]) 
	{
        g_KeyboardState[index] = true;
        tracker.CheckPress(now, g_Config.pressWindow);
        
        if (tracker.ShouldTrigger(g_Config.pressMode)) 
		{
            Dodge(player, animation);
			g_LastDodgeTime = now;
            tracker.Reset();
        }
    }
    
	if (!pressed) 
	{
        g_KeyboardState[index] = false;
        
		if (now - tracker.lastPressTime > g_Config.pressWindow)
            tracker.Reset();
    }
}

void CheckKeyboard(CharacterObject* player, unsigned long now) 
{
    CheckKeyboardKey(player, g_Config.frontKey, 0, g_Trackers.front, g_Config.frontAnimation, now);
    CheckKeyboardKey(player, g_Config.leftKey, 1, g_Trackers.left, g_Config.leftAnimation, now);
    CheckKeyboardKey(player, g_Config.rightKey, 2, g_Trackers.right, g_Config.rightAnimation, now);
    CheckKeyboardKey(player, g_Config.backKey, 3, g_Trackers.back, g_Config.backAnimation, now);
    CheckKeyboardKey(player, g_Config.leftPeekShootKey, 4, g_Trackers.leftPeekShoot, g_Config.leftPeekShootAnimation, now);
    CheckKeyboardKey(player, g_Config.rightPeekShootKey, 5, g_Trackers.rightPeekShoot, g_Config.rightPeekShootAnimation, now);
    CheckKeyboardKey(player, g_Config.copBribeKey, 6, g_Trackers.copBribe, g_Config.copBribeAnimation, now);
    CheckKeyboardKey(player, g_Config.gangBribeKey, 7, g_Trackers.gangBribe, g_Config.gangBribeAnimation, now);
    CheckKeyboardKey(player, g_Config.backupKey, 8, g_Trackers.backup, g_Config.backupAnimation, now);
}

void CheckControllerButton(CharacterObject* player, WORD buttons, WORD button, int index, PressTracker& tracker, const std::string& animation, unsigned long now) 
{
    bool pressed = (buttons & button) != 0;
    
    if (pressed && !g_ControllerState[index]) 
	{
        g_ControllerState[index] = true;
        tracker.CheckPress(now, g_Config.pressWindow);
        
        if (tracker.ShouldTrigger(g_Config.pressMode)) 
		{
            Dodge(player, animation);
            g_LastDodgeTime = now;
            tracker.Reset();
        }
    }
	
    if (!pressed) 
	{
        g_ControllerState[index] = false;
        
		if (now - tracker.lastPressTime > g_Config.pressWindow)
            tracker.Reset();
    }
}

void CheckController(CharacterObject* player, unsigned long now) 
{
    XINPUT_STATE state;
    ZeroMemory(&state, sizeof(XINPUT_STATE));
    
	if (XInputGetState(0, &state) != ERROR_SUCCESS)
        return;
    
    WORD buttons = state.Gamepad.wButtons;
    CheckControllerButton(player, buttons, g_Config.frontButton, 0, g_Trackers.front, g_Config.frontAnimation, now);
    CheckControllerButton(player, buttons, g_Config.leftButton, 1, g_Trackers.left, g_Config.leftAnimation, now);
    CheckControllerButton(player, buttons, g_Config.rightButton, 2, g_Trackers.right, g_Config.rightAnimation, now);
    CheckControllerButton(player, buttons, g_Config.backButton, 3, g_Trackers.back, g_Config.backAnimation, now);
    CheckControllerButton(player, buttons, g_Config.leftPeekShootButton, 4, g_Trackers.leftPeekShoot, g_Config.leftPeekShootAnimation, now);
    CheckControllerButton(player, buttons, g_Config.rightPeekShootButton, 5, g_Trackers.rightPeekShoot, g_Config.rightPeekShootAnimation, now);
    CheckControllerButton(player, buttons, g_Config.copBribeButton, 6, g_Trackers.copBribe, g_Config.copBribeAnimation, now);
    CheckControllerButton(player, buttons, g_Config.gangBribeButton, 7, g_Trackers.gangBribe, g_Config.gangBribeAnimation, now);
    CheckControllerButton(player, buttons, g_Config.backupButton, 8, g_Trackers.backup, g_Config.backupAnimation, now);
}

//  Direct-Input Devices Enumeration
BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCE* pdidInstance, VOID* pContext) 
{
    HRESULT hr = g_pDI->CreateDevice(pdidInstance->guidInstance, &g_pJoystick, NULL);
    
	if (SUCCEEDED(hr))
        return DIENUM_STOP;
    
	return DIENUM_CONTINUE;
}

// Direct-Input Initialisation
bool InitDirectInput() 
{
    HINSTANCE hInstance = GetModuleHandle(NULL);
	
	// Direct-Input Device Interface Creation
    HRESULT hr = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (VOID**)&g_pDI, NULL);
    
	if (FAILED(hr))
        return false;
	
	// Direct-Input Device Enumeration
    hr = g_pDI->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, NULL, DIEDFL_ATTACHEDONLY);          
    
	if (FAILED(hr) || !g_pJoystick) 
	{
        if (g_pDI) 
		{
            g_pDI->Release();
            g_pDI = NULL;
        }
        
		return false;
    }
	
	// Simple Joystick
    hr = g_pJoystick->SetDataFormat(&c_dfDIJoystick2);                                                        
    
	if (FAILED(hr)) 
	{
        g_pJoystick->Release();
        g_pDI->Release();
        g_pJoystick = NULL;
        g_pDI = NULL;
        
		return false;
    }
	
	// Cooperative Level Initialisation
    HWND hwnd = GetForegroundWindow();                                                                        
    hr = g_pJoystick->SetCooperativeLevel(hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
    
	if (FAILED(hr)) 
	{
        g_pJoystick->Release();
        g_pDI->Release();
        g_pJoystick = NULL;
        g_pDI = NULL;
        
		return false;
    }
	
	// Device Acquisition
    g_pJoystick->Acquire();                                                                                   
    g_DirectInputEnabled = true;
    
	return true;
}

// Direct-Input Cleanup
void CleanupDirectInput() 
{
    if (g_pJoystick) 
	{
        g_pJoystick->Unacquire();
        g_pJoystick->Release();
        g_pJoystick = NULL;
    }
    
	if (g_pDI) 
	{
        g_pDI->Release();
        g_pDI = NULL;
    }
    
	g_DirectInputEnabled = false;
}

void CheckDirectInputButton(CharacterObject* player, const DIJOYSTATE2& js, int buttonIndex, int stateIndex, PressTracker& tracker, const std::string& animation, unsigned long now)
{
    bool pressed = (buttonIndex >= 0 && buttonIndex < 128) && (js.rgbButtons[buttonIndex] & 0x80);
    
    if (pressed && !g_DirectInputState[stateIndex]) 
	{
        g_DirectInputState[stateIndex] = true;
        tracker.CheckPress(now, g_Config.pressWindow);
        
        if (tracker.ShouldTrigger(g_Config.pressMode)) 
		{
            Dodge(player, animation);
            g_LastDodgeTime = now;
            tracker.Reset();
        }
    }
    
	if (!pressed) 
	{
        g_DirectInputState[stateIndex] = false;
        
		if (now - tracker.lastPressTime > g_Config.pressWindow)
            tracker.Reset();
    }
}

void CheckDirectInput(CharacterObject* player, unsigned long now) 
{
    if (!g_DirectInputEnabled || !g_pJoystick)
        return;
    
	// Device Polling For Reading The Current State
    DIJOYSTATE2 js;
    ZeroMemory(&js, sizeof(DIJOYSTATE2));
    HRESULT hr = g_pJoystick->Poll();     						
    
	if (FAILED(hr)) 
	{
		// Device Lost, Try Reacquiring
        hr = g_pJoystick->Acquire();      						
        while (hr == DIERR_INPUTLOST)
            hr = g_pJoystick->Acquire();
        
		// Failed Rcquiring It Back
        if (FAILED(hr))                   						
            return; 
    }
    
	// Device State Acquisition
    hr = g_pJoystick->GetDeviceState(sizeof(DIJOYSTATE2), &js); 
    
	if (FAILED(hr))
        return;
	
    CheckDirectInputButton(player, js, g_Config.diFrontButton, 0, g_Trackers.front, g_Config.frontAnimation, now);
    CheckDirectInputButton(player, js, g_Config.diLeftButton, 1, g_Trackers.left, g_Config.leftAnimation, now);
    CheckDirectInputButton(player, js, g_Config.diRightButton, 2, g_Trackers.right, g_Config.rightAnimation, now);
    CheckDirectInputButton(player, js, g_Config.diBackButton, 3, g_Trackers.back, g_Config.backAnimation, now);
    CheckDirectInputButton(player, js, g_Config.diLeftPeekShootButton, 4, g_Trackers.leftPeekShoot, g_Config.leftPeekShootAnimation, now);
    CheckDirectInputButton(player, js, g_Config.diRightPeekShootButton, 5, g_Trackers.rightPeekShoot, g_Config.rightPeekShootAnimation, now);
    CheckDirectInputButton(player, js, g_Config.diCopBribeButton, 6, g_Trackers.copBribe, g_Config.copBribeAnimation, now);
    CheckDirectInputButton(player, js, g_Config.diGangBribeButton, 7, g_Trackers.gangBribe, g_Config.gangBribeAnimation, now);
    CheckDirectInputButton(player, js, g_Config.diBackupButton, 8, g_Trackers.backup, g_Config.backupAnimation, now);
} 

// Vehicles' Idles' Cancellation Input Detection
bool IsAnyRawInputActive()
{
    // Keyboard Detection
    for (int vk = 0x08; vk <= 0xFE; vk++)
        if (GetAsyncKeyState(vk) & 0x8000)
            return true;
	
	// Mouse Detection
	if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) 
		return true;
	
	if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) 
		return true;
	
	if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) 
		return true;
	
	if (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) 
		return true;
	
	if (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) 
		return true;

    // Xbox Controller Detection 
    XINPUT_STATE xstate;
    ZeroMemory(&xstate, sizeof(XINPUT_STATE));

    if (XInputGetState(0, &xstate) == ERROR_SUCCESS)
    {
        XINPUT_GAMEPAD& gp = xstate.Gamepad;

        // Buttons
        if (gp.wButtons != 0)
            return true;

        // Triggers
        if (gp.bLeftTrigger > 30) 
			return true;
        
		if (gp.bRightTrigger > 30) 
			return true;

        // Sticks
        if (abs(gp.sThumbLX) > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)  
			return true;
        
		if (abs(gp.sThumbLY) > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)  
			return true;
        
		if (abs(gp.sThumbRX) > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) 
			return true;
        
		if (abs(gp.sThumbRY) > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) 
			return true;
    }

    // Direct-Input Detection
    if (g_DirectInputEnabled && g_pJoystick)
    {
        DIJOYSTATE2 js;
        ZeroMemory(&js, sizeof(DIJOYSTATE2));

        if (SUCCEEDED(g_pJoystick->Poll()) && SUCCEEDED(g_pJoystick->GetDeviceState(sizeof(DIJOYSTATE2), &js)))
        {
            for (int i = 0; i < 128; i++)
                if (js.rgbButtons[i] & 0x80) 
					return true;

            // Axes
            if (abs((int)js.lX - 32767) > 3000) 
				return true;
            
			if (abs((int)js.lY - 32767) > 3000) 
				return true;
            
			if (abs((int)js.lZ - 32767) > 3000) 
				return true;
            
			if (abs((int)js.lRx - 32767) > 3000) 
				return true;
            
			if (abs((int)js.lRy - 32767) > 3000) 
				return true;
            
			if (abs((int)js.lRz - 32767) > 3000) 
				return true;
        }
    }

    return false;
}

void CheckVehicleAnimation(CharacterObject* player)
{
    int vehicleState = *(int*)ADDR_VehicleState, 
		Reversing = Land_Vehicle_Reverse_Driving(player);
	
	unsigned long now = GetTickCount();
	
	if (vehicleState <= 0 || vehicleState > 2)
	{
		g_VehicleAnim.Reset();
		return;
	}
	
	if (!g_VehicleAnim.wasInVehicle)
    {
        g_VehicleAnim.idle = false;
        g_VehicleAnim.active = false;
        g_VehicleAnim.entryTime = now;      
        g_VehicleAnim.inputReleasedAt = now;      
        g_VehicleAnim.wasInVehicle = true;
		g_VehicleAnim.wasInputActive = false;
    }

    bool inputActive = IsAnyRawInputActive();
	
	if (g_VehicleAnim.wasInputActive && !inputActive)
        g_VehicleAnim.inputReleasedAt = now;

    g_VehicleAnim.wasInputActive = inputActive;
	
	if (inputActive)
    {
        if (!g_VehicleAnim.active)
        {			
			if (vehicleState == 1 && Reversing)
			{
				const std::string& anim = g_Config.landVehicleDriverReverse;
				PlayAnimation(player, anim, 0);
				g_VehicleAnim.active = true;
				g_VehicleAnim.idle = false;
			}
			
			else
			{
				const std::string& anim = (vehicleState == 1) ? g_Config.landVehicleDriver : g_Config.waterVehicleDriver;
				PlayAnimation(player, anim, 0);
				g_VehicleAnim.active = true;
				g_VehicleAnim.idle = false;
			}
        }
    }
    
	else
    {
        if (!g_VehicleAnim.idle)
		{
			bool entryDelayMet = (now - g_VehicleAnim.entryTime) >= 7532UL, 
				 releaseDelayMet = (now - g_VehicleAnim.inputReleasedAt) >= 7532UL;
			
			if (entryDelayMet && releaseDelayMet)
			{
				const std::string& anim = (vehicleState == 1) ? g_Config.landVehicleIdles : g_Config.waterVehicleIdles;
				PlayAnimation(player, anim, 0);
				g_VehicleAnim.idle = true;
				g_VehicleAnim.active = false;
			}			
		}
    }
}

void CheckNPCVehicleAnimations(void** pData, int count)
{
    unsigned long now = GetTickCount();

    if (now - g_LastNPCAnimCheck < NPC_ANIM_INTERVAL)
        return;

    g_LastNPCAnimCheck = now;

    if (!pData || count <= 0)
        return;

    // Main Character Detector
    CharacterObject* mainChar = GetPlayer();

    for (int i = 0; i < count && i < MAX_NPCS; i++)
    {
        CharacterObject* npc = (CharacterObject*)pData[i];

        if (!npc || npc == mainChar)
            continue;

        bool isBoat = NPC_IsInBoat(npc), 
			 isCar = NPC_IsInCar(npc);

        if (!isBoat && !isCar)
            continue;

        int seatingPosition = Get_Vehicle_State(npc), 
			Weapon_State = Get_Weapon_State(npc), 
			Reversing = Land_Vehicle_Reverse_Driving(npc), 
			Shooting = Vehicle_Shooting(npc),
			currentVocalMS = (Current_Voice_MS(npc) != -1);
			
		bool refreshFunction = (seatingPosition != 1 || Weapon_State || Shooting || currentVocalMS || g_NPCDamageStates[i].damageTaken);

        if (refreshFunction)
        {
            g_NPCStates[i].Reset();
            continue;
        }

        __try
        {
            NPCAnimState& state = g_NPCStates[i];
			float curX = *(float*)((uintptr_t)npc + 0x34 + 48), 
				  curZ = *(float*)((uintptr_t)npc + 0x34 + 56), 
				  dx = curX - state.lastX, 
				  dz = curZ - state.lastZ;
            
			bool isMoving = ((dx * dx + dz * dz) > 0.01f);
            state.lastX = curX;
            state.lastZ = curZ;

            if (isMoving)
            {
                if (!state.activePlayed)
                {
                    if (!isBoat && Reversing && isCar)
					{
						const std::string& anim = g_Config.landVehicleDriverReverse;
						PlayAnimation(npc, anim, 0);
						state.activePlayed = true;
						state.idlePlayed = false;
					}
					
					else
					{
						const std::string& anim = isBoat ? g_Config.waterVehicleDriver : g_Config.landVehicleDriver;
						PlayAnimation(npc, anim, 0);
						state.activePlayed = true;
						state.idlePlayed = false;	
					}
                }
            }
            
            else
            {
                if (!state.idlePlayed)
                {
                    const std::string& anim = isBoat ? g_Config.waterVehicleIdles : g_Config.landVehicleIdles;
                    PlayAnimation(npc, anim, 0);
                    state.idlePlayed = true;
                    state.activePlayed = false;
                }
            }
        }
        
        __except(EXCEPTION_EXECUTE_HANDLER) 
        { 
            continue; 
        }
    }
} 

// Vehicle Damage Animation Player
void ProcessDamageAnim(CharacterObject* character, DamageAnimState& state, bool isBoat)
{
    int currentHealth = GetCharacterHealth(character), 
		Seat = Get_Vehicle_State(character), 
		Reversing = Land_Vehicle_Reverse_Driving(character);

    if (currentHealth <= 0)
        return;

    if (state.lastHealth == -1)
    {
        state.lastHealth = currentHealth;
        return;
    }

    bool healthDropped = (currentHealth < state.lastHealth), 
		 stillAlive = (currentHealth > 1);

    if (healthDropped && stillAlive)
        state.animPlayed = false;

    if (!state.animPlayed && healthDropped && stillAlive)
    {		
		state.damageTaken = true;
		state.lastTimer = GetTickCount();
		
		if (Seat)
		{
			if (!isBoat && Reversing)
			{
				const std::string& anim = g_Config.landVehicleDamageDriverReverse;
				PlayAnimation(character, anim, 0);
				state.animPlayed = true;
			}
			
			else
			{
				const std::string& anim = isBoat ? g_Config.waterVehicleDamageDriver : g_Config.landVehicleDamageDriver;
				PlayAnimation(character, anim, 0);
				state.animPlayed = true;
			}
		}
	
		else
		{
			const std::string& anim = isBoat ? g_Config.waterVehicleDamagePassenger : g_Config.landVehicleDamagePassenger;
			PlayAnimation(character, anim, 0);
			state.animPlayed = true;
		}
    }

    state.lastHealth = currentHealth;
}

void CheckNPCVehicleDamage(void** pData, int count)
{
    if (!pData || count <= 0)
        return;

    for (int i = 0; i < count && i < MAX_NPC_DAMAGE; i++)
    {
        CharacterObject* npc = (CharacterObject*)pData[i];

        if (!npc)
            continue;

        bool isBoat = NPC_IsInBoat(npc), 
			 Vehicle_Detected = isBoat || NPC_IsInCar(npc);
        
		int Injuries_50_Caliber = Get_50_Calibers(npc), 
			Driver_Passenger = Actual_Vehicle_Intention(npc), 
			Weapon_Damage = Damage_Weapon(npc);

        if (!Vehicle_Detected || Injuries_50_Caliber || !Driver_Passenger || Weapon_Damage == -1)
        {
            g_NPCDamageStates[i].Reset();
            continue;
        }

        ProcessDamageAnim(npc, g_NPCDamageStates[i], isBoat);
    }
} 

void Damages_50_Calibers(void** pData, int count)
{
    if (!pData || count <= 0)
        return;

    for (int i = 0; i < count && i < MAX_NPC_50CAL; i++)
    {
        CharacterObject* npc = (CharacterObject*)pData[i];

        if (!npc)
            continue;

        int get_50_calibers = Get_50_Calibers(npc);

        if (!get_50_calibers)
        {
            g_NPC50CalStates[i].Reset();
            continue;
        }

        int currentHealth = GetCharacterHealth(npc);

        if (currentHealth <= 0)
            continue;

        DamageAnimState& state = g_NPC50CalStates[i];

        // First Tick Initialisation
        if (state.lastHealth == -1)
        {
            state.lastHealth = currentHealth;
            continue;
        }

        bool healthDropped = (currentHealth < state.lastHealth), 
			 stillAlive = (currentHealth > 1);

        if (healthDropped && stillAlive)
            state.animPlayed = false;

        if (!state.animPlayed && healthDropped && stillAlive)
        {
            PlayAnimation(npc, g_Config.Damage_50_Calibers, 0);
            state.animPlayed = true;
        }

        state.lastHealth = currentHealth;
    }
}

void Vehicle_Character_Animation_Reset_Fix(void** pData, int count)
{
	if (!pData || count <= 0)
        return;
	
	for (int i = 0; i < count; i++)
	{
		CharacterObject* character = (CharacterObject*)pData[i];

        if (!character)
            continue;
		
		bool Inside_Water_Vehicle = NPC_IsInBoat(character), 
			 Inside_Land_Vehicle = NPC_IsInCar(character), 
			 Inside_Vehicle = (Inside_Water_Vehicle || Inside_Land_Vehicle);
		
		if (!Inside_Vehicle)
			continue;
		
		int animationRequestID = Get_Animation_Request_ID(character);
		
		if (animationRequestID)
			continue;
		
		bool reset = (!animationRequestID), 
			 landVehicleDriverReversing = Land_Vehicle_Reverse_Driving(character), 
			 driver = Get_Vehicle_State(character), 
			 passenger = Get_Vehicle_Passenger_State(character);
		
		if (reset)
		{
			if (driver)
			{
				if (Inside_Land_Vehicle)
				{
					const std::string& anim = landVehicleDriverReversing ? g_Config.landVehicleDriverReverse : g_Config.landVehicleDriver;
					PlayAnimation(character, anim, 0);
				}
				
				if (Inside_Water_Vehicle)
				{
					const std::string& anim = g_Config.waterVehicleDriver;
					PlayAnimation(character, anim, 0);
				}
			}
			
			if (passenger)
			{
				if (Inside_Land_Vehicle)
				{
					const std::string& anim = g_Config.landVehicleIdles;
					PlayAnimation(character, anim, 0);
				}
				
				if (Inside_Water_Vehicle)
				{
					const std::string& anim = g_Config.waterVehiclePassenger;
					PlayAnimation(character, anim, 0);
				}
			}
		}
	}
}

// ScarfaceEX Features' Validation Tracker
bool ScarfaceEX_Trigger_Valid(CharacterObject* p)
{
	bool scarfaceEXTriggerValid = false;
	
	__try
	{
		float currentX = *(float*)((uintptr_t)p + 0x34 + 48), 
			  currentY = *(float*)((uintptr_t)p + 0x34 + 52), 
			  currentZ = *(float*)((uintptr_t)p + 0x34 + 56);
		
		bool Main_Menu = ((currentX > -3500.0f && currentX < -3400.0f) && (currentY > 19.0f && currentY < 20.0f) && (currentZ > -1800.0f && currentZ < -1700.0f)),
			 currentNIS = Is_NIS_Active(), 
			 currentGamePaused = Is_Game_Paused_HUD(), 
			 currentScreenFXTransition = Get_Screen_Effects_Transition_Blacked_Out(), 
			 currentTeleportation = Get_Is_Teleporting(), 
			 currentDeathStatus = Main_Character_Death_Status(),
			 externalScriptsInitialised = false;
			 
		externalScriptsInitialised = Initialise_Run_Script() && !currentNIS && !currentGamePaused && !currentDeathStatus && !currentTeleportation;
		externalScriptsInitialised = !currentScreenFXTransition && !External_Scripts_Initialised && externalScriptsInitialised;
			 
		if (externalScriptsInitialised)
		{
			RunScript(External_Scripts.c_str());
			External_Scripts_Initialised = true;
		}
		
		scarfaceEXTriggerValid = (!currentNIS && !currentGamePaused && !currentScreenFXTransition && !currentTeleportation && !currentDeathStatus && !Main_Menu);
	}
	
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		scarfaceEXTriggerValid = false;
	}
	
	if (!scarfaceEXTriggerValid)
	{
		DPCBS_Struct.blockedTimer = GetTickCount();
		DPCBS_Struct.blockedFlag = true;
	}
	
	return scarfaceEXTriggerValid;
}

void Reset_Action_Map()
{
	unsigned long currentTimer = GetTickCount();
	bool currentGlobalSoundMS = (GlobalSoundGetCurrentMs() == -1),
		 resetActionMap = (((currentTimer - DPCBS_Struct.blockedTimer) > 12357UL) && currentGlobalSoundMS && DPCBS_Struct.blockedFlag),
		 resetTimer = (DPCBS_Struct.blockedFlag && !currentGlobalSoundMS);
		 
	if (resetTimer)
		DPCBS_Struct.blockedTimer = currentTimer;
		 
	if (resetActionMap)
		DPCBS_Struct.blockedFlag = false;
}

// Weapon Switching Animations
void Characters_Changing_Weapons_Function(void** pData, int count)
{
	if (!pData || count <= 0)
        return;
	
	for (int i = 0; i < count && i < MAX_CHARACTERS; i++)
    {
        CharacterObject* characters = (CharacterObject*)pData[i];

        if (!characters)
            continue;
		
		int currentWeapon = Get_Current_Drawn_Weapon(characters),
			currentLocomotionRingType = Current_Locomotion_Ring_Type(characters);
			
		bool triggerInvalid0 = (NPC_IsInBoat(characters) || NPC_IsInCar(characters) || Gun_Up_State(characters) || Crouch_State(characters)),
			 triggerInvalid1 = (!currentWeapon || Dead_Body_Tracker(characters) || (currentLocomotionRingType > 1) || Get_Weapon_State(characters)),
			 refreshFunction = (triggerInvalid0 || triggerInvalid1 || Get_Animation_Request_ID(characters)),
			 weaponChanged = ((currentWeapon != CHWS_Struct[i].lastWeapon) && (currentWeapon != 0));
			 
		if (refreshFunction)
		{
			CHWS_Struct[i].lastWeapon = currentWeapon;
			continue;
		}
		
		if (weaponChanged)
			PlayAnimation(characters, g_Config.SwitchingWeapons, 67);
		
		CHWS_Struct[i].lastWeapon = currentWeapon;
	}
}

void Characters_Speaking_Inside_Vehicles_Function(void** pData, int count)
{
	if (!pData || count <= 0)
        return;
	
	unsigned long now = GetTickCount();
	
	for (int i = 0; i < count && i < MAX_SPEAKERS; i++)
    {
        CharacterObject* characters = (CharacterObject*)pData[i];

        if (!characters)
            continue;
	
		bool Inside_Water_Vehicle = NPC_IsInBoat(characters), 
			 Inside_Land_Vehicle = NPC_IsInCar(characters), 
			 Inside_Vehicle = (Inside_Water_Vehicle || Inside_Land_Vehicle);
		
		if (!Inside_Vehicle)
			continue;
		
		bool landVehicleDriverReversing = Land_Vehicle_Reverse_Driving(characters), 
			 driver = Get_Vehicle_State(characters), 
			 passenger = Get_Vehicle_Passenger_State(characters),
			 currentVocalMS = (Current_Voice_MS(characters) == -1),
			 timerValid = g_NPCDamageStates[i].damageTaken ? ((now - g_NPCDamageStates[i].lastTimer) > 2753UL) : true;
			 
		if (timerValid)
			g_NPCDamageStates[i].damageTaken = false;
		
		bool triggerValid = (CSIVS_Struct[i].lastVocalMS && !currentVocalMS && timerValid);
			 
		if (triggerValid)
		{
			if (driver)
			{
				if (Inside_Land_Vehicle)
				{
					const std::string& anim = landVehicleDriverReversing ? g_Config.landVehicleDriverReverseTalking : g_Config.landVehicleDriverTalking;
					PlayAnimation(characters, anim, 0);
				}
				
				if (Inside_Water_Vehicle)
				{
					const std::string& anim = g_Config.waterVehicleDriverTalking;
					PlayAnimation(characters, anim, 0);
				}
			}
			
			if (passenger)
			{
				if (Inside_Land_Vehicle)
				{
					const std::string& anim = g_Config.landVehiclePassengerTalking;
					PlayAnimation(characters, anim, 0);
				}
				
				if (Inside_Water_Vehicle)
				{
					const std::string& anim = g_Config.waterVehiclePassengerTalking;
					PlayAnimation(characters, anim, 0);
				}
			}
		}
		
		CSIVS_Struct[i].lastVocalMS = currentVocalMS;
	}
}

// Main Input Monitoring Thread
DWORD WINAPI InputThread(LPVOID lpParam) 
{
    InitDirectInput(); // Initialize DirectInput
    
    while (g_Running) 
	{
        Sleep(10);
        CharacterObject* p = GetPlayer();
        
        if (!p) 
            continue;
		
        unsigned long now = GetTickCount();
		bool Scarface_EX_Trigger_Valid = ScarfaceEX_Trigger_Valid(p);
		
		if (!Scarface_EX_Trigger_Valid)
		{
			if (DPCBS_Struct.blockedFlag)
				DPCBS_Struct.blockedTimer = GetTickCount();
			
			continue;
		}
		
		// Global Trackers
		int npcCount = 0, 
			Shooting = Vehicle_Shooting(p), 
			Seating_Position = Get_Vehicle_State(p), 
			Weapon_State = Get_Weapon_State(p),
			currentReputationLevel = DB_Get_Reputation_Level_Wrapper();
			
		std::string currentPackage = CVM_Get_Main_Character_Package_Wrapper();
		void** npcList = GetNPCList(&npcCount);
		bool currentMission = Get_Mission_Active(), 
			 Dodges_Trigger_Valid = ((now - g_LastDodgeTime) >= g_Config.cooldown),
			 VehicleIdleStatesTriggerValid = (Seating_Position && !Shooting && !Weapon_State),
			 phoneCallsBlockedInvalid = (DPCBS_Struct.blockedFlag && !currentMission);
		
		if (currentMission)
		{
			DPCBS_Struct.blockedFlag = true;
			DPCBS_Struct.blockedTimer = GetTickCount();
		}
		
		if (phoneCallsBlockedInvalid)
			Reset_Action_Map();
		
		if (Dodges_Trigger_Valid) 
        {
            CheckKeyboard(p, now);
            CheckController(p, now);
            CheckDirectInput(p, now);
        }
		
		if (VehicleIdleStatesTriggerValid)
			CheckVehicleAnimation(p);
		
		Damages_50_Calibers(npcList, npcCount);
		CheckNPCVehicleDamage(npcList, npcCount);
		CheckNPCVehicleAnimations(npcList, npcCount);
		Vehicle_Character_Animation_Reset_Fix(npcList, npcCount);
		Characters_Changing_Weapons_Function(npcList, npcCount);
		Characters_Speaking_Inside_Vehicles_Function(npcList, npcCount);
    } 
	
	CleanupDirectInput();

    return 0;
}

// DLL Entry Point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) 
{
    if (reason == DLL_PROCESS_ATTACH) 
	{
        DisableThreadLibraryCalls(hModule);
		
		// Load Configuration
		LoadConfig();
		DPCBS_Struct.Reset();

		// Characters' States' Initialisation
		for (int i = 0; i < MAX_NPCS; i++)       
			g_NPCStates[i].Reset();
		
		for (int i = 0; i < MAX_NPC_DAMAGE; i++)  
			g_NPCDamageStates[i].Reset();
		
		for (int i = 0; i < MAX_NPC_50CAL; i++)   
			g_NPC50CalStates[i].Reset();
		
		for (int i = 0; i < MAX_CHARACTERS; i++)
			CHWS_Struct[i].Reset();
		
		for (int i = 0; i < MAX_SPEAKERS; i++)
			CSIVS_Struct[i].Reset();
		
		// Initialize Dodges & Evades system; First Person Camera system; Characters' Tracking System
		bool dodgesOK = InitPlayerPointer() && InitPlayAnimation(),
			 fpsOK = InitGetBonePosition() && InitCameraSetPosition(),
			 npcOK = InitCVManager(), 
			 scriptOK = Initialise_Run_Script();

		// Install Camera Hook If 1st Person System Initialized Successfully
		if (fpsOK)
			InstallCameraHook();

		// Start Input Thread If At Least 1 System Initialized
		if (dodgesOK || fpsOK || npcOK || scriptOK)
			g_InputThreadHandle = CreateThread(NULL, 0, InputThread, NULL, 0, NULL);
    }
    
	if (reason == DLL_PROCESS_DETACH) 
	{
		// Signal Thread To Stop
        g_Running = false;                                             
        
        // Wait For Thread To Finish
        if (g_InputThreadHandle) 
		{
			// Wait Maximum 1 Second
            WaitForSingleObject(g_InputThreadHandle, 1000);            
			CloseHandle(g_InputThreadHandle);
            g_InputThreadHandle = NULL;
        }
	}
    
	return TRUE;
}