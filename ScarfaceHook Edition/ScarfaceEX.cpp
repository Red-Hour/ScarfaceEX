#include <Windows.h>
#include <Psapi.h>
#include <Xinput.h>
#include <dinput.h>
#include <fstream>
#include <string>
#include <sstream>
#include <unordered_map>

#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "Xinput.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

#define DIRECTINPUT_VERSION 0x0800
#define ADDR_VehicleState 0x007BC51C
#define CONFIG_FILE "ScarfaceEX_Configuration.ini"
#define README_FILE "ScarfaceEX_Instructions.txt"

typedef void* CharacterObject;
typedef void (__cdecl *Run_Script_Function)(const char*, int, int, int, int);
typedef int (__thiscall *PlayAnimationFunc)(CharacterObject*, unsigned int*, int, int);

//std::ofstream debugFile("ScarfaceEX_Debug.txt", std::ios::app);

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

// Loaded Script
std::string External_Scripts = Load_Script("ScarfaceEX_External_Scripts.txt");

// Characters Speaking Inside Vehicles Tracker
struct Characters_Speaking_Inside_Vehicles_Structure
{
	int animationRequestID;
	bool lastVocalMS,
		 triggerInvalid;
		 
	void Reset()
	{
		lastVocalMS = false;
		triggerInvalid = false;
		animationRequestID = -1;
	}
};

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
				landVehicleDriverTalking,
				landVehicleDriverReverseTalking,
				landVehiclePassengerTalking,
				waterVehicleDriverTalking,
				waterVehiclePassengerTalking,
				copBribeAnimation,
				gangBribeAnimation,
				backupAnimation,
				steerForwardReload,
				steerReverseReload,
				tMVehicleCarSeatPassReload,
				boatSteerGenericReload,
				passSitReload;
    
	// Press Mode \ Toggles
	int pressMode,
		disableEveryEnhancement,
		disableVehicleReloads,
		disableVehicleTalking,
		disableVehicleDamage,
		disableDodgesAndPhoneCalls,
		disableVehicleIdles;

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

// Characters' Weapon State Detector
int Get_Weapon_State(CharacterObject* npc)
{
	__try
	{
		return (*(int*)((uintptr_t)npc + 0x2AC + 0xC));
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
		return ((*(int*)((uintptr_t)characters + 0x2E8 + 0x8)) == 3) ? 1 : 0;
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
		return ((*(int*)((uintptr_t)npc + 0x2E8 + 0x8)) == 6) ? 1 : 0;
	}
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return 0; 
	}
}

// Vehicle Damage Animation Tracking
struct DamageAnimState
{
    int lastHealth,
		lastAnimationRequestID;
		
    bool animPlayed,
		 triggerInvalid; 

    void Reset()
    {
        lastHealth = -1; 
		lastAnimationRequestID = -1;
        animPlayed = false;
		triggerInvalid = false;
    }
};

// 50 Caliber Damage Animation Tracking (uses DamageAnimState; stored per character below)

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
};

// Per-Character Tracking State [keyed by the character object, NOT by its slot in the characters' list]
struct CharacterTrackState
{
    CharacterObject* owner;
    unsigned long stamp,
				  lastSeenTick;

    DamageAnimState damage,
					cal50;
    
	NPCAnimState move;
    Characters_Speaking_Inside_Vehicles_Structure speak;

    CharacterTrackState() 
	{ 
		Reset(); 
	}

    void Reset()
    {
        owner = NULL;
        stamp = 0;
        lastSeenTick = 0;
        damage.Reset();
        cal50.Reset();
        move.Reset();
        speak.Reset();
    }
};

static std::unordered_map<CharacterObject*, CharacterTrackState> g_CharStates;
static unsigned long g_TrackerTick = 0;

// Unique Character Stamp: Fill With A Spawn Counter \ Unique ID \ Name Hash If CharacterObject Has One
unsigned long GetCharacterStamp(CharacterObject* c)
{
    return 0;
}

CharacterTrackState& GetCharacterState(CharacterObject* c)
{
    unsigned long stamp = GetCharacterStamp(c);
    CharacterTrackState& st = g_CharStates[c];

    // New entry, or the address was recycled by a different character
    if (st.owner != c || st.stamp != stamp)
    {
        st.Reset();
        st.owner = c;
        st.stamp = stamp;
    }

    st.lastSeenTick = g_TrackerTick;
    
	return st;
}

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
int lastAnimationRequestID = -1;
bool External_Scripts_Initialised = false,
	 lastVehicleReload = false;

unsigned long g_LastDodgeTime = 0,
			  extendedFeaturesTimer = 0,
			  vehicleReloadsTimer = 0;

uintptr_t g_PlayerPointerAddress = 0, 
		  g_CVManagerAddress = 0;

PlayAnimationFunc g_PlayAnimationFunc = NULL;
Run_Script_Function Run_Script_Pointer = NULL;
uintptr_t Run_Script_Address = 0;

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
int ParseKey(const std::string& name) 
{
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
	g_Config.steerForwardReload = "Steer_Forward_Reload";
	g_Config.steerReverseReload = "Steer_Reverse_Reload";
	g_Config.tMVehicleCarSeatPassReload = "TM_Vehicle_Car_Seat_Pass_Reload";
	g_Config.boatSteerGenericReload = "Boat_Steer_Generic_Reload";
	g_Config.passSitReload = "Pass_Sit_Reload";
    
    // Toggles \ Dodge Mechanics Adjustments
	g_Config.pressMode = 2;
    g_Config.pressWindow = 210;
    g_Config.cooldown = 753;
	g_Config.disableDodgesAndPhoneCalls = 0;
	g_Config.disableEveryEnhancement = 0;
	g_Config.disableVehicleDamage = 0;
	g_Config.disableVehicleIdles = 0;
	g_Config.disableVehicleReloads = 0;
	g_Config.disableVehicleTalking = 0;
	
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
		outFile << "Cooldown (milliseconds)- Success input block time.";
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
        outFile << "Right_Peek_Shoot_Animation=Right_Peek_Shoot\n\n";
		outFile << "[Behavior]\n\n";
		outFile << "Disable_Dodges_And_Phone_Calls=0\n";
		outFile << "Disable_Every_Enhancement=0\n";
		outFile << "Disable_Vehicle_Reloads=0\n";
		outFile << "Disable_Vehicle_Talking=0\n";
		outFile << "Disable_Vehicle_Damage=0\n";
		outFile << "Disable_Vehicle_Idles=0\n";
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
		outFile << "Damage_50_Calibers=Damage_50_Calibers\n";
		outFile << "Steer_Forward_Reload=Steer_Forward_Reload\n";
		outFile << "Steer_Reverse_Reload=Steer_Reverse_Reload\n";
		outFile << "TM_Vehicle_Car_Seat_Pass_Reload=TM_Vehicle_Car_Seat_Pass_Reload\n";
		outFile << "Boat_Steer_Generic_Reload=Boat_Steer_Generic_Reload\n";
		outFile << "Pass_Sit_Reload=Pass_Sit_Reload";
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
            
			if (key == "Cop_Bribe_Animation" && !value.empty()) 
				g_Config.copBribeAnimation = value; 
            
			if (key == "Gang_Bribe_Animation" && !value.empty()) 
				g_Config.gangBribeAnimation = value; 
            
			if (key == "Backup_Animation" && !value.empty()) 
				g_Config.backupAnimation = value; 
        }
		
		if (section == "Behavior") 
		{
            if (key == "Disable_Dodges_And_Phone_Calls")
				g_Config.disableDodgesAndPhoneCalls = (ParseInt(value, 0) != 0);
			
			if (key == "Disable_Every_Enhancement")
				g_Config.disableEveryEnhancement = (ParseInt(value, 0) != 0);
			
			if (key == "Disable_Vehicle_Damage")
				g_Config.disableVehicleDamage = (ParseInt(value, 0) != 0);
			
			if (key == "Disable_Vehicle_Idles")
				g_Config.disableVehicleIdles = (ParseInt(value, 0) != 0);
			
			if (key == "Disable_Vehicle_Reloads")
				g_Config.disableVehicleReloads = (ParseInt(value, 0) != 0);
			
			if (key == "Disable_Vehicle_Talking")
				g_Config.disableVehicleTalking = (ParseInt(value, 0) != 0);
			
			if (key == "Press_Mode") 
				g_Config.pressMode = ParseInt(value, 1);
            
			if (key == "Press_Window") 
				g_Config.pressWindow = ParseInt(value, 300);
            
			if (key == "Cooldown") 
				g_Config.cooldown = ParseInt(value, 500);
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

			if (key == "Steer_Forward_Reload" && !value.empty())
				g_Config.steerForwardReload = value;

			if (key == "Steer_Reverse_Reload" && !value.empty())
				g_Config.steerReverseReload = value;

			if (key == "TM_Vehicle_Car_Seat_Pass_Reload" && !value.empty())
				g_Config.tMVehicleCarSeatPassReload = value;

			if (key == "Boat_Steer_Generic_Reload" && !value.empty())
				g_Config.boatSteerGenericReload = value;

			if (key == "Pass_Sit_Reload" && !value.empty())
				g_Config.passSitReload = value;
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
bool Action_Map() 
{
    __try 
	{
		return ((*(int*)ADDR_VehicleState) != 0);
    } 
	
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{
        return false;
    }
} 

// Main Character Alive \ Dead Tracker
bool Is_Main_Character_Killed_Or_Flagged_For_On_Killed()
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
        result = false;
    }

    return result;
} 

// Non-Interactive Sequence Tracker
bool NIS_Is_Playing()
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
        result = false;
    }

    return result;
}

// Pause Menu Tracker
bool HUD_Is_Game_Paused()
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
        result = false;
    }

    return result;
} 

// Screen Transition Tracker
bool Screen_Fx_Transition_Is_Blacked_Out()
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
        result = false;
    }

    return result;
}

// Player Teleportation Tracker
bool Is_Teleporting()
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
        result = false;
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

// Gun Aiming Tracker
bool Gun_Up_State(CharacterObject* character)
{
	__try
	{
		return (*(int*)((uintptr_t)character + 0x380 + 0x4));
	}
	
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
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
        result = -1;
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
        return (*(int*)((uintptr_t)character + 0x2E8 + 0xC));
    }
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return 0; 
	}
} 

// Characters' Vehicle Shooting Detector
int Vehicle_Shooting(CharacterObject* character)
{
    int vehicleShooting = -1;
	
	__try
    {
        vehicleShooting = *(int*)((uintptr_t)character + 0x2E8 + 0x8);
		vehicleShooting = (vehicleShooting == 1 || vehicleShooting == 5) ? 1 : 0;
    }
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		vehicleShooting = 0;
	}
	
	return vehicleShooting;
} 

// Characters' Vehicle Intention Detector
int Actual_Vehicle_Intention(CharacterObject* character)
{
    int actualVehicleIntention = -1;
	
	__try
    {
        actualVehicleIntention = *(int*)((uintptr_t)character + 0x2E8);
		actualVehicleIntention = (actualVehicleIntention > 2 && actualVehicleIntention < 6) ? 1 : 0;
    }
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		actualVehicleIntention = 0;
	}
	
	return actualVehicleIntention;
} 

// Characters' Passenger Vehicle State Detector
int Get_Vehicle_Passenger_State(CharacterObject* npc)
{
    __try
    {
		return ((*(int*)((uintptr_t)npc + 0x2E8)) == 4) ? 1 : 0;
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
        return ((*(int*)((uintptr_t)npc + 0x2E8)) == 3) ? 1 : 0;
    }
    
	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return 0; 
	}
}

// Characters' Vehicle Types' Detector
bool Is_Character_In_Car(CharacterObject* npc)
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
		result = false; 
	}

    return result;
}

bool Is_Character_In_Boat(CharacterObject* npc)
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
		result = false; 
	}

    return result;
}

// Current Voice Timespan of Characters
int Get_Remaining_Vocal_Ms(CharacterObject* character)
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
		result = -1; 
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
		result = -1; 
	}

    return result;
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
    bool ActionMap = Action_Map() || Is_Character_In_Boat(player) || Is_Character_In_Car(player),
		 dodgesTriggerValid = (animName != "Cop_Bribe") && (animName != "Gang_Bribe") && (animName != "Backup"),
		 copBribeTriggerValid = (animName == "Cop_Bribe"),
		 gangBribeTriggerValid = (animName == "Gang_Bribe"),
		 backupTriggerValid = (animName == "Backup");
		 
	if (ActionMap)
		return;
	
	if (dodgesTriggerValid)
		PlayAnimation(player, animName, 67);
	
	if (copBribeTriggerValid)
		RunScript("Phone_Calls_Cop_Bribe(); return;");
	
	if (gangBribeTriggerValid)
		RunScript("Phone_Calls_Gang_Bribe(); return;");
	
	if (backupTriggerValid)
		RunScript("Phone_Calls_Backup(); return;");
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
bool IsBinaryInputActive()
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
        }
    }

    return false;
}

void VehicleReloads(CharacterObject* player)
{
	int vehicleState = *(int*)ADDR_VehicleState, 
		reversing = Land_Vehicle_Reverse_Driving(player),
		passenger = Get_Vehicle_Passenger_State(player),
		currentAnimationRequestID = Get_Animation_Request_ID(player);
		
	bool weaponStateReloading = (Get_Weapon_State(player) == 5) && !lastVehicleReload,
		 resetReloading = (lastAnimationRequestID != currentAnimationRequestID) && lastVehicleReload;
	
	if (vehicleState <= 0 || vehicleState > 2)
	{
		lastVehicleReload = false;
		lastAnimationRequestID = -1;
		
		return;
	}
	
	if (weaponStateReloading)
	{
		if (passenger)
		{
			if (vehicleState == 1)
			{
				PlayAnimation(player, g_Config.tMVehicleCarSeatPassReload, 0);
				
				lastVehicleReload = true;
				lastAnimationRequestID = Get_Animation_Request_ID(player);
			}
			
			if (vehicleState == 2)
			{
				PlayAnimation(player, g_Config.passSitReload, 0);
				
				lastVehicleReload = true;
				lastAnimationRequestID = Get_Animation_Request_ID(player);
			}
		}
		
		else
		{
			if (vehicleState == 1)
			{
				if (reversing)
				{
					PlayAnimation(player, g_Config.steerReverseReload, 0);
					
					lastVehicleReload = true;
					lastAnimationRequestID = Get_Animation_Request_ID(player);
				}
				
				else
				{
					PlayAnimation(player, g_Config.steerForwardReload, 0);
					
					lastVehicleReload = true;
					lastAnimationRequestID = Get_Animation_Request_ID(player);
				}
			}
			
			if (vehicleState == 2)
			{
				PlayAnimation(player, g_Config.boatSteerGenericReload, 0);
				
				lastVehicleReload = true;
				lastAnimationRequestID = Get_Animation_Request_ID(player);
			}
		}
	}
	
	if (resetReloading)
		lastVehicleReload = false;
}

void CheckVehicleAnimation(CharacterObject* player)
{
    int vehicleState = *(int*)ADDR_VehicleState, 
		Reversing = Land_Vehicle_Reverse_Driving(player);
	
	unsigned long now = GetTickCount();
	
	if (vehicleState <= 0 || vehicleState > 2 || !Get_Vehicle_State(player) || Vehicle_Shooting(player) || Get_Weapon_State(player) || lastVehicleReload)
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

    bool inputActive = IsBinaryInputActive();
	
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
				g_VehicleAnim.active = true;
				g_VehicleAnim.idle = false;
				
				PlayAnimation(player, anim, 0);
			}
			
			else
			{
				const std::string& anim = (vehicleState == 1) ? g_Config.landVehicleDriver : g_Config.waterVehicleDriver;
				g_VehicleAnim.active = true;
				g_VehicleAnim.idle = false;
				
				PlayAnimation(player, anim, 0);
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
				g_VehicleAnim.idle = true;
				g_VehicleAnim.active = false;
				
				PlayAnimation(player, anim, 0);
			}			
		}
    }
}

struct VehicleFrameContext
{
    int vehicleSeatState,
		health;
	
	bool isBoat,
		 isCar,
		 insideVehicle,
		 passenger,
		 reversing;
};

VehicleFrameContext BuildVehicleFrameContext(CharacterObject* character)
{
    VehicleFrameContext ctx;
    ctx.isBoat = Is_Character_In_Boat(character);
    ctx.isCar = Is_Character_In_Car(character);
    ctx.insideVehicle = (ctx.isBoat || ctx.isCar);
    ctx.vehicleSeatState = Get_Vehicle_State(character);
    ctx.passenger = Get_Vehicle_Passenger_State(character);
    ctx.reversing = Land_Vehicle_Reverse_Driving(character);
    ctx.health = GetCharacterHealth(character);
    
	return ctx;
}

void ProcessDamageAnim(CharacterObject* character, DamageAnimState& state, const VehicleFrameContext& ctx, unsigned long now)
{
    if (ctx.health <= 0)
        return;

    if (state.lastHealth == -1)
    {
        state.lastHealth = ctx.health;
        
		return;
    }

    bool healthDropped = (ctx.health < state.lastHealth),
         stillAlive = (ctx.health > 1);

    if (healthDropped && stillAlive)
        state.animPlayed = false;

    if (!state.animPlayed && healthDropped && stillAlive && !state.triggerInvalid)
    {
        if (ctx.vehicleSeatState)
        {
            if (!ctx.isBoat && ctx.reversing)
            {
                state.animPlayed = true;
				state.triggerInvalid = true;
				const std::string& anim = g_Config.landVehicleDamageDriverReverse;
                
				PlayAnimation(character, anim, 0);
			
				state.lastAnimationRequestID = Get_Animation_Request_ID(character);
            }

            else
            {
                state.animPlayed = true;
				state.triggerInvalid = true;
				const std::string& anim = ctx.isBoat ? g_Config.waterVehicleDamageDriver : g_Config.landVehicleDamageDriver;
                
				PlayAnimation(character, anim, 0);
			
				state.lastAnimationRequestID = Get_Animation_Request_ID(character);
            }
        }

        else
        {
            state.animPlayed = true;
			state.triggerInvalid = true;
			const std::string& anim = ctx.isBoat ? g_Config.waterVehicleDamagePassenger : g_Config.landVehicleDamagePassenger;
                
			PlayAnimation(character, anim, 0);
			
			state.lastAnimationRequestID = Get_Animation_Request_ID(character);
        }
    }
	
	int currentAnimationRequestID = Get_Animation_Request_ID(character);
	
	if (state.triggerInvalid && (currentAnimationRequestID != state.lastAnimationRequestID))
		state.triggerInvalid = false;

    state.lastHealth = ctx.health;
}

void NPCVehicleDamageCheck(CharacterObject* npc, CharacterTrackState& st, const VehicleFrameContext& ctx, unsigned long now)
{
    if (!npc)
        return;
	
	bool refreshFunction = !ctx.insideVehicle || Get_50_Calibers(npc) || !Actual_Vehicle_Intention(npc);

    if (refreshFunction)
    {
        st.damage.Reset();
        
		return;
    }

    ProcessDamageAnim(npc, st.damage, ctx, now);
}

void NPC50CalDamageCheck(CharacterObject* npc, CharacterTrackState& st, const VehicleFrameContext& ctx)
{
    if (!npc)
        return;

    if (!Get_50_Calibers(npc))
    {
        st.cal50.Reset();
        
		return;
    }

    if (ctx.health <= 0)
        return;

    DamageAnimState& state = st.cal50;

    if (state.lastHealth == -1)
    {
        state.lastHealth = ctx.health;
        
		return;
    }

    bool healthDropped = (ctx.health < state.lastHealth),
         stillAlive = (ctx.health > 1);

    if (healthDropped && stillAlive)
        state.animPlayed = false;

    if (!state.animPlayed && healthDropped && stillAlive && !state.triggerInvalid)
    {
        PlayAnimation(npc, g_Config.Damage_50_Calibers, 0);
		
		state.lastAnimationRequestID = Get_Animation_Request_ID(npc);
		state.animPlayed = true;
		state.triggerInvalid = true;
    }
	
	int currentAnimationRequestID = Get_Animation_Request_ID(npc);
	
	if (state.triggerInvalid && (currentAnimationRequestID != state.lastAnimationRequestID))
		state.triggerInvalid = false;

    state.lastHealth = ctx.health;
}

void NPCVehicleAnimationCheck(CharacterObject* npc, CharacterTrackState& st, const VehicleFrameContext& ctx)
{
    if (!npc)
        return;

    if (!ctx.isBoat && !ctx.isCar)
        return;

    bool refreshFunction = (Get_Remaining_Vocal_Ms(npc) != -1) || Get_Weapon_State(npc) || Vehicle_Shooting(npc) || (ctx.vehicleSeatState != 1);

    if (refreshFunction)
    {
        st.move.Reset();
        
		return;
    }

    __try
    {
        NPCAnimState& state = st.move;
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
                if (!ctx.isBoat && ctx.reversing && ctx.isCar)
                {
                    const std::string& anim = g_Config.landVehicleDriverReverse;
                    state.activePlayed = true;
                    state.idlePlayed = false;
                    
					PlayAnimation(npc, anim, 0);
                }

                else
                {
                    const std::string& anim = ctx.isBoat ? g_Config.waterVehicleDriver : g_Config.landVehicleDriver;
                    state.activePlayed = true;
                    state.idlePlayed = false;
                    
					PlayAnimation(npc, anim, 0);
                }
            }
        }

        else
        {
            if (!state.idlePlayed)
            {
                const std::string& anim = ctx.isBoat ? g_Config.waterVehicleIdles : g_Config.landVehicleIdles;
                state.idlePlayed = true;
                state.activePlayed = false;
                
				PlayAnimation(npc, anim, 0);
            }
        }
    }

    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return;
    }
}

void VehicleAnimationResetCheck(CharacterObject* character, const VehicleFrameContext& ctx)
{
    if (!character)
        return;

    if (!ctx.insideVehicle)
        return;

    if (Get_Animation_Request_ID(character) == 0)
    {
        if (ctx.vehicleSeatState != 0)
        {
            if (ctx.isCar)
            {
                const std::string& anim = ctx.reversing ? g_Config.landVehicleDriverReverse : g_Config.landVehicleDriver;
                
				PlayAnimation(character, anim, 0);
            }

            if (ctx.isBoat)
            {
                const std::string& anim = g_Config.waterVehicleDriver;
                
				PlayAnimation(character, anim, 0);
            }
        }

        if (ctx.passenger)
        {
            if (ctx.isCar)
            {
                const std::string& anim = g_Config.landVehicleIdles;
                
				PlayAnimation(character, anim, 0);
            }

            if (ctx.isBoat)
            {
                const std::string& anim = g_Config.waterVehiclePassenger;
                
				PlayAnimation(character, anim, 0);
            }
        }
    }
}

void CharacterVehicleSpeakingCheck(CharacterObject* characters, CharacterTrackState& st, unsigned long now, const VehicleFrameContext& ctx)
{
    if (!characters)
        return;

    if (!ctx.insideVehicle || ((characters == GetPlayer()) && lastVehicleReload))
	{
		st.speak.animationRequestID = -1;
		st.speak.triggerInvalid = false;
        
		return;
	}

	bool currentVocalMS = (Get_Remaining_Vocal_Ms(characters) == -1),
		 triggerValid = !currentVocalMS && !st.speak.lastVocalMS && !st.speak.triggerInvalid && !st.damage.triggerInvalid;

    if (triggerValid)
    {
        if (ctx.vehicleSeatState != 0)
        {
            if (ctx.isCar)
            {
                st.speak.triggerInvalid = true;
				const std::string& anim = ctx.reversing ? g_Config.landVehicleDriverReverseTalking : g_Config.landVehicleDriverTalking;
				
				PlayAnimation(characters, anim, 0);
				
				st.speak.animationRequestID = Get_Animation_Request_ID(characters);
            }

            if (ctx.isBoat)
            {
                st.speak.triggerInvalid = true;
				const std::string& anim = g_Config.waterVehicleDriverTalking;
				
				PlayAnimation(characters, anim, 0);
				
				st.speak.animationRequestID = Get_Animation_Request_ID(characters);
            }
        }

        if (ctx.passenger)
        {
            if (ctx.isCar)
            {
                st.speak.triggerInvalid = true;
				const std::string& anim = g_Config.landVehiclePassengerTalking;
				
				PlayAnimation(characters, anim, 0);
				
				st.speak.animationRequestID = Get_Animation_Request_ID(characters);
            }

            if (ctx.isBoat)
            {
                st.speak.triggerInvalid = true;
				const std::string& anim = g_Config.waterVehiclePassengerTalking;
				
				PlayAnimation(characters, anim, 0);
				
				st.speak.animationRequestID = Get_Animation_Request_ID(characters);
            }
        }
    }
	
	int currentAnimationRequestID = Get_Animation_Request_ID(characters);
	
	if (st.speak.triggerInvalid && (currentAnimationRequestID != st.speak.animationRequestID) && currentVocalMS && st.speak.lastVocalMS)
		st.speak.triggerInvalid = false;

    st.speak.lastVocalMS = currentVocalMS;
}

void Character_Object_Master_Tracker(void** pData, int count)
{
    if (!pData || count <= 0)
    {
        g_CharStates.clear();   // Stale states must not survive a load / teleport / empty list
        
		return;
    }

    unsigned long now = GetTickCount();
    bool doVehicleAnimCheck = (now - g_LastNPCAnimCheck >= NPC_ANIM_INTERVAL);

    if (doVehicleAnimCheck)
        g_LastNPCAnimCheck = now;

    CharacterObject* mainChar = GetPlayer();

    ++g_TrackerTick;

    for (int i = 0; i < count; i++)
    {
        CharacterObject* character = (CharacterObject*)pData[i];
		
        if (!character)
            continue;

        CharacterTrackState& st = GetCharacterState(character);
        VehicleFrameContext ctx = BuildVehicleFrameContext(character);
		
		if (!g_Config.disableEveryEnhancement)
		{
			bool npcVehicleAnimationCheckValid = doVehicleAnimCheck && (character != mainChar) && !g_Config.disableVehicleIdles;

			if (npcVehicleAnimationCheckValid)
				NPCVehicleAnimationCheck(character, st, ctx);

			if (!g_Config.disableVehicleDamage)
			{
				NPCVehicleDamageCheck(character, st, ctx, now);
				NPC50CalDamageCheck(character, st, ctx);	
			}
			
			if (!g_Config.disableVehicleTalking)
				CharacterVehicleSpeakingCheck(character, st, now, ctx);
		}
		
		VehicleAnimationResetCheck(character, ctx);
    }

    // Drop state for characters that were not in the list this tick
    for (std::unordered_map<CharacterObject*, CharacterTrackState>::iterator it = g_CharStates.begin(); it != g_CharStates.end(); )
    {
        if (it->second.lastSeenTick != g_TrackerTick)
            it = g_CharStates.erase(it);
        
		else
            ++it;
    }
}

// ScarfaceEX Features' Validation Tracker
bool ScarfaceEX_Trigger_Valid(CharacterObject* p)
{
	bool scarfaceEXTriggerValid = false,
		 externalScriptsInitialised = false,
		 mainMenu = false;
	
	__try
	{
		float currentX = *(float*)((uintptr_t)p + 0x34 + 48), 
			  currentY = *(float*)((uintptr_t)p + 0x34 + 52), 
			  currentZ = *(float*)((uintptr_t)p + 0x34 + 56);
			  
		mainMenu = (currentX > -3500.0f) && (currentX < -3400.0f) && (currentY > 19.0f) && (currentY < 20.0f) && (currentZ > -1800.0f) && (currentZ < -1700.0f);
		scarfaceEXTriggerValid = !Screen_Fx_Transition_Is_Blacked_Out() && !NIS_Is_Playing() && !HUD_Is_Game_Paused() && !Is_Teleporting() && !mainMenu;
		scarfaceEXTriggerValid = !Is_Main_Character_Killed_Or_Flagged_For_On_Killed() && scarfaceEXTriggerValid;
		externalScriptsInitialised = Initialise_Run_Script() && !NIS_Is_Playing() && !HUD_Is_Game_Paused() && !Is_Teleporting() && !External_Scripts_Initialised;
		externalScriptsInitialised = !Screen_Fx_Transition_Is_Blacked_Out() && !Is_Main_Character_Killed_Or_Flagged_For_On_Killed() && externalScriptsInitialised;
			 
		if (externalScriptsInitialised)
		{
			RunScript(External_Scripts.c_str());
			
			External_Scripts_Initialised = true;
			extendedFeaturesTimer = GetTickCount();
			vehicleReloadsTimer = GetTickCount();
		}
	}
	
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		scarfaceEXTriggerValid = false;
	}
	
	return scarfaceEXTriggerValid;
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
			continue;
		
		// Global Trackers
		bool Dodges_Trigger_Valid = ((now - g_LastDodgeTime) >= g_Config.cooldown) && !g_Config.disableDodgesAndPhoneCalls,
			 Extended_Features_Valid = External_Scripts_Initialised && ((GetTickCount() - extendedFeaturesTimer) >= 100),
			 Vehicle_Reloads_Valid = External_Scripts_Initialised && ((GetTickCount() - vehicleReloadsTimer) >= 67) && !g_Config.disableVehicleReloads;
			 
		if (!g_Config.disableEveryEnhancement)
		{
			if (Dodges_Trigger_Valid) 
			{
				CheckKeyboard(p, now);
				CheckController(p, now);
				CheckDirectInput(p, now);
			}
			
			if (Vehicle_Reloads_Valid)
			{
				vehicleReloadsTimer = GetTickCount();
				
				VehicleReloads(p);
			}
		}
		
		if (Extended_Features_Valid)
		{
			extendedFeaturesTimer = GetTickCount();
			int npcCount = 0;
			void** npcList = GetNPCList(&npcCount);
			
			if (!g_Config.disableEveryEnhancement && !g_Config.disableVehicleIdles)
				CheckVehicleAnimation(p);
			
			Character_Object_Master_Tracker(npcList, npcCount);
		}
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

		// Initialize Dodges & Evades system; First Person Camera system; Characters' Tracking System
		bool dodgesOK = InitPlayerPointer() && InitPlayAnimation(),
			 npcOK = InitCVManager(), 
			 scriptOK = Initialise_Run_Script();

		// Start Input Thread If At Least 1 System Initialized
		if (dodgesOK || npcOK || scriptOK)
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