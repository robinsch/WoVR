#include "game_extras.h"
#include "cShaderData.h"
#include "cRenderObject.h"
#include "steamVR.h"
#include "uiViewport.h"
#include "clsKeyboard.h"
#include "OSK.h"
#include "monitorLayout.h"
#include "accountInformation.h"
#include "clsMemManager.h"
#include "structures.h"

extern bool doLog;
extern std::stringstream logError;

#define M_PI		3.14159265358979323846f
float Deg2Rad = M_PI / 180.0f;
float Rad2Deg = 180.0f / M_PI;

const std::string g_VR_PATH = "./vr/";
const std::string g_CONFIG_FILE = g_VR_PATH + "config.txt";

stDX11 devDX11;
int curEye = 0;
IDirect3DDevice9* devDX9 = nullptr;
int textureIndex = 0;
//simpleXR* sxr = new simpleXR(true);
simpleVR* svr = new simpleVR(false);

stMonitorLayout monitors;
stScreenLayout screenLayout = stScreenLayout();
POINT hmdBufferSize = { 0, 0 };
POINT uiBufferSize = { 0, 0 };

stBasicTexture BackBuffer11[6] = { stBasicTexture(), stBasicTexture(), stBasicTexture(), stBasicTexture(), stBasicTexture(), stBasicTexture() };
stBasicTexture DepthBuffer11[6] = { stBasicTexture(), stBasicTexture(), stBasicTexture(), stBasicTexture(), stBasicTexture(), stBasicTexture() };

stBasicTexture9 BackBuffer[6] = { stBasicTexture9(), stBasicTexture9(), stBasicTexture9(), stBasicTexture9(), stBasicTexture9(), stBasicTexture9() };
stBasicTexture9 DepthBuffer[6] = { stBasicTexture9(), stBasicTexture9(), stBasicTexture9(), stBasicTexture9(), stBasicTexture9(), stBasicTexture9() };

//stBasicTexture9 mainRenderBuffer = stBasicTexture9();
//stBasicTexture9 mainDepthBuffer = stBasicTexture9();

stBasicTexture9 uiRender = stBasicTexture9();
stBasicTexture9 uiRenderMask = stBasicTexture9();
stBasicTexture9 uiRenderCheck = stBasicTexture9();
stBasicTexture9 uiRenderCheckSystem = stBasicTexture9();
stBasicTexture9 uiDepth = stBasicTexture9();
stBasicTexture9 cursor = stBasicTexture9();

struct RayTarget
{
    unsigned int targetIDA;
    unsigned int targetIDB;
    Vector3 intersectPoint;
    float intersectDepth;
    Vector3 intersectFrom;
    Vector3 intersectTo;
};

OSK osk = OSK();
stBasicTexture9 oskTexture = stBasicTexture9();
stScreenLayout* oskLayout = nullptr;
int oskRenderCount = -1;
bool showOSK = true;
bool oldOSK = true;
bool isRunningAsAdmin = false;
bool defaultCutupResolution = false;
bool isPossessing = false;
std::stringstream outStream;

stBasicTexture9 handWatchList[] = {
    stBasicTexture9(), stBasicTexture9(),
    stBasicTexture9(), stBasicTexture9(),
    stBasicTexture9(), stBasicTexture9(),
};

int handWatchCount = (sizeof(handWatchList) / sizeof(stBasicTexture9)) / 2;
bool handWatchAtUI[3] = {};

enum uiFullLayout
{
    loginScreen
};

enum uiCutupLayout
{
    onHead,
    centerScreen,
    characterScreen,
    questLog,
    spellbook,
    chatbox,
    topMenu,
    map,
    menu,
    handActionbar,
    recountOmen,
    mouseover,
    
    //rightClickMenu,
    //pinkSquare,
};

int numViewPortsUI = 0;
int numViewPortsGame = 0;
std::vector<uiViewport> uiViewUI;
std::vector<uiViewport> uiViewGame;
bool doCutUI = true;
bool isCutActionShown = true;
bool doOcclusion = true;
stObjectManager* gPlayerObj = nullptr;

float gRotation = 0;
float gCamRotation = 0;
float vRotationOffset = 0;
float hRotationStickOffset = 0;
float maxRadRot = 2 * M_PI;
bool resetPlayerAnimCounter = false;
DWORD origBackBuffer = 0;
DWORD origDepthBuffer = 0;
DWORD curBackBufferLoc = 0;
IDirect3DSurface9* mainBackBuffer = nullptr;

bool printRoute = false;
bool isOverUI = false;
int indent = 0;
POINT wtfSize = { 0, 0 };

ShaderData vsTexture = VertexShaderTexture();
ShaderData psTexture = PixelShaderTexture();
ShaderData psMouseDot = PixelShaderWithMouseDot();
ShaderData vsColor = VertexShaderColor();
ShaderData psColor = PixelShaderColor();
ShaderData psMask = PixelShaderMask();

RenderObject curvedUI = nullptr;
RenderObject maskedUI = nullptr;
RenderObject cutUI = nullptr;
RenderObject cursorUI = nullptr;
RenderObject cursorWorld = nullptr;
RenderObject rayLine = nullptr;
RenderObject oskUI = nullptr;
RenderObject xyzGizmo = nullptr;
RenderObject handWatchSquare[3] = { nullptr, nullptr, nullptr };



XMMATRIX matProjection[2] = { XMMatrixIdentity(), XMMatrixIdentity() };
XMMATRIX matEyeOffset[2] = { XMMatrixIdentity(), XMMatrixIdentity() };
XMMATRIX matHMDPos = XMMatrixIdentity();
XMMATRIX matController[2] = { XMMatrixIdentity(), XMMatrixIdentity() };
XMMATRIX matControllerPalm[2] = { XMMatrixIdentity(), XMMatrixIdentity() };
XMMATRIX cameraMatrix = XMMatrixIdentity();
XMMATRIX cameraMatrixIPD = XMMatrixIdentity();
XMMATRIX cameraMatrixGame = XMMatrixIdentity();
XMMATRIX zeroScale = XMMatrixScaling(0.00001f, 0.00001f, 0.00001f);
XMMATRIX before = {
         0, 0,-1, 0,
        -1, 0, 0, 0,
         0, 1, 0, 0,
         0, 0, 0, 1,
};
XMMATRIX after = {
         0,-1, 0, 0,
         0, 0, 1, 0,
        -1, 0, 0, 0,
         0, 0, 0, 1,
};
IDirect3DTexture9* hiddenTexture;

//----
// Config settings
//----
bool cfg_OSK = true;
bool cfg_snapRotateX = false;
bool cfg_snapRotateY = true;
float cfg_snapRotateAmountX = 45.0f;
float cfg_snapRotateAmountY = 15.0f;
float cfg_uiOffsetScale = 0.5f;
float cfg_uiOffsetZ = -60.0f;
float cfg_uiOffsetY = 0.0;
float cfg_uiOffsetD = -0.94055f;
int cfg_flyingMountID = 0;
int cfg_groundMountID = 0;
int cfg_hmdOnward = 0;
int cfg_uiMultiplier = 3;
int cfg_gameMultiplier = 2;
bool cfg_disableControllers = false;
bool cfg_showBodyFPS = false;
std::string cfg_loginAccount = "";
std::string cfg_loginPassword = "";
inputController input = {}; //{ { 0, 0, 0, 0, 0, 0, 0, 0, 0 } };

//----
// DEBUG STUFF
//----
struct debugCounter
{
    int ida = 0;
    int idb = 0;
    int idc = 0;
    int idd = 0;
    int ide = 0;

    debugCounter()
    {
        ida = 0;
        idb = 0;
        idc = 0;
        idd = 0;
        ide = 0;
    }

    void update(int itemCount, int counter)
    {
        int value = 0;
        value = counter;
        counter = value / itemCount;
        ida = value % itemCount;

        value = counter;
        counter = value / itemCount;
        idb = value % itemCount;

        value = counter;
        counter = value / itemCount;
        idc = value % itemCount;

        value = counter;
        counter = value / itemCount;
        idd = value % itemCount;
    }
};

//----
// Game Declarations
//----
//void(__thiscall* CGWorldFrameM__OnWorldUpdate)(void*) = (void(__thiscall*)(void*))0x004FA5F0;
//void(__thiscall* CGWorldFrameM__Render)(void*) = (void(__thiscall*)(void*))0x004F8EA0;
void(__thiscall* CalculateForwardMovement)(int, int) = (void(__thiscall*)(int, int))0x0098B0E0;
void(__thiscall* CGMovementInfo__SetFacing)(int, float) = (void(__thiscall*)(int, float))0x00989B70;
void(__thiscall* CGInputControl__SetControlBit)(int, int, int) = (void(__thiscall*)(int, int, int))0x005FA170;
int (__thiscall* CGInputControl__UnsetControlBit)(int, int, int, int) = (int(__thiscall*)(int, int, int, int))0x005FA450;
void(__thiscall* CGInputControl__UpdatePlayer)(int, int, int) = (void(__thiscall*)(int, int, int))0x005FBBC0;
//void(__thiscall* CGCamera__ZoomIn)(int, float, int, float) = (void(__thiscall*)(int, float, int, float))0x005FF950;
//void(__thiscall* CGCamera__ZoomOut)(int, float, int, float) = (void(__thiscall*)(int, float, int, float))0x005FFA60;
//void(__thiscall* CameraUpdateX)(int, float) = (void(__thiscall*)(int, float))0x005FE5F0;
//void(__thiscall* CameraUpdateY)(int, float) = (void(__thiscall*)(int, float))0x005FFC20;
//bool(__thiscall* IsFallingSwimmingFlying)(int) = (bool(__thiscall*)(int))0x006EABA0;
void (*CastSpell)(int, int, int, int, int) = (void (*)(int, int, int, int, int))0x0080DA40;

bool(__thiscall* RayIntersect)(int, float, float, Vector3*, Vector3*) = (bool(__thiscall*)(int, float, float, Vector3*, Vector3*))0x004F6450;
bool(__thiscall* WorldClickIntersect)(int, Vector3*, Vector3*, unsigned int, RayTarget*) = (bool(__thiscall*)(int, Vector3*, Vector3*, unsigned int, RayTarget*))0x0004F9930;

float (*EnsureProperRadians)(float) = (float (*)(float))0x004C5090;
int (*CGWorldFrame__GetActiveCamera)() = (int (*)())0x004F5960;
//int (*ClntObjMgrGetActivePlayer)() = (int(*)())0x004D3790;
stObjectManager*(*ClntObjMgrObjectPtr)(unsigned int, unsigned int, unsigned int, const char*, unsigned int) = (stObjectManager * (*)(unsigned int, unsigned int, unsigned int, const char*, unsigned int))0x004D4DB0;
stObjectManager*(*ClntObjMgrGetActivePlayerObj)() = (stObjectManager*(*)())0x004038F0;
//int (*CMovementStatus__Read)(int, int) = (int(*)(int, int))0x004F4D40;

void (__thiscall* Matrix44Translate)(int, int) = (void (__thiscall*)(int, int))0x004C1B30;

//bool (*lua_ChangeActionBarPage)(int) = (bool (*)(int))0x005A7F60;
//int  (*lua_ToggleRun)() = (int (*)())0x005FAAE0;
//void (*lua_RunBinding)(char*, char*) = (void(*)(char*, char*))0x0055FAD0;
void (*lua_TargetNearestEnemy)(int*) = (void(*)(int*))0x00525AD0;
void (*lua_Dismount)() = (void(*)())0x0051D170;
//bool (*lua_IsMounted)(int) = (bool(*)(int))0x006125A0;

//void (*lua_MoveView)() = (void(*)())0x005FF000;
//void (*lua_CameraOrSelectOrMoveStart)() = (void(*)())0x005FC6C0;
//void (*lua_CameraOrSelectOrMoveStop)(int*) = (void(*)(int*))0x005FC730;
//void (*lua_TurnOrActionStart)() = (void(*)())0x005FC610;
//void (*lua_TurnOrActionStop)() = (void(*)())0x005FC680;

void RunFrameUpdateController();
void RunFrameUpdateKeyboard();
void RunControllerGame();


XMVECTOR GetAngles(XMMATRIX source)
{
    float thetaX, thetaY, thetaZ = 0.0f;
    thetaX = std::asin(source._32);

    if (thetaX < (M_PI / 2))
    {
        if (thetaX > (-M_PI / 2))
        {
            thetaZ = std::atan2(-source._12, source._22);
            thetaY = std::atan2(-source._31, source._33);
        }
        else
        {
            thetaZ = -std::atan2(-source._13, source._11);
            thetaY = 0;
        }
    }
    else
    {
        thetaZ = std::atan2(source._13, source._11);
        thetaY = 0;
    }
    return { thetaX, thetaY, thetaZ, 0 };
}

bool IsElevated()
{
    bool fRet = false;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        TOKEN_ELEVATION Elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize))
        {
            fRet = Elevation.TokenIsElevated;
        }
    }
    if (hToken)
    {
        CloseHandle(hToken);
    }
    return fRet;
}

int searchAddress(std::vector<std::vector<int>> addressList)
{
    int addressCount = addressList.size();
    for (int i = 0; i < addressCount; i++)
        if (i == 0)
            addressList[i][1] = *(int*)(addressList[i][0]);
        else if (addressList[i - 1][1] > 0)
            addressList[i][1] = *(int*)(addressList[i - 1][1] + addressList[i][0]);

    return (addressList[addressCount - 1][1]) ? addressList[addressCount - 1][1] : 0;
}

stObjectManager* objManagerGetActiveObject()
{
    std::vector<std::vector<int>> charObj = {
            { 0xCD87A8, 0 },
            { 0x34, 0 },
            { 0x24, 0 },
            { 0x77C, 0 },
            { 0x150, 0 }
    };
    return (stObjectManager*)searchAddress(charObj);
}


stObjectManager* objManagerGetTargetObj()
{
    // Game Target
    int idA = *(int*)0x00BD07B0;
    int idB = *(int*)0x00BD07B4;
    if (idA)
        return ClntObjMgrObjectPtr(idA, idB, 8, ".\\GameUI.cpp", 0x774);
    return 0;
}

stObjectManager* objManagerGetMouseoverObj()
{
    // Mouseover
    int worldFrame = *(int*)0x00B7436C;
    if (worldFrame)
    {
        int idA = *(int*)(worldFrame + 0x2C8);
        int idB = *(int*)(worldFrame + 0x2CC);
        if (idA)
            return ClntObjMgrObjectPtr(idA, idB, 1, ".\\GameUI.cpp", 0xFFFF);
    }
    return 0;
}


void RunOSKEnable()
{
    showOSK = false;
    if (devDX9)
    {
        oskRenderCount = 50;
        RECT position = { 100, 100, 700, 200 };
        if (!osk.LoadOSK9(devDX9, &oskTexture, position))
            logError << "Error creating/finding the OnScreen Keyboard" << std::endl;
        oskLayout = osk.GetOSKLayout();
    }
}

void RunOSKDisable()
{
    showOSK = false;
    osk.ShowHide(true);
    osk.UnloadOSK();
    oskTexture.Release();
    oskLayout = nullptr;
    oskRenderCount = -1;
}

void RunOSKUpdate()
{
    //----
    // Hide keyboard if shown after a few frames
    //----
    if (cfg_OSK && oskRenderCount <= 0 && oldOSK != showOSK)
    {
        oldOSK = showOSK;
        osk.ShowHide(showOSK);

        //----
        // move chat down while keyboard is shown
        //----
        if (showOSK == true)
        {
            uiViewGame.at(uiCutupLayout::chatbox).rotation.x = 0;
            uiViewGame.at(uiCutupLayout::chatbox).offset.y = -0.16f;
        }
        else
        {
            uiViewGame.at(uiCutupLayout::chatbox).rotation.x = 20;
            uiViewGame.at(uiCutupLayout::chatbox).offset.y = 0.16f;
        }
    }
    if (oskLayout != nullptr && oskLayout->haveLayout && oskRenderCount > 0)
        oskRenderCount--;
    else if (cfg_OSK && oskLayout != nullptr && !oskLayout->haveLayout)
    {
        RunOSKDisable();
        RunOSKEnable();
    }

    if (cfg_OSK && showOSK)
        osk.CopyOSKTexture9(devDX9, &oskTexture);
}

int cfg_tID = 0;

void writeConfigFile()
{
    std::ofstream cfgFile(g_CONFIG_FILE);
    if (cfgFile.is_open())
    {
        cfgFile << "OSK: " << cfg_OSK << std::endl;
        cfgFile << "snapRotateX: " << cfg_snapRotateX << std::endl;
        cfgFile << "snapRotateY: " << cfg_snapRotateY << std::endl;
        cfgFile << "snapRotateAmountX: " << cfg_snapRotateAmountX << std::endl;
        cfgFile << "snapRotateAmountY: " << cfg_snapRotateAmountY << std::endl;
        cfgFile << "uiOffsetScale: " << cfg_uiOffsetScale << std::endl;
        cfgFile << "uiOffsetZ: " << cfg_uiOffsetZ << std::endl;
        cfgFile << "uiOffsetY: " << cfg_uiOffsetY << std::endl;
        cfgFile << "uiOffsetD: " << cfg_uiOffsetD << std::endl;
        cfgFile << "flyingMountID: " << cfg_flyingMountID << std::endl;
        cfgFile << "groundMountID: " << cfg_groundMountID << std::endl;
        cfgFile << "hmdOnward: " << cfg_hmdOnward << std::endl;
        cfgFile << "uiMultiplier: " << cfg_uiMultiplier << std::endl;
        cfgFile << "gameMultiplier: " << cfg_gameMultiplier << std::endl;
        cfgFile << "disableControllers: " << cfg_disableControllers << std::endl;
        cfgFile << "showBodyFPS: " << cfg_showBodyFPS << std::endl;
        cfgFile.close();
    }
    else
    {
        logError << "Unable to write config file" << std::endl;
    }
}

void readConfigFile()
{
    //----
    // save the default config data if the file dosnt exist
    //----
    std::ifstream cfgFile(g_CONFIG_FILE);
    if (!cfgFile.good())
        writeConfigFile();
    cfgFile.close();

    std::string s_cfg_OSK = "";
    std::string s_cfg_snapRotateX = "";
    std::string s_cfg_snapRotateY = "";
    std::string s_cfg_snapRotateAmountX = "";
    std::string s_cfg_snapRotateAmountY = "";
    std::string s_cfg_uiOffsetScale = "";
    std::string s_cfg_uiOffsetZ = "";
    std::string s_cfg_uiOffsetY = "";
    std::string s_cfg_uiOffsetD = "";
    std::string s_cfg_flyingMountID = "";
    std::string s_cfg_groundMountID = "";
    std::string s_cfg_hmdOnward = "";
    std::string s_cfg_uiMultiplier = "";
    std::string s_cfg_gameMultiplier = "";
    std::string s_cfg_disableControllers = "";
    std::string s_cfg_showBodyFPS = "";
    std::string s_cfg_loginAccount = "";
    std::string s_cfg_loginPassword = "";

    cfgFile.open(g_CONFIG_FILE);
    if (cfgFile.is_open())
    {
        //----
        // load the config file
        //----
        std::getline(cfgFile, s_cfg_OSK);
        std::getline(cfgFile, s_cfg_snapRotateX);
        std::getline(cfgFile, s_cfg_snapRotateY);
        std::getline(cfgFile, s_cfg_snapRotateAmountX);
        std::getline(cfgFile, s_cfg_snapRotateAmountY);
        std::getline(cfgFile, s_cfg_uiOffsetScale);
        std::getline(cfgFile, s_cfg_uiOffsetZ);
        std::getline(cfgFile, s_cfg_uiOffsetY);
        std::getline(cfgFile, s_cfg_uiOffsetD);
        std::getline(cfgFile, s_cfg_flyingMountID);
        std::getline(cfgFile, s_cfg_groundMountID);
        std::getline(cfgFile, s_cfg_hmdOnward);
        std::getline(cfgFile, s_cfg_uiMultiplier);
        std::getline(cfgFile, s_cfg_gameMultiplier);
        std::getline(cfgFile, s_cfg_disableControllers);
        std::getline(cfgFile, s_cfg_showBodyFPS);
        std::getline(cfgFile, s_cfg_loginAccount);
        std::getline(cfgFile, s_cfg_loginPassword);
        cfgFile.close();

        //----
        // remove the config names
        //----
        s_cfg_OSK.erase(0, s_cfg_OSK.find(": ") + 2);
        s_cfg_snapRotateX.erase(0, s_cfg_snapRotateX.find(": ") + 2);
        s_cfg_snapRotateY.erase(0, s_cfg_snapRotateY.find(": ") + 2);
        s_cfg_snapRotateAmountX.erase(0, s_cfg_snapRotateAmountX.find(": ") + 2);
        s_cfg_snapRotateAmountY.erase(0, s_cfg_snapRotateAmountY.find(": ") + 2);
        s_cfg_uiOffsetScale.erase(0, s_cfg_uiOffsetScale.find(": ") + 2);
        s_cfg_uiOffsetZ.erase(0, s_cfg_uiOffsetZ.find(": ") + 2);
        s_cfg_uiOffsetY.erase(0, s_cfg_uiOffsetY.find(": ") + 2);
        s_cfg_uiOffsetD.erase(0, s_cfg_uiOffsetD.find(": ") + 2);
        s_cfg_flyingMountID.erase(0, s_cfg_flyingMountID.find(": ") + 2);
        s_cfg_groundMountID.erase(0, s_cfg_groundMountID.find(": ") + 2);
        s_cfg_hmdOnward.erase(0, s_cfg_hmdOnward.find(": ") + 2);
        s_cfg_uiMultiplier.erase(0, s_cfg_uiMultiplier.find(": ") + 2);
        s_cfg_gameMultiplier.erase(0, s_cfg_gameMultiplier.find(": ") + 2);
        s_cfg_disableControllers.erase(0, s_cfg_disableControllers.find(": ") + 2);
        s_cfg_showBodyFPS.erase(0, s_cfg_showBodyFPS.find(": ") + 2);
        s_cfg_loginAccount.erase(0, s_cfg_loginAccount.find(": ") + 2);
        s_cfg_loginPassword.erase(0, s_cfg_loginPassword.find(": ") + 2);

        //----
        // set the config options
        //----
        cfg_OSK = s_cfg_OSK != "0";
        cfg_snapRotateX = s_cfg_snapRotateX != "0";
        cfg_snapRotateY = s_cfg_snapRotateY != "0";
        cfg_snapRotateAmountX = std::stof(s_cfg_snapRotateAmountX);
        cfg_snapRotateAmountY = std::stof(s_cfg_snapRotateAmountY);
        cfg_uiOffsetScale = std::stof(s_cfg_uiOffsetScale);
        cfg_uiOffsetZ = std::stof(s_cfg_uiOffsetZ);
        cfg_uiOffsetY = std::stof(s_cfg_uiOffsetY);
        cfg_uiOffsetD = std::stof(s_cfg_uiOffsetD);
        cfg_flyingMountID = std::stoi(s_cfg_flyingMountID);
        cfg_groundMountID = std::stoi(s_cfg_groundMountID);
        cfg_hmdOnward = std::stoi(s_cfg_hmdOnward);
        cfg_uiMultiplier = std::stoi(s_cfg_uiMultiplier);
        cfg_gameMultiplier = std::stoi(s_cfg_gameMultiplier);
        cfg_disableControllers = s_cfg_disableControllers != "0";
        cfg_showBodyFPS = s_cfg_showBodyFPS != "0";
        cfg_loginAccount = s_cfg_loginAccount;
        cfg_loginPassword = s_cfg_loginPassword;
        if (cfg_uiMultiplier < 1) cfg_uiMultiplier = 1;
        if (cfg_gameMultiplier < 1) cfg_gameMultiplier = 1;

        //hiddenTexture = (IDirect3DTexture9*)std::stol(s_cfg_groundMountID, NULL, 16);
    }
}

void CreateTextures(ID3D11Device* devDX11, IDirect3DDevice9* devDX9, POINT textureSize, POINT textureSizeUI)
{
    HRESULT result = S_OK;
    for (int i = 0; i < 6; i++)
    {
        BackBuffer11[i].SetWidthHeight(textureSize.x, textureSize.y);
        if(!BackBuffer11[i].Create(devDX11, false, true, false, true))
            logError << BackBuffer11[i].GetErrors();
        //logError << i << " BackBuffer Shared " << BackBuffer11[i].pSharedHandle << std::endl;

        BackBuffer[i].SetWidthHeight(textureSize.x, textureSize.y);
        BackBuffer[i].pSharedHandle = BackBuffer11[i].pSharedHandle;
        if (!BackBuffer[i].Create(devDX9, false, true, false, true))
            logError << BackBuffer[i].GetErrors();


        DepthBuffer11[i].SetWidthHeight(textureSize.x, textureSize.y);
        DepthBuffer11[i].textureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        DepthBuffer11[i].textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        if(!DepthBuffer11[i].Create(devDX11, false, false, true, true))
            logError << DepthBuffer11[i].GetErrors();
        //logError << i << " DepthBuffer Shared " << DepthBuffer11[i].pSharedHandle << std::endl;

        DepthBuffer[i].SetWidthHeight(textureSize.x, textureSize.y);
        DepthBuffer[i].pSharedHandle = DepthBuffer11[i].pSharedHandle;
        DepthBuffer[i].renderFormat = D3DFMT_D24X8;
        if(!DepthBuffer[i].Create(devDX9, false, false, true, false))
            logError << DepthBuffer[i].GetErrors();

    }

    uiRender.SetWidthHeight(textureSizeUI.x, textureSizeUI.y);
    if(!uiRender.Create(devDX9, false, true, false, false))
        logError << uiRender.GetErrors();

    uiRenderMask.SetWidthHeight(textureSizeUI.x / 4, textureSizeUI.y / 4);
    if (!uiRenderMask.Create(devDX9, false, true, false, false))
        logError << uiRenderMask.GetErrors();

    uiRenderCheck.SetWidthHeight(1, 1);
    if (!uiRenderCheck.Create(devDX9, false, true, false, false))
        logError << uiRenderCheck.GetErrors();

    //----
    // Requires SystemMem
    //----
    uiRenderCheckSystem.creationType = 1;
    result = devDX9->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &uiRenderCheckSystem.pTexture, NULL);
    if (SUCCEEDED(result))
        uiRenderCheckSystem.CreateShaderResourceView(devDX9);


    uiDepth.SetWidthHeight(textureSizeUI.x, textureSizeUI.y);
    uiDepth.renderFormat = D3DFMT_D24X8;
    if (!uiDepth.Create(devDX9, false, false, true, false))
        logError << uiDepth.GetErrors();


    //----
    // Cursor icons
    //----
    std::string cursorPath = g_VR_PATH + "images/cursors.png";
    if (!cursor.CreateFromFile(devDX9, false, false, false, cursorPath.c_str()))
        logError << cursor.GetErrors();

    //----
    // Watch on back of hand
    //----
    std::string imgFilePaths[] = {
            g_VR_PATH + "images/occlusion_off.png",   g_VR_PATH + "images/occlusion_on.png",
            g_VR_PATH + "images/keyboard_off.png",    g_VR_PATH + "images/keyboard_on.png",
            g_VR_PATH + "images/swapgroup_off.png",   g_VR_PATH + "images/swapgroup_on.png",
    };

    for (int i = 0; i < (handWatchCount * 2); i++)
    {
        struct stat buffer;
        std::string fullPath = imgFilePaths[i];
        if (stat(fullPath.c_str(), &buffer) == 0)
        {
            if (!handWatchList[i].CreateFromFile(devDX9, false, true, false, fullPath.c_str()))
                logError << handWatchList[i].GetErrors() << std::endl;
        }
        else
            logError << "Image not found " << fullPath << std::endl;
    }
}

void DestroyTextures()
{
    for (int i = 0; i < (handWatchCount * 2); i++)
        handWatchList[i].Release();

    cursor.Release();

    uiDepth.Release();
    uiRenderCheckSystem.Release();
    uiRenderCheck.Release();
    uiRenderMask.Release();
    uiRender.Release();

    for (int i = 0; i < 6; i++)
    {
        BackBuffer11[i].Release();
        BackBuffer[i].Release();

        DepthBuffer11[i].Release();
        DepthBuffer[i].Release();
    }    
}


bool CreateShaders(IDirect3DDevice9* devDX9)
{
    vsTexture.CompileShaderFromString(devDX9);
    psTexture.CompileShaderFromString(devDX9);
    psMouseDot.CompileShaderFromString(devDX9);
    vsColor.CompileShaderFromString(devDX9);
    psColor.CompileShaderFromString(devDX9);
    psMask.CompileShaderFromString(devDX9);

    return true;
}

void DestroyShaders()
{
    vsTexture.Release();
    psTexture.Release();
    psMouseDot.Release();
    vsColor.Release();
    psColor.Release();
    psMask.Release();
}

int setViewPorts(IDirect3DDevice9* devDX9, int tWidth, int tHeight, std::vector<uiViewport>* uiView, int type)
{
    if (type == 0)
    {
        std::vector<RECT> coords = std::vector<RECT>(1);
        coords.at(uiFullLayout::loginScreen) = { 516, 0, 1404, 500 };
        //coords.at(uiFullLayout::loginScreen) = { 0, 0, 1920, 500 };
        for (int i = 0; i < coords.size(); i++)
        {
            RECT coord = coords.at(i);
            coord.left = (int)((coord.left / 1920.f) * tWidth);
            coord.right = (int)((coord.right / 1920.f) * tWidth);
            coord.top = (int)((coord.top / 500.f) * tHeight);
            coord.bottom = (int)((coord.bottom / 500.f) * tHeight);
            coords.at(i) = coord;
        }

        uiView->resize(coords.size());
        uiView->at(uiFullLayout::loginScreen) = uiViewport(devDX9, coords.at(uiFullLayout::loginScreen),       0.f, 0.f, -0.5f,          0.f, 0.f, 0.f,      1.6f, 1.6f, 1.0f, true);

        //uiView->push_back(uiViewport(d3dDevice,  0,    0, 1920,  500,  0.f, 0.f, -0.75f,   0.f, 0.f, 0.f,   1.0f, 1.0f, 1.0f,  true)); // login screen
    }
    else if (type == 1)
    {
        std::vector<RECT> coords = std::vector<RECT>(12);
        coords.at(uiCutupLayout::characterScreen)   = { 1, 1, 337, 339 };
        coords.at(uiCutupLayout::handActionbar)     = { 1, 341, 147, 499 };
        coords.at(uiCutupLayout::recountOmen)       = { 1696, 43, 1919, 272 };
        coords.at(uiCutupLayout::menu)              = { 149, 341, 424, 499 };
        coords.at(uiCutupLayout::spellbook)         = { 339, 1, 616, 339 };
        coords.at(uiCutupLayout::onHead)            = { 618, 1, 833, 200 };
        coords.at(uiCutupLayout::questLog)          = { 618, 202, 802, 499 };
        coords.at(uiCutupLayout::chatbox)           = { 835, 1, 1119, 178 };
        coords.at(uiCutupLayout::centerScreen)      = { 804, 180, 1119, 362 };
        coords.at(uiCutupLayout::topMenu)           = { 804, 364, 1119, 499 };
        coords.at(uiCutupLayout::map)               = { 1121, 1, 1694, 499 };
        coords.at(uiCutupLayout::mouseover)         = { 1696, 274, 1919, 499 };


        for (int i = 0; i < coords.size(); i++)
        {
            RECT coord = coords.at(i);
            coord.left = (int)((coord.left / 1920.f) * tWidth);
            coord.right = (int)((coord.right / 1920.f) * tWidth);
            coord.top = (int)((coord.top / 500.f) * tHeight);
            coord.bottom = (int)((coord.bottom / 500.f) * tHeight);
            coords.at(i) = coord;
        }

        uiView->resize(coords.size());

        uiView->at(uiCutupLayout::centerScreen) = uiViewport(devDX9, coords.at(uiCutupLayout::centerScreen),            0.018f, -0.044f, -0.468f,      0.f, 0.f, 0.f,       1.212f, 1.212f, 1.f, true);
        uiView->at(uiCutupLayout::questLog) = uiViewport(devDX9, coords.at(uiCutupLayout::questLog),                     0.f, -0.050f, -0.500f,         0.f, 30.f, 0.f,      0.573f, 0.573f, 1.f, true);
        uiView->at(uiCutupLayout::characterScreen) = uiViewport(devDX9, coords.at(uiCutupLayout::characterScreen),      -0.131f, -0.036f, -0.518f,      0.f, 40.f, 0.f,     0.688f, 0.688f, 1.f, true);
        uiView->at(uiCutupLayout::spellbook) = uiViewport(devDX9, coords.at(uiCutupLayout::spellbook),                  -0.112f, -0.048f, -0.490f,      0.f, -50.f, 0.f,    0.732f, 0.732f, 1.f, true);
        uiView->at(uiCutupLayout::chatbox) = uiViewport(devDX9, coords.at(uiCutupLayout::chatbox),                      0.030f, 0.16f, -0.450f,      20.f, 0.f, 0.f,     0.735f, 0.735f, 1.f, true);
        uiView->at(uiCutupLayout::topMenu) = uiViewport(devDX9, coords.at(uiCutupLayout::topMenu),                      0.024f, 0.273f, -0.595f,      35.f, 0.f, 0.f,     0.833f, 0.833f, 1.f, true);
        uiView->at(uiCutupLayout::map) = uiViewport(devDX9, coords.at(uiCutupLayout::map),                              -0.041f, -0.177f, -0.505f,   -30.f, 0.f, 0.f,    0.903f, 0.903f, 1.f, true);
        uiView->at(uiCutupLayout::menu) = uiViewport(devDX9, coords.at(uiCutupLayout::menu),                            0.f, 0.f, -0.460f,             0.f, 0.f, 0.f,      0.5f, 0.5f, 1.f, true);
        uiView->at(uiCutupLayout::handActionbar) = uiViewport(devDX9, coords.at(uiCutupLayout::handActionbar),          -0.13f, 0.f, 0.f,            180.f, 90.f, 0.f,   0.05f, 0.05f, 1.f, true);
        uiView->at(uiCutupLayout::recountOmen) = uiViewport(devDX9, coords.at(uiCutupLayout::recountOmen),              -0.13f, 0.f, 0.f,            180.f, 90.f, 0.f,   0.05f, 0.05f, 1.f, true);
        uiView->at(uiCutupLayout::mouseover) = uiViewport(devDX9, coords.at(uiCutupLayout::mouseover),                  0.f, 0.f, 0.f,               0.f, 0.f, 0.f,      0.2f, 0.2f, 1.f, false);
        uiView->at(uiCutupLayout::onHead) = uiViewport(devDX9, coords.at(uiCutupLayout::onHead),                        0.f, 0.f, -0.600f,            0.f, 0.f, 0.f,      0.8f, 0.8f, 1.f, false);
    }
    return uiView->size();
}

bool CreateBuffers(IDirect3DDevice9* devDX9, POINT textureSizeUI)
{
    curvedUI = RenderCurvedUI(devDX9);
    curvedUI.SetShadersLayout(vsTexture.Layout, vsTexture.VS, psMouseDot.PS);

    cutUI = RenderSquare(devDX9);
    cutUI.SetShadersLayout(vsTexture.Layout, vsTexture.VS, psTexture.PS);
    //cutUI.SetShadersLayout(vsTexture.Layout, vsTexture.VS, psMask.PS);
    cutUI.indexSet = false;

    maskedUI = RenderMaskUI(devDX9);
    maskedUI.SetShadersLayout(vsTexture.Layout, vsTexture.VS, psMask.PS);

    cursorUI = RenderSquare(devDX9);
    cursorUI.SetShadersLayout(vsTexture.Layout, vsTexture.VS, psTexture.PS);

    cursorWorld = RenderSquare(devDX9);
    cursorWorld.SetShadersLayout(vsTexture.Layout, vsTexture.VS, psTexture.PS);

    rayLine = RenderRayLine(devDX9);
    rayLine.SetShadersLayout(vsColor.Layout, vsColor.VS, psColor.PS);

    oskUI = RenderOSK(devDX9);
    oskUI.SetShadersLayout(vsTexture.Layout, vsTexture.VS, psTexture.PS);
    
    xyzGizmo = RenderXYZGizmo(devDX9);
    xyzGizmo.SetShadersLayout(vsColor.Layout, vsColor.VS, psColor.PS);
    

    doCutUI = false;
    std::vector<uiViewport> uiView = std::vector<uiViewport>();
    //if ((textureSizeUI.x / 96.0f) == (textureSizeUI.y / 25.0f))
    if(!cfg_disableControllers && (defaultCutupResolution || ((textureSizeUI.x / 96.0f) == (textureSizeUI.y / 25.0f))))
    {
        doCutUI = true;
        numViewPortsUI = setViewPorts(devDX9, textureSizeUI.x, textureSizeUI.y, &uiViewUI, 0);
        numViewPortsGame = setViewPorts(devDX9, textureSizeUI.x, textureSizeUI.y, &uiViewGame, 1);
    }

    //----
    // Back of hand squares
    //----
    for (int i = 0; i < handWatchCount; i++)
    {
        handWatchSquare[i] = RenderSquare(devDX9);
        handWatchSquare[i].SetShadersLayout(vsTexture.Layout, vsTexture.VS, psTexture.PS);
    }

    return true;
}

void DestroyBuffers()
{
    curvedUI.Release();
    cutUI.Release();
    maskedUI.Release();
    cursorUI.Release();
    cursorWorld.Release();
    rayLine.Release();
    oskUI.Release();
    xyzGizmo.Release();

    for (int i = 0; i < handWatchCount; i++)
        handWatchSquare[i].Release();
}



XMMATRIX DxToGame(XMMATRIX matrix)
{
    return ((before * matrix) * after);
}

XMMATRIX GameToDx(XMMATRIX matrix)
{
    return ((after * matrix) * before);
}

XMMATRIX GetGameCamera(int camAddress, bool convert = false)
{
    if (camAddress)
    {
        XMMATRIX camMatrix = {
            *(float*)(camAddress + 0x14),
            *(float*)(camAddress + 0x18),
            *(float*)(camAddress + 0x1C),
            0.f,

            *(float*)(camAddress + 0x20),
            *(float*)(camAddress + 0x24),
            * (float*)(camAddress + 0x28),
            0.f,

            * (float*)(camAddress + 0x2C),
            * (float*)(camAddress + 0x30),
            * (float*)(camAddress + 0x34),
            0.f,

            * (float*)(camAddress + 0x08),
            * (float*)(camAddress + 0x0C),
            * (float*)(camAddress + 0x10),
            1.f
        };
        if (convert)
            return GameToDx(camMatrix);
        else
            return camMatrix;
    }
    else
        return XMMatrixIdentity();
}

void SetGameCamera(CGCamera* camera, XMMATRIX camMatrix, bool convert = false)
{
    if (camera)
    {
        if (convert)
            camMatrix = DxToGame(camMatrix);

        camera->m_position.x = camMatrix(3, 0);
        camera->m_position.y = camMatrix(3, 1);
        camera->m_position.z = camMatrix(3, 2);

        camera->m_rotation[0] = camMatrix(0, 0);
        camera->m_rotation[1] = camMatrix(0, 1);
        camera->m_rotation[2] = camMatrix(0, 2);
        camera->m_rotation[3] = camMatrix(1, 0);
        camera->m_rotation[4] = camMatrix(1, 1);
        camera->m_rotation[5] = camMatrix(1, 2);
        camera->m_yaw = camMatrix(2, 0);
        camera->m_pitch = camMatrix(2, 1);
        camera->m_roll = camMatrix(2, 2);
    }
}

void fnUpdateCameraController(int camAddress)
{
    if (camAddress)
    {
        //*(float*)(camLoc + 0x11C) += hRotationOffset;
        *(float*)(camAddress + 0x120) += vRotationOffset;
        //*(float*)(camLoc + 0x124) = 0;

        vRotationOffset = 0;
    }
}

void fnUpdateCameraHMD(int camAddress)
{
    if (camAddress)
    {
        CGCamera* camera = reinterpret_cast<CGCamera*>(camAddress);

        cameraMatrixGame = GetGameCamera(camAddress, false);
        cameraMatrix = GameToDx(cameraMatrixGame);

        XMVECTOR angles = GetAngles(cameraMatrix);
        XMMATRIX horizonLockMatirx = XMMatrixRotationAxis({ 1, 0, 0, 0 }, angles.vector4_f32[0]);
        cameraMatrix = horizonLockMatirx * cameraMatrix;
        cameraMatrixGame = DxToGame(cameraMatrix);
        
        cameraMatrixIPD = XMMatrixIdentity();
        if (curEye == 0 || curEye == 1)
            cameraMatrixIPD = matEyeOffset[curEye] * matHMDPos;
        else
            cameraMatrixIPD = matHMDPos;
        cameraMatrixIPD *= cameraMatrix;

        SetGameCamera(camera, cameraMatrixIPD, true);
        camera->m_fov = maxRadRot;
    }
}


void UpdateCharacterAnimation_post(stObjectManager* playerObj)
{
    int cameraAddress = CGWorldFrame__GetActiveCamera();
    XMMATRIX camRaw = GetGameCamera(cameraAddress, false);

    if (!isPossessing && cameraAddress && *(float*)(cameraAddress + 0x118) == 0)
    {
        int headId = boneLookup.Get("Head");
        int headParentId = boneLookup.parentList[headId];
        XMMATRIX newHead = *(XMMATRIX*)(playerObj->pModelContainer->ptrBonePos + (0x40 * headParentId));
        *(XMMATRIX*)(playerObj->pModelContainer->ptrBonePos + (0x40 * headParentId)) = newHead * zeroScale;
        for (int childId : boneLookup.allChildren[headParentId])
            *(XMMATRIX*)(playerObj->pModelContainer->ptrBonePos + (0x40 * childId)) = newHead * zeroScale;
    }
}

int IntersectGround(RayTarget* rayTarget)
{
    rayTarget->targetIDA = 0;
    rayTarget->targetIDB = 0;
    rayTarget->intersectPoint = { 0, 0, 0 };
    rayTarget->intersectDepth = 0;
    rayTarget->intersectFrom = { 0, 0, 0 };
    rayTarget->intersectTo = { 0, 0, 0 };

    int worldFrame = *(int*)0x00B7436C;
    int worldPanel = *(int*)0x00B499A8;
    if (worldPanel && worldFrame)
    {
        float gvFOV = *(float*)0xAC0CB4;
        float ghFOV = *(float*)0xAC0CB8;
        float panelMouseCoordPercentX = *(float*)(worldPanel + 0x1224);
        float panelMouseCoordPercentY = *(float*)(worldPanel + 0x1228);
        float screenMouseCoordPercentX = gvFOV * panelMouseCoordPercentX;
        float screenMouseCoordPercentY = ghFOV * panelMouseCoordPercentY;

        Vector3 point1 = { 0, 0, 0 };
        Vector3 point2 = { 0, 0, 0 };
        bool intersect = RayIntersect(worldFrame, screenMouseCoordPercentX, screenMouseCoordPercentY, &point1, &point2);
        if (intersect)
        {
            rayTarget->intersectFrom = { point1.x, point1.y, point1.z };
            rayTarget->intersectTo = { point2.x, point2.y, point2.z };
            //int intersectType = WorldClickIntersect(worldFrame, &point1, &point2, 0x5C, rayTarget);
            int retData = WorldClickIntersect(worldFrame, &point1, &point2, 0, rayTarget);
            return (rayTarget->intersectDepth > 0) ? true : false;
        }
    }
    return 0;
}

// Mouse to world ray caluclations
void (*CameraGetLineSegment)(float, float, Vector3*, Vector3*) = (void (*)(float, float, Vector3*, Vector3*))0x004BF0F0;
void (CameraGetLineSegment_hk)(float a, float b, Vector3* c, Vector3* d)
{
    XMMATRIX rayMatrix = XMMatrixIdentity();
    if (cfg_disableControllers)
        rayMatrix = (matHMDPos * after);
    else
        rayMatrix = (matController[1] * after);
    XMVECTOR origin = { rayMatrix._41, rayMatrix._42, rayMatrix._43 };
    XMVECTOR frwd = { rayMatrix._31, rayMatrix._32, rayMatrix._33 };
    XMVECTOR norm = XMVector3Normalize(frwd);
    XMVECTOR end = origin + (norm * -1000.0f);

    origin = XMVector4Transform(origin, cameraMatrixGame);
    end = XMVector4Transform(end, cameraMatrixGame);

    c->x = origin.vector4_f32[0];
    c->y = origin.vector4_f32[1];
    c->z = origin.vector4_f32[2];

    d->x = end.vector4_f32[0];
    d->y = end.vector4_f32[1];
    d->z = end.vector4_f32[2];
}

// SetClientMouseResetPoint
void (*sub_869DB0)() = (void(*)())0x00869DB0;
void (msub_869DB0)()
{
    sub_869DB0();
}

// Calculate Window Size
void (*sub_684D70)(int, int, int) = (void (*)(int, int, int))0x00684D70;
void (msub_684D70)(int a, int b, int c)
{
    //if(doLog) logError << "CalcWindowSize Pre : " << *(int*)(c + 0x0) << " : " << *(int*)(c + 0x4) << " : " << *(int*)(c + 0x8) << " : " << *(int*)(c + 0xC) << std::endl;

    RECT sizePre = { *(int*)(c + 0x0), *(int*)(c + 0x4), *(int*)(c + 0x8), *(int*)(c + 0xC) };

    sub_684D70(a, b, c);
    
    *(int*)(c + 0x0) = 0;
    *(int*)(c + 0x4) = 0;
    *(int*)(c + 0x8) = *(int*)(c + 0x0) + sizePre.right;
    *(int*)(c + 0xc) = *(int*)(c + 0x4) + sizePre.bottom;

    //if (doLog) logError << "CalcWindowSize Post : " << *(int*)(c + 0x0) << " : " << *(int*)(c + 0x4) << " : " << *(int*)(c + 0x8) << " : " << *(int*)(c + 0xC) << std::endl;
}

//----
// Create Window
//----
void(msub_6A08D0_pre)()
{
    svr->PreloadVR();

    hmdBufferSize = svr->GetBufferSize();
    if (hmdBufferSize.x == 0) {
        hmdBufferSize.x = 1844;
        hmdBufferSize.y = 1844;
    }

    hmdBufferSize.x *= cfg_gameMultiplier;
    hmdBufferSize.y *= cfg_gameMultiplier;

    //if (doLog) logError << "CreateWindow Pre : " << *(float*)((int)ecx + 0x16C) << " : " << *(float*)((int)ecx + 0x170) << std::endl;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
}

void(msub_6A08D0_post)(void* ecx)
{
    int xOffset = 0;// -(wtfSize.x / 2.0f);
    int yOffset = 0;// -(wtfSize.y / 2.0f);
    *(float*)((int)ecx + 0x16C) = wtfSize.y;
    *(float*)((int)ecx + 0x170) = wtfSize.x;
    *(float*)((int)ecx + 0x17C) = wtfSize.y;
    *(float*)((int)ecx + 0x180) = wtfSize.x;

    //----
    // Calculate the window border difference and resize the window to the given size
    //----
    HWND hWnd = *(HWND*)((int)ecx + 0x3968);
    //RECT rcClient, rcWind;
    //GetClientRect(hWnd, &rcClient);
    //GetWindowRect(hWnd, &rcWind);
    //POINT diff = { (rcWind.right - rcWind.left) - rcClient.right, (rcWind.bottom - rcWind.top) - rcClient.bottom };
    SetWindowLongA(hWnd, GWL_STYLE, GetWindowLongA(hWnd, GWL_STYLE) & ~(WS_BORDER | WS_SIZEBOX | WS_DLGFRAME));
    SetWindowPos(hWnd, 0, xOffset, yOffset, wtfSize.x, wtfSize.y, SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOSENDCHANGING);

    //if (doLog) logError << "CreateWindow Post : " << *(float*)((int)ecx + 0x16C) << " : " << *(float*)((int)ecx + 0x170) << std::endl;
}

// Create Window
void(__thiscall* sub_68EBB0)(void*, int) = (void(__thiscall*)(void*, int))0x0068EBB0;
void(__fastcall msub_68EBB0)(void* ecx, void* edx, int a)
{
    msub_6A08D0_pre();
    sub_68EBB0(ecx, a);
    msub_6A08D0_post(ecx);
}

void(__thiscall* sub_6A08D0)(void*, int) = (void(__thiscall*)(void*, int))0x006A08D0; // Ex
void(__fastcall msub_6A08D0)(void* ecx, void* edx, int a)
{
    msub_6A08D0_pre();
    sub_6A08D0(ecx, a);
    msub_6A08D0_post(ecx);
}

//----
// Create DX Device
//----
void msub_6A2040_pre(int a)
{
    //if (doLog) logError << "-- Create DX Device" << std::endl;
    wtfSize.x = *(int*)(a + 0x14);
    wtfSize.y = *(int*)(a + 0x18);

    defaultCutupResolution = false;
    if (wtfSize.x == 1824 && wtfSize.y == 475)
    {
        float recountOmenPercent = 1.0f - (1696.0f / 1920.f);

        defaultCutupResolution = true;
        POINT primarySize = monitors.GetPrimarySize();
        wtfSize.x = primarySize.x * (1.0f + recountOmenPercent);
        wtfSize.y = (wtfSize.x / 1824.0f) * 475;
    }
}

void msub_6A2040_post(void* ecx, bool* retVal)
{
    svr->StartVR();
    if (svr->HasErrors())
        logError << svr->GetErrors();

    if (svr->isEnabled())
    {
        //----
        // Gets the newly created dx device and layout
        //----
        devDX9 = (IDirect3DDevice9*)(*(int*)((int)ecx + 0x397C));
        screenLayout.hwnd = *(HWND*)((int)ecx + 0x3968);
        screenLayout.width = wtfSize.x;
        screenLayout.height = wtfSize.y;
        screenLayout.haveLayout = true;

        readConfigFile();
        CutupAddonFilesCopy(g_VR_PATH);

        //----
        // Saves the origional back and depth buffers
        //---
        curBackBufferLoc = ((int)ecx + 0x3B3C);
        origBackBuffer = *(int*)(curBackBufferLoc);
        origDepthBuffer = *(int*)(curBackBufferLoc + 0x4);
        D3DSURFACE_DESC bDesc;
        ((IDirect3DSurface9*)origBackBuffer)->GetDesc(&bDesc);
        //logError << "BackBuffer:" << std::endl;
        //logError << bDesc.Width << "x" << bDesc.Height << std::endl;
        //logError << "f:" << bDesc.Format << " : p:" << bDesc.Pool << " : t:" << bDesc.Type << " : u:" << bDesc.Usage << std::endl;

        ((IDirect3DSurface9*)origDepthBuffer)->GetDesc(&bDesc);
        //logError << "DepthBuffer:" << std::endl;
        //logError << bDesc.Width << "x" << bDesc.Height << std::endl;
        //logError << "f:" << bDesc.Format << " : p:" << bDesc.Pool << " : t:" << bDesc.Type << " : u:" << bDesc.Usage << std::endl;

        devDX11.createDevice();
        logError << devDX11.GetErrors();

        uiBufferSize = { screenLayout.width * cfg_uiMultiplier, screenLayout.height * cfg_uiMultiplier };
        CreateShaders(devDX9);
        CreateBuffers(devDX9, uiBufferSize);
        CreateTextures(devDX11.dev, devDX9, hmdBufferSize, uiBufferSize);

        /*
        if (doLog)
        {
            for (int monitorIndex = 0; monitorIndex < monitors.iMonitors.size(); monitorIndex++)
            {
                logError << std::dec << "Screen id: " << monitorIndex << std::endl;
                logError << "-----------------------------------------------------" << std::endl;
                logError << " - screen left-top corner coordinates : (" << monitors.rcMonitors[monitorIndex].left << "," << monitors.rcMonitors[monitorIndex].top << ")" << std::endl;
                logError << " - screen dimensions (width x height) : (" << std::abs(monitors.rcMonitors[monitorIndex].right - monitors.rcMonitors[monitorIndex].left) << "," << std::abs(monitors.rcMonitors[monitorIndex].top - monitors.rcMonitors[monitorIndex].bottom) << ")" << std::endl;
                logError << "-----------------------------------------------------" << std::endl;
            }
        }
        */

        clsMemManager mem = clsMemManager();
        if (mem.attachWindow(screenLayout.hwnd))
        {
            // nop delete epic code
            mem.writeMemoryL(0x97044C, 1, 0x90);
            mem.writeMemoryL(0x97044D, 1, 0x90);

            DWORD overrideLocation = 0x0082FDF6;
        }
        mem.closeProcess();

        isRunningAsAdmin = IsElevated();
        if (cfg_OSK && isRunningAsAdmin)
            RunOSKEnable();

        //setActionHandlesGame(&steamInput);
        if (*retVal) { *retVal = setActiveJSON(g_VR_PATH + "actions.json"); }
        if (*retVal) { *retVal = setActionHandlesGame(&input); }
        //if (retVal) { retVal = setActionHandlesMenu(&input); }
    }
}


bool(__thiscall* sub_6904D0)(void*, int) = (bool(__thiscall*)(void*, int))0x006904D0;
bool(__fastcall msub_6904D0)(void* ecx, void* edx, int a)
{
    msub_6A2040_pre(a);
    bool retVal = sub_6904D0(ecx, a);
    msub_6A2040_post(ecx, &retVal);
    return retVal;
}

// Create DX Device
bool(__thiscall* sub_6A2040)(void*, int) = (bool(__thiscall*)(void*, int))0x006A2040; // Ex
bool(__fastcall msub_6A2040)(void* ecx, void* edx, int a)
{
    msub_6A2040_pre(a);
    bool retVal = sub_6A2040(ecx, a);
    msub_6A2040_post(ecx, &retVal);
    return retVal;
}
//----
// Close DX Device
//----
void msub_6A1F40_pre()
{
    if (svr->isEnabled())
    {
        RunOSKDisable();

        DestroyTextures();
        DestroyBuffers();
        DestroyShaders();

        devDX11.Release();
        svr->StopVR();
    }
}

void msub_6A1F40_post()
{
    //if (doLog) logError << "-- Close DX Device" << std::endl;
}

void(__thiscall* sub_6903B0)(void*) = (void(__thiscall*)(void*))0x006903B0;
void(__fastcall msub_6903B0)(void* ecx, void* edx)
{
    msub_6A1F40_pre();
    sub_6903B0(ecx);
    msub_6A1F40_post();
}

void(__thiscall* sub_6A1F40)(void*) = (void(__thiscall*)(void*))0x006A1F40; // Ex
void(__fastcall msub_6A1F40)(void* ecx, void* edx)
{
    msub_6A1F40_pre();
    sub_6A1F40(ecx);
    msub_6A1F40_post();
}

// Begin SceneSetup
void(__thiscall* sub_6A73E0)(void*) = (void(__thiscall*)(void*))0x006A73E0;
void(__fastcall msub_6A73E0)(void* ecx, void* edx)
{
	sub_6A73E0(ecx);
}

// End SceneSetup
void(__thiscall* sub_6A7540)(void*) = (void(__thiscall*)(void*))0x006A7540;
void(__fastcall msub_6A7540)(void* ecx, void* edx)
{
	sub_6A7540(ecx);
}

// Present Scene
void(__thiscall* sub_6A7610)(void*) = (void(__thiscall*)(void*))0x006A7610;
void(__fastcall msub_6A7610)(void* ecx, void* edx)
{
    // Render:
    // End Scene
    // Begin Scene - Mouse
    // End Scene   - Mouse
    // Begin Scene
    sub_6A7610(ecx);
}







// Should Render Char
void(__thiscall* CGPlayer_C__ShouldRender)(void*, int, int, int) = (void(__thiscall*)(void*, int, int, int))0x006E0840;
void(__fastcall CGPlayer_C__ShouldRender_hk)(void* ecx_, void* edx_, int a, int b, int c)
{
    int showHidePlayer = 0;
    if (curEye == 0 || curEye == 1) {
        showHidePlayer = 1;
    }
    ((stObjectManager*)ecx_)->alpha4 = 255;

    int cameraAddress = CGWorldFrame__GetActiveCamera();
    float zoomLevel = *(float*)(cameraAddress + 0x118);
    if (zoomLevel == 0 && showHidePlayer == 1 && !cfg_showBodyFPS)
        *(DWORD*)0xC9D540 = false;
    else
        *(DWORD*)0xC9D540 = true;
    CGPlayer_C__ShouldRender(ecx_, a, b, c);
}

// Update Freelook Camera
void(__thiscall* sub_5FF530)(void*) = (void(__thiscall*)(void*))0x005FF530;
void(__fastcall msub_5FF530)(void* ecx, void* edx)
{
    fnUpdateCameraController((int)ecx);
    sub_5FF530(ecx);
}

// Update Camera Fn
void(__thiscall* CGCamera__CalcTargetCamera)(CGCamera*, int, int) = (void(__thiscall*)(CGCamera*, int, int))0x00606F90;
void(__fastcall CGCamera__CalcTargetCamera_hk)(CGCamera* camera, void* edx, int a, int b)
{
    CGCamera__CalcTargetCamera(camera, a, b);
    fnUpdateCameraHMD((int)camera);
}

// Slows animation value (frame timing?)
void (*World__Render)(int, float) = (void (*)(int, float))0x0077EFF0;
void (World__Render_hk)(int a, float b)
{
    b = b / 2.f;
    World__Render(a, b);
}

// Dynamic model animations
void(__thiscall* CM2Model__AnimateMT)(void*, int, int, int, int, int) = (void(__thiscall*)(void*, int, int, int, int, int))0x0082F0F0;
void(__fastcall CM2Model__AnimateMT_hk)(void* ecx, void* edx, int a, int b, int c, int d, int e)
{
    CM2Model__AnimateMT(ecx, a, b, c, d, e);
    
    if (gPlayerObj && gPlayerObj->pModelContainer == ecx)
        UpdateCharacterAnimation_post(gPlayerObj);
}

// Update Model Proj
void(__thiscall* CGxDevice__XformSetProjection)(void*, int) = (void(__thiscall*)(void*, int))0x006A9B40;
void(__fastcall CGxDevice__XformSetProjection_hk)(void* ecx, void* edx, int a)
{
    if (svr->isEnabled())
    {
        CGxDevice__XformSetProjection(ecx, a);

        int projMatrixAddr = *(int*)(0x0C5DF88);
        projMatrixAddr = projMatrixAddr + 0xFC8;

        if (curEye == 0 || curEye == 1) {
            if (*(float*)(projMatrixAddr + 0x3C) == 0) {
                XMMATRIX matProj = matProjection[curEye];
                //matProj._33 = uiOffsetD;// -0.938f;
                //matProj._34 = -0.06f;
                matProj._31 *= -1; matProj._32 *= -1; matProj._33 *= -1; matProj._34 *= -1;
                matProj._43 = *(float*)(projMatrixAddr + 0x38);

                memcpy((void*)projMatrixAddr, &matProj._11, 64);
                devDX9->SetTransform(D3DTS_PROJECTION, (D3DMATRIX*)&matProj._11);
            }
        }
    }
    else
    {
        CGxDevice__XformSetProjection(ecx, a);
    }
}

// Render Mouse
void(__thiscall* CGxDevice__ICursorDraw)(void*) = (void(__thiscall*)(void*))0x00687A90;
void(__fastcall CGxDevice__ICursorDraw_hk)(void* ecx, void* edx)
{
    CGxDevice__ICursorDraw(ecx);
}

// StartUI
void(__thiscall* CFrameStrata__RenderBatches)(int) = (void(__thiscall*)(int))0x00494F30;
void(__fastcall CFrameStrata__RenderBatches_hk)(void* ecx, void* edx)
{
    CFrameStrata__RenderBatches((int)ecx);
}

// Start Render
void (*CameraSetupScreenProjection)(float*, float*, int, int) = (void (*)(float*, float*, int, int))0x004BEE60;
void(__thiscall* CFrameStrata__BuildBatches)(int, int) = (void(__thiscall*)(int, int))0x00494EE0;

void(__thiscall* CSimpleTop__OnLayerRender)(void*) = (void(__thiscall*)(void*))0x00495410;
void(__fastcall CSimpleTop__OnLayerRender_hk)(void* ecx, void* edx)
{
    if (svr->isEnabled())
    {
        HRESULT result = S_OK;
        D3DVIEWPORT9 hViewport, uViewport, mViewport;
        XMMATRIX identityMatrix = XMMatrixIdentity();

        hViewport.X = 0;
        hViewport.Y = 0;
        hViewport.Width = hmdBufferSize.x;
        hViewport.Height = hmdBufferSize.y;
        hViewport.MinZ = 0.0f;
        hViewport.MaxZ = 1.0f;

        uViewport.X = 0;
        uViewport.Y = 0;
        uViewport.Width = uiBufferSize.x;
        uViewport.Height = uiBufferSize.y;
        uViewport.MinZ = 0.0f;
        uViewport.MaxZ = 1.0f;

        mViewport.X = 0;
        mViewport.Y = 0;
        mViewport.Width = uiBufferSize.x / 4;
        mViewport.Height = uiBufferSize.y / 4;
        mViewport.MinZ = 0.0f;
        mViewport.MaxZ = 1.0f;

        float a[] = { 0, 0, 0, 0 };
        float b[] = { 0, 0 };
        int esi = (int)ecx + 0x0CE4;
        int dxLoc = *(int*)(0x0C5DF88);

        //(float*)((int)ecx + 0x44)
        //a[0] = 0.1f; // fov values?
        //a[1] = 0.1f;
        //a[2] = 0.15f;
        //a[3] = 0.88f;

        b[0] = 0.0f; // screen coord center x,y offset -1:1
        b[1] = 0.0f; 

        //sub_4BEE60(a, b, 0, 0);
        CameraSetupScreenProjection((float*)((int)ecx + 0x44), b, 0, 0);

        //----
        // Checks to see if were in the game world or ui
        //----
        int frameWorld = *(int*)(0x00B7436C);

        std::tuple<int, IDirect3DSurface9*, IDirect3DSurface9*, int, int, D3DVIEWPORT9, D3DCOLOR> bufferList[] = { // left eye, right eye, ui
            std::make_tuple(textureIndex + 0, BackBuffer[textureIndex + 0].pShaderResource, DepthBuffer[textureIndex + 0].pDepthStencilView, 0, 1, hViewport, D3DCOLOR_ARGB(0, 0, 0, 0)),
            std::make_tuple(textureIndex + 3, BackBuffer[textureIndex + 3].pShaderResource, DepthBuffer[textureIndex + 3].pDepthStencilView, 0, 1, hViewport, D3DCOLOR_ARGB(0, 0, 0, 0)),
            std::make_tuple(0, uiRender.pShaderResource, uiDepth.pDepthStencilView, 1, 9, uViewport, D3DCOLOR_ARGB(0, 0, 0, 0))
        };
        int tIndex = 0;
        IDirect3DSurface9* renderTarget = NULL;
        IDirect3DSurface9* depthBuffer = NULL;
        int frameStart = 0;
        int frameStop = 0;
        D3DVIEWPORT9 viewport;
        D3DCOLOR clearColor = D3DCOLOR_ARGB(0, 0, 0, 0);

        int start = 0;
        int stop = 3;

        // Run Frames
        for (int j = start; j < stop; j++)
        {
            curEye = j;
            std::tie(tIndex, renderTarget, depthBuffer, frameStart, frameStop, viewport, clearColor) = bufferList[j];

            *(float*)(dxLoc + 0x164) = 0;
            *(float*)(dxLoc + 0x168) = 0;
            *(float*)(dxLoc + 0x16C) = (float)viewport.Height;
            *(float*)(dxLoc + 0x170) = (float)viewport.Width;
            *(float*)(dxLoc + 0x174) = 0;
            *(float*)(dxLoc + 0x178) = 0;
            *(float*)(dxLoc + 0x17C) = (float)viewport.Height;
            *(float*)(dxLoc + 0x180) = (float)viewport.Width;

            *(int*)(curBackBufferLoc) = (int)renderTarget;
            *(int*)(curBackBufferLoc + 0x4) = (int)depthBuffer;
            result = devDX9->SetRenderTarget(0, renderTarget);
            result = devDX9->SetDepthStencilSurface(depthBuffer);
            result = devDX9->SetViewport(&viewport);
            result = devDX9->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clearColor, 1.0f, 0);

            for (int i = frameStart; i < frameStop; i++)
            {
                int tBool = *(int*)((int)ecx + 0x7C);
                int tesi = *(int*)(esi + (i * 4));
                CFrameStrata__BuildBatches(tesi, tBool == 0);
                CFrameStrata__RenderBatches(tesi);
            }
        }

        devDX9->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        devDX9->SetRenderState(D3DRS_ZENABLE, TRUE);
        devDX9->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        //----
        // Renders masked ui
        //----
        result = devDX9->SetRenderTarget(0, uiRenderMask.pShaderResource);
        result = devDX9->SetDepthStencilSurface(NULL);
        result = devDX9->SetViewport(&mViewport);
        result = devDX9->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);

        result = devDX9->SetVertexShaderConstantF(0, &identityMatrix._11, 4);
        result = devDX9->SetVertexShaderConstantF(4, &identityMatrix._11, 4);
        result = devDX9->SetVertexShaderConstantF(8, &identityMatrix._11, 4);
        result = devDX9->SetTexture(0, uiRender.pTexture);
        maskedUI.Render();
        //devDX9->StretchRect(uiRender.pShaderResource, NULL, uiRenderMask.pShaderResource, NULL, D3DTEXF_NONE);

        devDX9->SetFVF(NULL);
        devDX9->SetRenderState(D3DRS_LIGHTING, FALSE);
        devDX9->SetRenderState(D3DRS_ZENABLE, (doOcclusion) ? TRUE : FALSE);
        devDX9->SetRenderState(D3DRS_ZWRITEENABLE, (doOcclusion) ? TRUE : FALSE);
        devDX9->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
        devDX9->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        devDX9->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        //devDX9->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
        devDX9->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESS);
        devDX9->SetRenderState(D3DRS_ALPHAREF, 0x00);
        devDX9->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        devDX9->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        devDX9->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);


        XMMATRIX projectionMatrix = XMMatrixIdentity();
        XMMATRIX viewMatrix = XMMatrixIdentity();
        XMMATRIX worldMatrix = XMMatrixIdentity();

        if (cfg_disableControllers)
        {
            RunFrameUpdateKeyboard();

            //----
            // Render cursor to to ui windows
            //----
            std::tie(tIndex, renderTarget, depthBuffer, frameStart, frameStop, viewport, clearColor) = bufferList[2];
            *(int*)(curBackBufferLoc) = (int)renderTarget;
            *(int*)(curBackBufferLoc + 0x4) = (int)depthBuffer;
            result = devDX9->SetRenderTarget(0, renderTarget);
            result = devDX9->SetDepthStencilSurface(depthBuffer);
            result = devDX9->SetViewport(&viewport);
            //result = devDX9->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB(255, 255, 255, 255), 1.0f, 0);

            //----
            // Renders mouseover cursor
            //----
            worldMatrix = cursorUI.GetObjectMatrix(false, true);
            result = devDX9->SetVertexShaderConstantF(0, &projectionMatrix._11, 4);
            result = devDX9->SetVertexShaderConstantF(4, &viewMatrix._11, 4);
            result = devDX9->SetVertexShaderConstantF(8, &worldMatrix._11, 4);
            result = devDX9->SetTexture(0, cursor.pTexture);
            cursorUI.Render();
            
            for (int i = 0; i < 2; i++)
            {
                //----
                // Render ui to game windows
                //----
                std::tie(tIndex, renderTarget, depthBuffer, frameStart, frameStop, viewport, clearColor) = bufferList[i];
                *(int*)(curBackBufferLoc) = (int)renderTarget;
                *(int*)(curBackBufferLoc + 0x4) = (int)depthBuffer;
                result = devDX9->SetRenderTarget(0, renderTarget);
                result = devDX9->SetDepthStencilSurface(depthBuffer);
                result = devDX9->SetViewport(&viewport);
                //result = devDX9->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB(255, 255, 255, 255), 1.0f, 0);

                XMMATRIX hmdData = matEyeOffset[i] * matHMDPos;
                projectionMatrix = XMMatrixTranspose(matProjection[i]);
                viewMatrix = XMMatrixTranspose(XMMatrixInverse(0, (hmdData)));
                worldMatrix = XMMatrixIdentity();
                projectionMatrix._33 = cfg_uiOffsetD;// -0.938f;
                //projectionMatrix._34 =  -0.06f;

                devDX9->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
                devDX9->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
                devDX9->SetRenderState(D3DRS_ZENABLE, FALSE);

                //----
                // Renders xyzGizmo pointer
                //----
                worldMatrix = xyzGizmo.GetObjectMatrix(false, true);
                result = devDX9->SetVertexShaderConstantF(0, &projectionMatrix._11, 4);
                result = devDX9->SetVertexShaderConstantF(4, &viewMatrix._11, 4);
                result = devDX9->SetVertexShaderConstantF(8, &worldMatrix._11, 4);
                xyzGizmo.Render(D3DPT_LINELIST);

                devDX9->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

                //----
                // Renders mouseover cursor
                //----
                worldMatrix = cursorWorld.GetObjectMatrix(false, true);
                result = devDX9->SetVertexShaderConstantF(0, &projectionMatrix._11, 4);
                result = devDX9->SetVertexShaderConstantF(4, &viewMatrix._11, 4);
                result = devDX9->SetVertexShaderConstantF(8, &worldMatrix._11, 4);
                result = devDX9->SetTexture(0, cursor.pTexture);
                cursorWorld.Render();

                devDX9->SetRenderState(D3DRS_ZENABLE, (doOcclusion) ? TRUE : FALSE);

                worldMatrix = curvedUI.GetObjectMatrix(false, true);
                result = devDX9->SetVertexShaderConstantF(0, &projectionMatrix._11, 4);
                result = devDX9->SetVertexShaderConstantF(4, &viewMatrix._11, 4);
                result = devDX9->SetVertexShaderConstantF(8, &worldMatrix._11, 4);
                result = devDX9->SetTexture(0, uiRender.pTexture);
                curvedUI.Render();

                devDX9->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            }
        }
        else
        {
            RunFrameUpdateController();

            //----
            // Render cursor to to ui windows
            //----
            std::tie(tIndex, renderTarget, depthBuffer, frameStart, frameStop, viewport, clearColor) = bufferList[2];
            *(int*)(curBackBufferLoc) = (int)renderTarget;
            *(int*)(curBackBufferLoc + 0x4) = (int)depthBuffer;
            result = devDX9->SetRenderTarget(0, renderTarget);
            result = devDX9->SetDepthStencilSurface(depthBuffer);
            result = devDX9->SetViewport(&viewport);
            //result = devDX9->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB(255, 255, 255, 255), 1.0f, 0);

            //----
            // Renders mouseover cursor
            //----
            worldMatrix = cursorUI.GetObjectMatrix(false, true);
            result = devDX9->SetVertexShaderConstantF(0, &projectionMatrix._11, 4);
            result = devDX9->SetVertexShaderConstantF(4, &viewMatrix._11, 4);
            result = devDX9->SetVertexShaderConstantF(8, &worldMatrix._11, 4);
            result = devDX9->SetTexture(0, cursor.pTexture);
            cursorUI.Render();

            //----
            // Icons for back of hand
            //----
            IDirect3DTexture9* watchShaderView[] =
            {
                handWatchList[0].pTexture, handWatchList[1].pTexture,
                handWatchList[2].pTexture, handWatchList[3].pTexture,
                handWatchList[4].pTexture, handWatchList[5].pTexture,
            };

            std::vector<uiViewport>* uiView = &uiViewGame;
            for (int i = 0; i < 2; i++)
            {
                //----
                // Render ui to game windows
                //----
                std::tie(tIndex, renderTarget, depthBuffer, frameStart, frameStop, viewport, clearColor) = bufferList[i];
                *(int*)(curBackBufferLoc) = (int)renderTarget;
                *(int*)(curBackBufferLoc + 0x4) = (int)depthBuffer;
                result = devDX9->SetRenderTarget(0, renderTarget);
                result = devDX9->SetDepthStencilSurface(depthBuffer);
                result = devDX9->SetViewport(&viewport);
                //result = devDX9->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB(255, 255, 255, 255), 1.0f, 0);

                XMMATRIX hmdData = matEyeOffset[i] * matHMDPos;
                projectionMatrix = XMMatrixTranspose(matProjection[i]);
                viewMatrix = XMMatrixTranspose(XMMatrixInverse(0, (hmdData)));
                worldMatrix = XMMatrixIdentity();
                projectionMatrix._33 = cfg_uiOffsetD;// -0.938f;
                //projectionMatrix._34 =  -0.06f;

                devDX9->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
                devDX9->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

                //----
                // Renders keyboard
                //----
                worldMatrix = oskUI.GetObjectMatrix(false, true);
                result = devDX9->SetVertexShaderConstantF(0, &projectionMatrix._11, 4);
                result = devDX9->SetVertexShaderConstantF(4, &viewMatrix._11, 4);
                result = devDX9->SetVertexShaderConstantF(8, &worldMatrix._11, 4);
                result = devDX9->SetTexture(0, oskTexture.pTexture);
                oskUI.Render();

                //----
                // Renders back of hand icons
                //----
                int watchCount = (doCutUI) ? handWatchCount : handWatchCount - 1;
                for (int i = 0; i < watchCount; i++)
                {
                    int activeOffset = ((handWatchAtUI[i]) ? 1 : 0);
                    worldMatrix = handWatchSquare[i].GetObjectMatrix(false, true);
                    result = devDX9->SetVertexShaderConstantF(0, &projectionMatrix._11, 4);
                    result = devDX9->SetVertexShaderConstantF(4, &viewMatrix._11, 4);
                    result = devDX9->SetVertexShaderConstantF(8, &worldMatrix._11, 4);
                    result = devDX9->SetTexture(0, watchShaderView[(i * 2) + activeOffset]);
                    handWatchSquare[i].Render();
                }

                //----
                // Renders the ray
                //----
                worldMatrix = rayLine.GetObjectMatrix(false, true);
                result = devDX9->SetVertexShaderConstantF(0, &projectionMatrix._11, 4);
                result = devDX9->SetVertexShaderConstantF(4, &viewMatrix._11, 4);
                result = devDX9->SetVertexShaderConstantF(8, &worldMatrix._11, 4);
                rayLine.Render(D3DPT_LINELIST);

                devDX9->SetRenderState(D3DRS_ZENABLE, FALSE);

                //----
                // Renders xyzGizmo pointer
                //----
                worldMatrix = xyzGizmo.GetObjectMatrix(false, true);
                result = devDX9->SetVertexShaderConstantF(0, &projectionMatrix._11, 4);
                result = devDX9->SetVertexShaderConstantF(4, &viewMatrix._11, 4);
                result = devDX9->SetVertexShaderConstantF(8, &worldMatrix._11, 4);
                xyzGizmo.Render(D3DPT_LINELIST);

                devDX9->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

                //----
                // Renders mouseover cursor
                //----
                worldMatrix = cursorWorld.GetObjectMatrix(false, true);
                result = devDX9->SetVertexShaderConstantF(0, &projectionMatrix._11, 4);
                result = devDX9->SetVertexShaderConstantF(4, &viewMatrix._11, 4);
                result = devDX9->SetVertexShaderConstantF(8, &worldMatrix._11, 4);
                result = devDX9->SetTexture(0, cursor.pTexture);
                cursorWorld.Render();

                devDX9->SetRenderState(D3DRS_ZENABLE, (doOcclusion) ? TRUE : FALSE);

                //----
                // Renders the ui
                //----
                if (doCutUI && !gPlayerObj)
                {
                    worldMatrix = cutUI.GetObjectMatrix(false, true);
                    result = devDX9->SetVertexShaderConstantF(0, &projectionMatrix._11, 4);
                    result = devDX9->SetVertexShaderConstantF(4, &viewMatrix._11, 4);
                    result = devDX9->SetVertexShaderConstantF(8, &worldMatrix._11, 4);
                    result = devDX9->SetTexture(0, uiRender.pTexture);
                    cutUI.Render();
                }
                else if (doCutUI && gPlayerObj)
                {
                    worldMatrix = cutUI.GetObjectMatrix(false, true);
                    result = devDX9->SetVertexShaderConstantF(0, &projectionMatrix._11, 4);
                    result = devDX9->SetVertexShaderConstantF(4, &viewMatrix._11, 4);
                    result = devDX9->SetVertexShaderConstantF(8, &worldMatrix._11, 4);
                    result = devDX9->SetTexture(0, uiRender.pTexture);
                    cutUI.Render();
                }
                else if (!doCutUI)
                {
                    worldMatrix = curvedUI.GetObjectMatrix(false, true);
                    result = devDX9->SetVertexShaderConstantF(0, &projectionMatrix._11, 4);
                    result = devDX9->SetVertexShaderConstantF(4, &viewMatrix._11, 4);
                    result = devDX9->SetVertexShaderConstantF(8, &worldMatrix._11, 4);
                    result = devDX9->SetTexture(0, uiRender.pTexture);
                    curvedUI.Render();
                }

                devDX9->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            }
        }

        devDX9->SetRenderState(D3DRS_ZENABLE, TRUE);
        devDX9->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    }
    else
    {
        CSimpleTop__OnLayerRender(ecx);
    }
}

// OnPrint
void(__thiscall* OnPaint)() = (void(__thiscall*)())0x004A8720;
void(__fastcall OnPaint_hk)()
{
    gPlayerObj = nullptr;
    isPossessing = false;
    if (svr->isEnabled())
    {
        gPlayerObj = ClntObjMgrGetActivePlayerObj();
        
        //----
        // Active Character is not the active player
        // Possessed something else?
        //----
        if (gPlayerObj && gPlayerObj->unknown11 && *(int*)(gPlayerObj->unknown11 + 0x770) != 0)
        {
            stObjectManager* activeObj = objManagerGetActiveObject();
            if (gPlayerObj != activeObj)
            {
                isPossessing = true;
                gPlayerObj = activeObj;
            }
        }

        int tIndex = textureIndex;
        svr->Render(BackBuffer11[tIndex].pTexture, DepthBuffer11[tIndex].pTexture, BackBuffer11[tIndex + 3].pTexture, DepthBuffer11[tIndex + 3].pTexture);
        if (svr->HasErrors())
            logError << svr->GetErrors();

        textureIndex = ((textureIndex + 1) % 3);
        svr->WaitGetPoses();
        svr->SetFramePose();

        matProjection[0] = (XMMATRIX)(svr->GetFramePose(poseType::Projection, 0)._m);
        matProjection[1] = (XMMATRIX)(svr->GetFramePose(poseType::Projection, 1)._m);

        matEyeOffset[0] = (XMMATRIX)(svr->GetFramePose(poseType::EyeOffset, 0)._m);
        matEyeOffset[1] = (XMMATRIX)(svr->GetFramePose(poseType::EyeOffset, 1)._m);

        matHMDPos = (XMMATRIX)(svr->GetFramePose(poseType::hmdPosition, -1)._m);
        matController[0] = (XMMATRIX)(svr->GetFramePose(poseType::LeftHand, -1)._m);
        matController[1] = (XMMATRIX)(svr->GetFramePose(poseType::RightHand, -1)._m);

        matControllerPalm[0] = (XMMATRIX)(svr->GetFramePose(poseType::LeftHandPalm, -1)._m);
        matControllerPalm[1] = (XMMATRIX)(svr->GetFramePose(poseType::RightHandPalm, -1)._m);

        if (gPlayerObj && gPlayerObj->pModelContainer->p20Container->ptr20)
        {
            int boneCount = *(int*)(gPlayerObj->pModelContainer->p20Container->ptr20 + 0x2C);
            int boneOffset = *(int*)(gPlayerObj->pModelContainer->p20Container->ptr20 + 0x30);
            bool reset = boneLookup.Set(boneCount, boneOffset);
        }

        int worldPanel = *(int*)0x00B499A8;
        if (worldPanel)
        {
            isOverUI = true;
            int worldFrame = *(int*)0x00B7436C;
            int mouseOverUiElement = *(int*)(worldPanel + 0x78);
            if (worldFrame && mouseOverUiElement == worldFrame)
                isOverUI = false;
        }
        else
        {
            isOverUI = true;
        }

        //----
        // Update near/far clip
        //----
        *(float*)0xADEED4 = 0.06f;
        //*(float*)0xCD7748 = 1000.0f;

        svr->SetProjection({ *(float*)0xADEED4, *(float*)0xCD7748 });

        RunControllerGame();
        OnPaint();

        resetPlayerAnimCounter = true;

        if (cfg_OSK && isRunningAsAdmin)
            RunOSKUpdate();

        //----
        // Copies texture to back buffer
        //----
        IDirect3DSurface9* pBackBuffer = nullptr;
        devDX9->GetBackBuffer(NULL, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);
        //devDX9->StretchRect(uiRender.pShaderResource, NULL, pBackBuffer, NULL, D3DTEXF_NONE);
        devDX9->StretchRect(BackBuffer[0].pShaderResource, NULL, pBackBuffer, NULL, D3DTEXF_NONE);
        
        if (hiddenTexture)
        {
            IDirect3DSurface9* hiddenSurface = nullptr;
            hiddenTexture->GetSurfaceLevel(0, &hiddenSurface);
            devDX9->StretchRect(hiddenSurface, NULL, pBackBuffer, NULL, D3DTEXF_NONE);
            hiddenSurface->Release();
        }
        pBackBuffer->Release();
    }
    else
    {
        OnPaint();
    }
    gPlayerObj = nullptr;
}

void SetMousePosition(HWND hwnd, int mouseX, int mouseY, bool forceMouse)
{
    HWND active = GetActiveWindow();
    POINT p = { 0, 0 };

    if (active == hwnd || forceMouse == true)
    {
        int mouseHold = *(int*)0x00D4156C;
        if (!isOverUI && mouseHold == 1)
        {
            p.x = *(int*)0x00D413EC;
            p.y = *(int*)0x00D413F0;
        }
        else
        {
            p.x = mouseX;
            p.y = mouseY;
            ClientToScreen(hwnd, &p);
        }

        SetCursorPos(p.x, p.y);
    }
}

void RunFrameUpdateSetCursor()
{
    float aspect = (float)screenLayout.width / (float)screenLayout.height;

    //----
    // Cursor
    //----
    static int currentCursorID = -9;
    int cursorID = *(int*)0x00C26DE8;
    if (cfg_disableControllers)
    {
        if (cursorID == 0 || cursorID == 1)
            cursorID = 28;
        else if (cursorID == 53)
            cursorID = 1;
    }
    else
    {
        if (cursorID == 1)
            cursorID = -1;
        else if (cursorID == 53)
            cursorID = 1;
    }

    if (currentCursorID != (cursorID - 1))
    {
        currentCursorID = (cursorID - 1);

        int W = cursor.width / 26;
        int H = cursor.height / 2;
        int x = (currentCursorID % 26) * W;
        int y = (currentCursorID / 26) * H; // determin if were on the top or bottom row

        float uv[] = { (float)x / cursor.width,
                       (float)y / cursor.height,
                       (float)(x + W) / cursor.width,
                       (float)(y + H) / cursor.height,
        };

        std::vector<float> squareData = {
                 0, -1,  0,    uv[0], uv[3],
                 0,  0,  0,    uv[0], uv[1],
                 1,  0,  0,    uv[2], uv[1],
                 1, -1,  0,    uv[2], uv[3],
        };
        cursorUI.SetVertexBuffer(squareData, 5, true);
        cursorWorld.SetVertexBuffer(squareData, 5, true);
    }

    float x = screenLayout.width / 2;
    float y = screenLayout.height / 2;
    cursorWorld.SetObjectMatrix(zeroScale);

    if (cfg_disableControllers)
    {
        POINT p = { 0, 0 };
        GetCursorPos(&p);
        ScreenToClient(screenLayout.hwnd, &p);
        x = ((float)p.x / screenLayout.width) * 2 - 1;
        y = -(((float)p.y / screenLayout.height) * 2 - 1);
        if (cursorID == 28)
            cursorID = -1;
    }
    XMMATRIX cursorScale = (cfg_disableControllers) ? XMMatrixScaling(0.075f / aspect, 0.075f, 0.075f) : zeroScale;
    XMMATRIX cursorOffset = XMMatrixTranslation(x, y, 0.0f);
    cursorUI.SetObjectMatrix(cursorScale * cursorOffset);

    XMMATRIX gizmoOffset = XMMatrixIdentity();
    if (cfg_disableControllers)
        gizmoOffset = (matHMDPos);
    else
        gizmoOffset = (matController[1]);

    xyzGizmo.SetObjectMatrix(zeroScale);
    if (gPlayerObj && !isOverUI)
    {
        float distance = 0;
        RayTarget rayTarget;
        int intersectType = IntersectGround(&rayTarget);
        stObjectManager* mouseOverObj = objManagerGetMouseoverObj();
        if (mouseOverObj && mouseOverObj->pModelContainer)
        {
            XMVECTOR camPos = cameraMatrixGame.r[3];
            XMVECTOR objPos = ((XMMATRIX)(mouseOverObj->pModelContainer->characterMatrix._m)).r[3];
            distance = XMVector3Length(objPos - camPos).vector4_f32[0];
        }
        else if (intersectType > 0)
        {
            distance = rayTarget.intersectDepth;
        }
        float depth = min(max(distance, 0.1f), 100.0f);
        gizmoOffset.r[3] = gizmoOffset.r[2] * -depth;
        XMMATRIX xyzGizmoScale = (distance > 0) ? XMMatrixScaling(depth / 800.0f, depth / 800.0f, depth / 800.0f) : zeroScale;
        xyzGizmo.SetObjectMatrix(xyzGizmoScale * gizmoOffset);

        XMMATRIX cursorScale = (cursorID > 0) ? XMMatrixScaling(depth / 40.f, depth / 40.f, depth / 40.f) : zeroScale;
        cursorWorld.SetObjectMatrix(cursorScale * gizmoOffset);
    }
}

void RunFrameUpdateController()
{
    struct intersectLayout
    {
        RenderObject* item;
        bool* atUI;
        stScreenLayout* layout;
        std::vector<intersectPoint> intersection;
        std::vector<bool> interaction;
        unsigned int multiplier;
        bool updateDistance;
        bool forceMouse;
        bool fromCenter;
    };

    byte uiClear[4] = { 0, 0, 0, 0 };
    bool curvedUIAtUI = false;
    bool oskAtUI = false;
 
    float aspect = (float)screenLayout.width / (float)screenLayout.height;
    POINT halfScreen = { screenLayout.width / 2, screenLayout.height / 2 };

    //----
    // Add all the interactable items to the intersect list
    //----
    std::list<intersectLayout> intersectList = std::list<intersectLayout>();
    XMMATRIX uiScaleMatrix = XMMatrixScaling(cfg_uiOffsetScale, cfg_uiOffsetScale, cfg_uiOffsetScale);
    XMMATRIX uiZMatrix = XMMatrixTranslation(0.0f, 0.0f, (cfg_uiOffsetZ / 100.0f));
    XMMATRIX moveMatrix = XMMatrixTranslation(0.0f, (cfg_uiOffsetY / 100.0f), 0.0f);
    XMMATRIX playerOffset = (uiScaleMatrix * uiZMatrix * moveMatrix);

    if (doCutUI)
    {
        std::vector<uiViewport>* uiView = &uiViewUI;
        if (!gPlayerObj)
        {
            //----
            // Sets up the cutup ui locations for rendering
            //----
            for (unsigned int i = 0; i < uiView->size(); i++)
            {
                uiView->at(i).calculateMatrix();
                uiView->at(i).matPosition = (uiScaleMatrix * uiZMatrix * moveMatrix) * uiView->at(i).matPosition;
                //uiView->at(i).matPosition = playerOffset * uiView->at(i).matPosition;
            }
        }
        else
        {
            uiView = &uiViewGame;

            //----
            // Sets up the cutup ui locations for rendering
            //----
            for (unsigned int i = 0; i < uiView->size(); i++)
            {
                uiView->at(i).calculateMatrix();
                uiView->at(i).matPosition = (uiScaleMatrix * uiZMatrix * moveMatrix) * uiView->at(i).matPosition;
                //uiView->at(i).matPosition = playerOffset * uiView->at(i).matPosition;
            }
            
            //uiView->at(uiCutupLayout::rightClick).matPosition *= matController[0]; // right click
            uiView->at(uiCutupLayout::handActionbar).calculateMatrix();
            uiView->at(uiCutupLayout::handActionbar).matPosition *= matController[0];
            uiView->at(uiCutupLayout::handActionbar).show = isCutActionShown;
            uiView->at(uiCutupLayout::handActionbar).enableDetect = isCutActionShown;

            uiView->at(uiCutupLayout::recountOmen).calculateMatrix();
            uiView->at(uiCutupLayout::recountOmen).matPosition *= matController[0];
            uiView->at(uiCutupLayout::recountOmen).show = !isCutActionShown;
            uiView->at(uiCutupLayout::recountOmen).enableDetect = !isCutActionShown;
            
            uiView->at(uiCutupLayout::mouseover).calculateMatrix();
            uiView->at(uiCutupLayout::mouseover).matPosition *= matController[1];
            
            //uiView->at(uiCutupLayout::onHead).calculateMatrix();
            //uiView->at(uiCutupLayout::onHead).matPosition *= matHMDPos;
            //uiView->at(uiCutupLayout::menu).calculateMatrix();
            //uiView->at(uiCutupLayout::menu).matPosition *= matHMDPos;

            XMMATRIX mouseoverPos = uiView->at(uiCutupLayout::mouseover).matPosition;
            XMMATRIX mouseoverScale = XMMatrixScaling(uiView->at(uiCutupLayout::mouseover).scale.x, uiView->at(uiCutupLayout::mouseover).scale.y, uiView->at(uiCutupLayout::mouseover).scale.z);
            XMMATRIX mouseoverOffset = XMMatrixTranslation(0.f, 0.15f, -0.2f);
            XMVECTOR mouseoverVec = { mouseoverPos._41, mouseoverPos._42, mouseoverPos._43 };
            mouseoverPos = XMMatrixIdentity();
            mouseoverPos._41 = mouseoverVec.vector4_f32[0];
            mouseoverPos._42 = mouseoverVec.vector4_f32[1];
            mouseoverPos._43 = mouseoverVec.vector4_f32[2];
            uiView->at(uiCutupLayout::mouseover).matPosition = mouseoverScale * mouseoverPos * mouseoverOffset;
        }

        //----
        // Each square has 2 triangles
        //----
        std::vector<bool> uiInteract = std::vector<bool>(uiView->size() * 2);
        std::vector<float> vertices = std::vector<float>();
        for (unsigned int i = 0; i < uiView->size(); i++)
        {
            uiView->at(i).setBufferStart((i * 6));

            float uv[] = { uiView->at(i).position.left / (float)(uiBufferSize.x),
                            uiView->at(i).position.top / (float)(uiBufferSize.y),
                            uiView->at(i).position.right / (float)(uiBufferSize.x),
                            uiView->at(i).position.bottom / (float)(uiBufferSize.y),
            };

            float W = (float)(uiView->at(i).position.right - uiView->at(i).position.left);
            float H = (float)(uiView->at(i).position.bottom - uiView->at(i).position.top);
            aspect = min(1.f / W, 1.f / H);
            float hW = (W * aspect);
            float hH = (H * aspect);

            if (!uiView->at(i).show)
            {
                hW = 0;
                hH = 0;
            }

            //----
            // Translates the coordinates of the squares by their matrix
            //----
            XMVECTOR v0 = XMVector3Transform({ -hW, -hH, 0.0f }, uiView->at(i).matPosition);
            XMVECTOR v1 = XMVector3Transform({ -hW,  hH, 0.0f }, uiView->at(i).matPosition);
            XMVECTOR v2 = XMVector3Transform({  hW,  hH, 0.0f }, uiView->at(i).matPosition);
            XMVECTOR v3 = XMVector3Transform({  hW,  hH, 0.0f }, uiView->at(i).matPosition);
            XMVECTOR v4 = XMVector3Transform({  hW, -hH, 0.0f }, uiView->at(i).matPosition);
            XMVECTOR v5 = XMVector3Transform({ -hW, -hH, 0.0f }, uiView->at(i).matPosition);

            float square[] = {
                            v0.vector4_f32[0], v0.vector4_f32[1], v0.vector4_f32[2],    uv[0], uv[3],
                            v1.vector4_f32[0], v1.vector4_f32[1], v1.vector4_f32[2],    uv[0], uv[1],
                            v2.vector4_f32[0], v2.vector4_f32[1], v2.vector4_f32[2],    uv[2], uv[1],
                            v3.vector4_f32[0], v3.vector4_f32[1], v3.vector4_f32[2],    uv[2], uv[1],
                            v4.vector4_f32[0], v4.vector4_f32[1], v4.vector4_f32[2],    uv[2], uv[3],
                            v5.vector4_f32[0], v5.vector4_f32[1], v5.vector4_f32[2],    uv[0], uv[3],
            };
            vertices.insert(vertices.end(), square, square + (6 * 5));

            uiInteract[i * 2 + 0] = uiView->at(i).enableDetect;
            uiInteract[i * 2 + 1] = uiView->at(i).enableDetect;
        }
        cutUI.SetVertexBuffer(vertices, 5, true);
        cutUI.SetObjectMatrix(XMMatrixIdentity());// cameraMatrix);
        //cutUI.SetObjectMatrix(uiScaleMatrix * uiZMatrix * moveMatrix);

        //stScreenLayout screenLayoutFixed = screenLayout;
        //screenLayoutFixed.width = 1920;
        //screenLayoutFixed.height = 1080;
        intersectList.push_back({ &cutUI, &curvedUIAtUI, &screenLayout, std::vector<intersectPoint>(), uiInteract, 1, isOverUI, false, true });
    }
    else if (!doCutUI)
    {
        XMMATRIX aspectScaleMatrix = XMMatrixScaling(aspect, 1, 1);
        curvedUI.SetObjectMatrix(aspectScaleMatrix * playerOffset);// *(uiScaleMatrix* uiZMatrix* moveMatrix));
        
        //              item, atUI, layout, intersection, dist, multiplier, updateDistance, forceMouse, fromCenter;
        intersectList.push_back({ &curvedUI, &curvedUIAtUI, &screenLayout, std::vector<intersectPoint>(), std::vector<bool>(), 1, isOverUI, false, true });
    }

    //----
    // OSK stuff
    //----
    Vector3 oskOffset = Vector3();
    aspect = 1.f;

    if (oskLayout != nullptr && oskLayout->haveLayout)
        aspect = (float)oskLayout->height / (float)oskLayout->width;

    XMMATRIX aspectScaleMatrixOSK = XMMatrixScaling(1, aspect, 1);
    XMMATRIX scaleMatrixOSK = XMMatrixScaling(0.4f, 0.4f, 1.f);
    XMMATRIX rotateMatrixOSK = XMMatrixRotationX(-30.0f * Deg2Rad);
    XMMATRIX moveMatrixOSK = XMMatrixTranslation(0.0f, -0.6f, 0.2f);
    XMMATRIX scaleOSK = (oskLayout != nullptr && oskLayout->haveLayout && showOSK) ? XMMatrixScaling(1.0f, 1.0f, 1.0f) : zeroScale;
    XMMATRIX offsetMatrixOSK = XMMatrixTranslation(oskOffset.x, oskOffset.y, oskOffset.z);
    oskUI.SetObjectMatrix(aspectScaleMatrixOSK * scaleOSK * scaleMatrixOSK * rotateMatrixOSK * moveMatrixOSK * (uiScaleMatrix * uiZMatrix * moveMatrix));// * scaleOSK);

    intersectList.push_back({ &oskUI, &oskAtUI, oskLayout, std::vector<intersectPoint>(), std::vector<bool>(), 1, true, true, false });

    //----
    // Back of hand squares
    //----
    XMMATRIX rotateX = XMMatrixRotationX(180.f * Deg2Rad);
    XMMATRIX rotateY = XMMatrixRotationY(90.f * Deg2Rad);
    XMMATRIX rotateZ = XMMatrixRotationZ(0.f * Deg2Rad);
    XMMATRIX viewportRot = rotateX * rotateY * rotateZ;

    int x = 0;
    int y = 0;
    int watchCount = (doCutUI) ? handWatchCount : handWatchCount - 1;
    for (int i = 0; i < watchCount; i++)
    {
        handWatchAtUI[i] = false;
        XMMATRIX ScaleMatrix = XMMatrixScaling(0.010f, 0.010f, 1.0f);
        XMMATRIX moveMatrix = XMMatrixTranslation(-0.14f + (x * 2) * 0.010f, 0.06f + (y * 2) * 0.010f, 0.0f);
        handWatchSquare[i].SetObjectMatrix((ScaleMatrix * moveMatrix) * viewportRot * matController[0]);
    
        intersectList.push_back({ &handWatchSquare[i], &handWatchAtUI[i], &screenLayout, std::vector<intersectPoint>(), std::vector<bool>(), 1, true, true, false});

        x++;
        if (x >= 3)
        {
            y++;
            x = 0;
        }
    }

    RunFrameUpdateSetCursor();
 
    //XMMATRIX rayMatrix = matController[1];
    XMMATRIX rayMatrix = (matController[1]);
    XMVECTOR origin = { rayMatrix._41, rayMatrix._42, rayMatrix._43 };
    XMVECTOR frwd = { rayMatrix._31, rayMatrix._32, rayMatrix._33 };
    XMVECTOR originS = origin + ((frwd * -1) * 0.1f);
    XMVECTOR end = origin + (frwd * -1);
    XMVECTOR norm = XMVector3Normalize(frwd);

    //----
    // Ray Data
    //----
    std::vector<float> lineData =
    {
        originS.vector4_f32[0], originS.vector4_f32[1], originS.vector4_f32[2], 1.0f, 0.0f, 0.0f, 1.0f,
        end.vector4_f32[0],    end.vector4_f32[1],    end.vector4_f32[2],    1.0f, 0.0f, 0.0f, 1.0f
    };

    //----
    // Go though all interactable items and check to see if the ray interacts with something
    //----
    int maskWidth = uiBufferSize.x / 4;
    int maskHeight = uiBufferSize.y / 4;

    float dist = -9999;
    intersectLayout closest = intersectLayout();
    int closestIndex = -1;
    HRESULT result = S_OK;
    //isOverUI = false;
    for (std::list<intersectLayout>::iterator it = intersectList.begin(); it != intersectList.end(); ++it)
    {
        if (it->layout == nullptr || it->layout->haveLayout)
        {
            *(it->atUI) = it->item->RayIntersection(originS, norm, &it->intersection, it->interaction, &logError);
            if(*(it->atUI) == true)
            {
                for (int i = 0; i < it->intersection.size(); i++)
                {
                    bool isOverUIElement = true;
                    if (oskAtUI || handWatchAtUI[0] || handWatchAtUI[1] || handWatchAtUI[2])
                    {
                        //isOverUI = false;
                    }
                    else if (curvedUIAtUI)
                    {
                        POINT tmpPos = { it->intersection[i].point.vector4_f32[0] * maskWidth, it->intersection[i].point.vector4_f32[1] * maskHeight };
                        RECT fromBox = { tmpPos.x, tmpPos.y, tmpPos.x + 1, tmpPos.y + 1 };
                        result = devDX9->StretchRect(uiRenderMask.pShaderResource, &fromBox, uiRenderCheck.pShaderResource, NULL, D3DTEXF_NONE);
                        result = devDX9->GetRenderTargetData(uiRenderCheck.pShaderResource, uiRenderCheckSystem.pShaderResource);

                        D3DLOCKED_RECT lr;
                        ZeroMemory(&lr, sizeof(D3DLOCKED_RECT));
                        result = uiRenderCheckSystem.pShaderResource->LockRect(&lr, 0, D3DLOCK_READONLY);
                        byte* pixel = (byte*)lr.pBits;
                        result = uiRenderCheckSystem.pShaderResource->UnlockRect();

                        if (pixel[0] == 0) // not over ui element
                            isOverUIElement = false;
                    }

                    if (isOverUIElement && it->intersection[i].distance >= dist)
                    {
                        dist = it->intersection[i].distance;
                        closest = *it;
                        closestIndex = i;
                        //isOverUI = true;
                    }
                }
            }
        }
    }

    if (closest.item != nullptr && closestIndex >= 0)
    {
        HWND useHWND = 0;
        if (closest.layout != nullptr)
        {
            halfScreen.x = (closest.layout->width / 2);
            halfScreen.y = (closest.layout->height / 2);

            //----
            // converts uv (0.0->1.0) to screen coords | width/height
            //----
            closest.intersection[closestIndex].point.vector4_f32[0] = closest.intersection[closestIndex].point.vector4_f32[0] * closest.layout->width;
            closest.intersection[closestIndex].point.vector4_f32[1] = closest.intersection[closestIndex].point.vector4_f32[1] * closest.layout->height;

            //----
            // Changes anchor from top left corner to middle of screen
            //----
            if (closest.fromCenter)
            {
                closest.intersection[closestIndex].point.vector4_f32[0] = halfScreen.x + (closest.intersection[closestIndex].point.vector4_f32[0] - halfScreen.x);
                closest.intersection[closestIndex].point.vector4_f32[1] = halfScreen.y + (closest.intersection[closestIndex].point.vector4_f32[1] - halfScreen.y);
            }
            useHWND = closest.layout->hwnd;
        }

        end = originS + (norm * dist);
        if (closest.updateDistance)
        {
            lineData[7] = end.vector4_f32[0];
            lineData[8] = end.vector4_f32[1];
            lineData[9] = end.vector4_f32[2];
        }

        SetMousePosition(useHWND, (int)closest.intersection[closestIndex].point.vector4_f32[0], (int)closest.intersection[closestIndex].point.vector4_f32[1], closest.forceMouse);
    }
    else// if (!curvedUIAtUI)
    {
        SetMousePosition(screenLayout.hwnd, halfScreen.x, halfScreen.y, false);
    }

    rayLine.SetVertexBuffer(lineData, 7, true);
}

void RunFrameUpdateKeyboard()
{
    float aspect = (float)screenLayout.width / (float)screenLayout.height;
    POINT halfScreen = { screenLayout.width / 2, screenLayout.height / 2 };

    XMMATRIX uiScaleMatrix = XMMatrixScaling(cfg_uiOffsetScale, cfg_uiOffsetScale, cfg_uiOffsetScale);
    XMMATRIX uiZMatrix = XMMatrixTranslation(0.0f, 0.0f, (cfg_uiOffsetZ / 100.0f));
    XMMATRIX moveMatrix = XMMatrixTranslation(0.0f, (cfg_uiOffsetY / 100.0f), 0.0f);
    XMMATRIX playerOffset = (uiScaleMatrix * uiZMatrix * moveMatrix);
    XMMATRIX aspectScaleMatrix = XMMatrixScaling(aspect, 1, 1);
    curvedUI.SetObjectMatrix(aspectScaleMatrix * playerOffset);

    RunFrameUpdateSetCursor();
}

// Skybox fix? - disable makes skybox work on all viewports but kills water
void(__thiscall* sub_6A38D0)(void*) = (void(__thiscall*)(void*))0x006A38D0;
void(__fastcall msub_6A38D0)(void* ecx, void* edx)
{
    static bool show = false;
    static bool doSBS = false;
    if (show) {
        sub_6A38D0(ecx);
    }
}

// GreyBoxes
void (*sub_796C10)(int, int, int, int, int) = (void (*)(int, int, int, int, int))0x00796C10;
void (msub_796C10)(int a, int b, int c, int d, int e)
{
    //sub_796C10(a, b, c, d, e);
}

// Login
static void AutoLogin()
{
    static bool s_once = false;
    if (s_once)
        return;
    s_once = true;

    uint32_t now = static_cast<uint32_t>(time(nullptr));

    if (!cfg_loginAccount.empty() && !cfg_loginPassword.empty())
    {
        *CGlueMgr::m_scandllOkayToLogIn = 1;
        *CSimpleTop::m_eventTime = now;

        CGlueMgr::SetCurrentAccount(cfg_loginAccount.c_str());
        NetClient::Login(cfg_loginAccount.c_str(), cfg_loginPassword.c_str());
    }
}

static void (*CGlueMgr__Resume)() = (decltype(CGlueMgr__Resume))0x004DA9AC;
static void __declspec(naked) CGlueMgr__Resume_hk()
{
    __asm {
        pop ebx;
        mov esp, ebp;
        pop ebp;

        pushad;
        pushfd;
        call AutoLogin;
        popfd;
        popad;
        ret;
    }
}

void InitDetours(HANDLE hModule)
{
    //if (doLog) logError << "-- InitDetours Start" << std::endl;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    DetourAttach((PVOID*)&CameraGetLineSegment, (PVOID)CameraGetLineSegment_hk); // Mouse to world ray caluclations

    DetourAttach((PVOID*)&sub_869DB0, (PVOID)msub_869DB0); // SetClientMouseResetPoint
    DetourAttach((PVOID*)&sub_684D70, (PVOID)msub_684D70); // Calculate window size
    DetourAttach((PVOID*)&sub_68EBB0, (PVOID)msub_68EBB0); // Create Window
    DetourAttach((PVOID*)&sub_6A08D0, (PVOID)msub_6A08D0); // Create Window Ex
    DetourAttach((PVOID*)&sub_6904D0, (PVOID)msub_6904D0); // Create DX Device
    DetourAttach((PVOID*)&sub_6A2040, (PVOID)msub_6A2040); // Create DX Device Ex
    DetourAttach((PVOID*)&sub_6903B0, (PVOID)msub_6903B0); // Close DX Device
    DetourAttach((PVOID*)&sub_6A1F40, (PVOID)msub_6A1F40); // Close DX Device Ex
    DetourAttach((PVOID*)&sub_6A73E0, (PVOID)msub_6A73E0); // Begin SceneSetup
    DetourAttach((PVOID*)&sub_6A7540, (PVOID)msub_6A7540); // End SceneSetup
    DetourAttach((PVOID*)&sub_6A7610, (PVOID)msub_6A7610); // Present Scene

    DetourAttach((PVOID*)&CGPlayer_C__ShouldRender, (PVOID)CGPlayer_C__ShouldRender_hk);            // Should Render Char
    DetourAttach((PVOID*)&sub_5FF530, (PVOID)msub_5FF530);                                          // Update freelook Camera
    DetourAttach((PVOID*)&CGCamera__CalcTargetCamera, (PVOID)CGCamera__CalcTargetCamera_hk);        // Update Camera Fn
    DetourAttach((PVOID*)&World__Render, (PVOID)World__Render_hk);                                  // Slows animation value
    DetourAttach((PVOID*)&CM2Model__AnimateMT, (PVOID)CM2Model__AnimateMT_hk);                      // Dynamic model animations
    DetourAttach((PVOID*)&CGxDevice__XformSetProjection, (PVOID)CGxDevice__XformSetProjection_hk);  // Update Model Proj
    DetourAttach((PVOID*)&CGxDevice__ICursorDraw, (PVOID)CGxDevice__ICursorDraw_hk);                // Render Mouse
    DetourAttach((PVOID*)&CFrameStrata__RenderBatches, (PVOID)CFrameStrata__RenderBatches_hk);      // StartUI
    DetourAttach((PVOID*)&CSimpleTop__OnLayerRender, (PVOID)CSimpleTop__OnLayerRender_hk);          // Start Render
    DetourAttach((PVOID*)&OnPaint, (PVOID)OnPaint_hk);                                              // OnPaint
    DetourAttach((PVOID*)&CGlueMgr__Resume, (PVOID)CGlueMgr__Resume_hk);                             // AutoLogin

    DetourAttach((PVOID*)&sub_6A38D0, (PVOID)msub_6A38D0); // Skybox fix?
    DetourAttach((PVOID*)&sub_796C10, (PVOID)msub_796C10); // GreyBox

    if (DetourTransactionCommit() == NO_ERROR)
        OutputDebugString("detoured successfully started");
    return;
}


void ExitDetours()
{
    //if (doLog) logError << "-- ExitDetours Start" << std::endl;
    /*
    logError << "s:" << cfg_uiOffsetScale << " : z:" << cfg_uiOffsetZ << " : y:" << cfg_uiOffsetY << " : d:" << cfg_uiOffsetD << std::endl;
    
    for (unsigned int i = 0; i < uiViewGame.size(); i++)
    {
        logError << i << ": o: " << uiViewGame.at(i).offset.x << ", " << uiViewGame.at(i).offset.y << ", " << uiViewGame.at(i).offset.z << std::endl;
        logError << i << ": r: " << uiViewGame.at(i).rotation.x << ", " << uiViewGame.at(i).rotation.y << ", " << uiViewGame.at(i).rotation.z << std::endl;
        logError << i << ": s: " << uiViewGame.at(i).scale.x << ", " << uiViewGame.at(i).scale.y << ", " << uiViewGame.at(i).scale.z << std::endl;
    }
    */
    
    
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    
    DetourDetach((PVOID*)&CameraGetLineSegment, (PVOID)CameraGetLineSegment_hk); // Mouse to world ray caluclations

    DetourDetach((PVOID*)&sub_869DB0, (PVOID)msub_869DB0); // SetClientMouseResetPoint
    DetourDetach((PVOID*)&sub_684D70, (PVOID)msub_684D70); // Calculate window size
    DetourDetach((PVOID*)&sub_68EBB0, (PVOID)msub_68EBB0); // Create Window
    DetourDetach((PVOID*)&sub_6A08D0, (PVOID)msub_6A08D0); // Create Window Ex
    DetourDetach((PVOID*)&sub_6904D0, (PVOID)msub_6904D0); // Create DX Device
    DetourDetach((PVOID*)&sub_6A2040, (PVOID)msub_6A2040); // Create DX Device Ex
    DetourDetach((PVOID*)&sub_6903B0, (PVOID)msub_6903B0); // Close DX Device
    DetourDetach((PVOID*)&sub_6A1F40, (PVOID)msub_6A1F40); // Close DX Device Ex
    DetourDetach((PVOID*)&sub_6A73E0, (PVOID)msub_6A73E0); // Begin SceneSetup
    DetourDetach((PVOID*)&sub_6A7540, (PVOID)msub_6A7540); // End SceneSetup
    DetourDetach((PVOID*)&sub_6A7610, (PVOID)msub_6A7610); // Present Scene

    DetourDetach((PVOID*)&CGPlayer_C__ShouldRender, (PVOID)CGPlayer_C__ShouldRender_hk);            // Should Render Char
    DetourDetach((PVOID*)&sub_5FF530, (PVOID)msub_5FF530);                                          // Update freelook Camera
    DetourDetach((PVOID*)&CGCamera__CalcTargetCamera, (PVOID)CGCamera__CalcTargetCamera_hk);        // Update Camera Fn
    DetourDetach((PVOID*)&World__Render, (PVOID)World__Render_hk);                                  // Slows animation value
    DetourDetach((PVOID*)&CM2Model__AnimateMT, (PVOID)CM2Model__AnimateMT_hk);                      // Dynamic model animations
    DetourDetach((PVOID*)&CGxDevice__XformSetProjection, (PVOID)CGxDevice__XformSetProjection_hk);  // Update Model Proj
    DetourDetach((PVOID*)&CGxDevice__ICursorDraw, (PVOID)CGxDevice__ICursorDraw_hk);                // Render Mouse
    DetourDetach((PVOID*)&CFrameStrata__RenderBatches, (PVOID)CFrameStrata__RenderBatches_hk);      // StartUI
    DetourDetach((PVOID*)&CSimpleTop__OnLayerRender, (PVOID)CSimpleTop__OnLayerRender_hk);          // Start Render
    DetourDetach((PVOID*)&OnPaint, (PVOID)OnPaint_hk);                                              // OnPaint

    DetourDetach((PVOID*)&sub_6A38D0, (PVOID)msub_6A38D0); // Skybox fix?
    DetourDetach((PVOID*)&sub_796C10, (PVOID)msub_796C10); // GreyBox

    if (DetourTransactionCommit() == NO_ERROR)
        OutputDebugString("detoured successfully stopped");
    return;
}



void (*moveForwardStart)() = (void (*)())0x005FC200;
void (*moveForwardStop)() = (void (*)())0x005FC250;
void (*moveBackwardStart)() = (void (*)())0x005FC290;
void (*moveBackwardStop)() = (void (*)())0x005FC2E0;

void (*moveLeftStart)() = (void (*)())0x005FC440;
void (*moveLeftStop)() = (void (*)())0x005FC490;
void (*moveRightStart)() = (void (*)())0x005FC4D0;
void (*moveRightStop)() = (void (*)())0x005FC520;

void (*turnLeftStart)() = (void (*)())0x005FC320;
void (*turnLeftStop)() = (void (*)())0x005FC360;
void (*turnRightStart)() = (void (*)())0x005FC3B0;
void (*turnRightStop)() = (void (*)())0x005FC3F0;

void (*jumpOrAscendStart)() = (void (*)())0x005FBF80;
void (*jumpOrAscendStop)() = (void (*)())0x005FC0A0;
void (*sitOrDescendStart)() = (void (*)())0x0051B1D0;
void (*sitOrDescendStop)() = (void (*)())0x005FC140;


bool rightStickXCenter = false;
bool rightStickYCenter = false;

void setVerticalRotation(float rotation)
{
    int camera = CGWorldFrame__GetActiveCamera();
    if (gPlayerObj && gPlayerObj->ptrObjectData && camera)
        gPlayerObj->ptrObjectData->objPitch = rotation;
}

void setHorizontalRotation(float rotation, float camOffset, bool mouseHold)
{
    int camera = CGWorldFrame__GetActiveCamera();
    if (gPlayerObj && gPlayerObj->ptrObjectData && camera)
    {
        CGMovementInfo__SetFacing((int)gPlayerObj->ptrObjectData, rotation);
        *(float*)(camera + 0x11C) = -(rotation - camOffset);
        if (mouseHold)
            *(float*)(camera + 0x11C) = camOffset;
    }
}

bool IsPlayerRunning()
{
    if (gPlayerObj && gPlayerObj->ptrObjectData)
        return ((gPlayerObj->ptrObjectData->MovementStatus & 0x100) == 0);
    return false;
}

float DiffObjFaceObj(stObjectManager* viewerObj, stObjectManager* targetObj)
{
    if (viewerObj && viewerObj->ptrObjectData && targetObj && targetObj->ptrObjectData)
    {
        Vector3 oV = viewerObj->ptrObjectData->objPos;
        Vector3 tV = targetObj->ptrObjectData->objPos;
        float curRotation = viewerObj->ptrObjectData->objRot;

        Vector3 distance;
        distance.x = tV.x - oV.x;
        distance.y = tV.y - oV.y;
        distance.z = tV.z - oV.z;

        float posRotation = EnsureProperRadians(std::atan2f(distance.y, distance.x));
        return posRotation;
        //if (std::fabs(posRotation - curRotation) > 0.5f)
        //    return posRotation - curRotation;
        //else
        //    return posRotation - gRotation;
    }
    return 0;
}


void RunControllerGame()
{
    //----
    // Set the current action set
    //----
    vr::VRActiveActionSet_t actionSet = { 0 };
    actionSet.ulActionSet = input.game.setHandle;
    uint32_t setSize = sizeof(actionSet);
    uint32_t setCount = setSize / sizeof(vr::VRActiveActionSet_t);

    vr::ETrackingUniverseOrigin eOrigin = vr::TrackingUniverseSeated;

    vr::InputDigitalActionData_t digitalActionData = { 0 };
    vr::InputAnalogActionData_t analogActionData = { 0 };
    vr::InputPoseActionData_t poseActionData = { 0 };

    static const int maxFrameDelay = 10;
    static int curFrameDelay = 0;
    static int mouseOverTargetOld = 0;
    static bool bumperPressed = false;
    static bool leftTriggerL = false;
    static bool leftTriggerR = false;
    static bool mouseButtonDownL = false;
    static bool mouseButtonDownR = false;
    

    bool haptic_left = false;
    bool haptic_right = false;

    bool isRunning = IsPlayerRunning();
    bool shouldRun = false;
    bool leftStickActive = false;
    bool rightStickActive = false;
    bool bumperPressedL = false;
    bool bumperPressedR = false;


    int camera = CGWorldFrame__GetActiveCamera();
    stObjectManager* targetObj = objManagerGetTargetObj();
    int eventTick = *(int*)0x00B499A4;
    int inputControl = *(int*)0x00C24954;
    int mouseHold = *(int*)0x00D4156C;
    int worldFrame = *(int*)0x00B7436C;
    float hRotationOffset = 0.0f;
    float targetFacing = 0.0f;

    int zoomSpeedOffset = *(int*)0x00C24E58;
    int zoomSpeedAddr = 0;
    if(zoomSpeedOffset)
        zoomSpeedAddr = (zoomSpeedOffset + 0x2C);

    //----
    // If we successfully update the actions this frame, use them
    //----
    if (vr::VRInput()->UpdateActionState(&actionSet, setSize, setCount) == vr::VRInputError_None) {
        //----
        // Check the data of the current action, if its valid and active perform the action
        //----
        if (vr::VRInput()->GetPoseActionDataForNextFrame(input.game.lefthand, eOrigin, &poseActionData, sizeof(poseActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && poseActionData.bActive == true)
            svr->SetActionPose(poseActionData.pose.mDeviceToAbsoluteTracking, poseType::LeftHand);

        if (vr::VRInput()->GetPoseActionDataForNextFrame(input.game.lefthandpalm, eOrigin, &poseActionData, sizeof(poseActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && poseActionData.bActive == true)
            svr->SetActionPose(poseActionData.pose.mDeviceToAbsoluteTracking, poseType::LeftHandPalm);

        if (vr::VRInput()->GetPoseActionDataForNextFrame(input.game.righthand, eOrigin, &poseActionData, sizeof(poseActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && poseActionData.bActive == true)
            svr->SetActionPose(poseActionData.pose.mDeviceToAbsoluteTracking, poseType::RightHand);

        if (vr::VRInput()->GetPoseActionDataForNextFrame(input.game.righthandpalm, eOrigin, &poseActionData, sizeof(poseActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && poseActionData.bActive == true)
            svr->SetActionPose(poseActionData.pose.mDeviceToAbsoluteTracking, poseType::RightHandPalm);

        if (vr::VRInput()->GetAnalogActionData(input.game.left_bumper, &analogActionData, sizeof(analogActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && analogActionData.bActive == true)
        {
            if (analogActionData.x > 0.25f)
                bumperPressedL = true;
            else if (analogActionData.x <= 0.25f)
                bumperPressedL = false;
        }

        if (vr::VRInput()->GetAnalogActionData(input.game.right_bumper, &analogActionData, sizeof(analogActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && analogActionData.bActive == true)
        {
            if (analogActionData.x > 0.25f)
                bumperPressedR = true;
            else if (analogActionData.x <= 0.25f)
                bumperPressedR = false;
        }

        bumperPressed = bumperPressedL || bumperPressedR;



        // Movement
        if (vr::VRInput()->GetAnalogActionData(input.game.movement, &analogActionData, sizeof(analogActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && analogActionData.bActive == true) {
            static bool lyRunning = false;
            static bool lxRunning = false;

            /*if (bumperPressedL && bumperPressedR)
            {
                uiViewGame.at(cfg_tID).scale.x += (analogActionData.x / 100.f);
                uiViewGame.at(cfg_tID).scale.y += (analogActionData.x / 100.f);
                //cfg_uiOffsetScale += (analogActionData.y / 50.0f);
            }
            else if (bumperPressedL)
            {
                uiViewGame.at(cfg_tID).offset.x += (analogActionData.x / 100.f);
                uiViewGame.at(cfg_tID).offset.y += (analogActionData.y / 100.f);
                //cfg_uiOffsetZ += (analogActionData.y / 50.0f);
            }
            else if (bumperPressedR)
            {
                uiViewGame.at(cfg_tID).offset.z += (analogActionData.y / 100.f);
                //uiViewGame.at(cfg_tID).rotation.y += analogActionData.x;
                //uiViewGame.at(cfg_tID).rotation.x += analogActionData.y;
                //cfg_uiOffsetD += (analogActionData.y / 1000.0f);
            }
            else*/
            {
                if (gPlayerObj)
                {
                    float hyp = std::sqrt(analogActionData.y * analogActionData.y + analogActionData.x * analogActionData.x);
                    if (hyp > 0.1f)
                    {
                        leftStickActive = true;
                        if (hyp > 0.5f)
                            shouldRun = true;

                        hRotationStickOffset = std::atan2(analogActionData.x * -1, analogActionData.y);
                        if (lxRunning == false)
                        {
                            lxRunning = true;
                            moveForwardStart();
                        }

                    }
                    else if (hyp <= 0.1f && lxRunning == true)
                    {
                        lxRunning = false;
                        moveForwardStop();
                    }
                }
            }
        }

        // Rotation
        if (vr::VRInput()->GetAnalogActionData(input.game.rotate, &analogActionData, sizeof(analogActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && analogActionData.bActive == true) {
            static bool lTurning = false;

            if (gPlayerObj)
            {
                if (cfg_snapRotateX)
                {
                    if (std::fabs(analogActionData.x) > 0.5f)
                    {
                        rightStickActive = true;
                        if (rightStickXCenter)
                        {
                            rightStickXCenter = false;
                            hRotationOffset = cfg_snapRotateAmountX * Deg2Rad * ((analogActionData.x > 0) - (analogActionData.x < 0));
                        }
                    }
                    else
                        rightStickXCenter = true;
                }
                else
                    if (std::fabs(analogActionData.x) > 0.2f)
                    {
                        rightStickActive = true;
                        hRotationOffset = (analogActionData.x / -25.0f);
                    }

                if (bumperPressedL)
                {
                    if (isOverUI)
                    {
                        //----
                        // update the scroll once every x frames to slow down the scroll speed
                        // while 2x the speed when the stick is pushed all the way
                        // only done while over a ui element
                        //----
                        if (curFrameDelay == 0)
                        {
                            if (analogActionData.y > 0.75f)
                                mouse_event(MOUSEEVENTF_WHEEL, 0, 0, 2, 0);
                            else if (analogActionData.y > 0.25f)
                                mouse_event(MOUSEEVENTF_WHEEL, 0, 0, 1, 0);

                            else if (analogActionData.y < -0.75f)
                                mouse_event(MOUSEEVENTF_WHEEL, 0, 0, -2, 0);
                            else if (analogActionData.y < -0.25f)
                                mouse_event(MOUSEEVENTF_WHEEL, 0, 0, -1, 0);
                        }
                    }
                    else
                    {
                        //----
                        // if in first person, dont zoom in or out with stick movement
                        //---
                        float zoomLevel = *(float*)(camera + 0x118);
                        if (zoomLevel != 0)
                        {
                            if (analogActionData.y > 0.25f)
                            {
                                *(float*)(camera + 0x118) = std::fminf(std::fmaxf(1.0f, zoomLevel - (abs(analogActionData.y - 0.25f) / 2.0f)), 50.0f);
                                *(float*)(camera + 0x1E8) = std::fminf(std::fmaxf(1.0f, zoomLevel - (abs(analogActionData.y - 0.25f) / 2.0f)), 50.0f);
                            }
                            else if (analogActionData.y < -0.25f)
                            {
                                *(float*)(camera + 0x118) = std::fminf(std::fmaxf(1.0f, zoomLevel + (abs(analogActionData.y - -0.25f) / 2.0f)), 50.0f);
                                *(float*)(camera + 0x1E8) = std::fminf(std::fmaxf(1.0f, zoomLevel + (abs(analogActionData.y - -0.25f) / 2.0f)), 50.0f);
                            }
                        }
                    }
                }
                else
                {
                    if (cfg_snapRotateY)
                    {
                        if (std::fabs(analogActionData.y) > 0.5f)
                        {
                            if (rightStickYCenter)
                            {
                                rightStickYCenter = false;
                                vRotationOffset = cfg_snapRotateAmountY * Deg2Rad * ((analogActionData.y > 0) - (analogActionData.y < 0));
                            }
                        }
                        else
                            rightStickYCenter = true;
                    }
                    else
                        if (std::fabs(analogActionData.y) > 0.2f)
                            vRotationOffset = (analogActionData.y / 25.0f);
                }
            }
        }

        if (vr::VRInput()->GetAnalogActionData(input.game.left_trigger, &analogActionData, sizeof(analogActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && analogActionData.bActive == true)
        {
            if (gPlayerObj)
            {
                if (bumperPressedL)
                {
                    if (leftTriggerL == true)
                    {
                        leftTriggerL = false;
                        setKeyUp((unsigned int)VK_DOWN);
                    }

                    if (analogActionData.x > 0.25f && leftTriggerR == false)
                    {
                        leftTriggerR = true;
                        setKeyDown((unsigned int)VK_UP);
                    }
                    else if (analogActionData.x < 0.25f && leftTriggerR == true)
                    {
                        leftTriggerR = false;
                        setKeyUp((unsigned int)VK_UP);
                    }
                }
                else
                {
                    if (leftTriggerR == true)
                    {
                        leftTriggerR = false;
                        setKeyUp((unsigned int)VK_UP);
                    }

                    if (analogActionData.x > 0.25f && leftTriggerL == false)
                    {
                        leftTriggerL = true;
                        setKeyDown((unsigned int)VK_DOWN);
                    }
                    else if (analogActionData.x < 0.25f && leftTriggerL == true)
                    {
                        leftTriggerL = false;
                        setKeyUp((unsigned int)VK_DOWN);
                    }
                }
            }
        }

        if (vr::VRInput()->GetDigitalActionData(input.game.left_stick_click, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bActive == true)
        {
            if (gPlayerObj)
            {
                if (bumperPressed)
                {
                    if (digitalActionData.bState == true && digitalActionData.bChanged == true)
                    {
                        if (targetObj)
                        {
                            targetFacing = DiffObjFaceObj(gPlayerObj, targetObj);
                            CGInputControl__SetControlBit(inputControl, 0x100, eventTick);
                            CGInputControl__UpdatePlayer(inputControl, eventTick, 1);
                            CGInputControl__UnsetControlBit(inputControl, 0x100, eventTick, 0);
                            CGInputControl__UpdatePlayer(inputControl, eventTick, 1);
                        }

                        setKeyDown((unsigned int)'1');
                    }
                    else if (digitalActionData.bState == false && digitalActionData.bChanged == true)
                        setKeyUp((unsigned int)'1');
                }
                else
                {
                    int mountCount = *(int*)0xBE8E08;
                    // Mount / Unmount
                    if (digitalActionData.bState == true && digitalActionData.bChanged == true)
                    {
                        lua_Dismount();
                        if (cfg_flyingMountID > 0 && mountCount > 0)
                            CastSpell(cfg_flyingMountID, 0, 0, 0, 0);
                    }
                    else if (digitalActionData.bState == false && digitalActionData.bChanged == true)
                    {
                        if (cfg_groundMountID > 0 && mountCount > 0)
                            CastSpell(cfg_groundMountID, 0, 0, 0, 0);
                        else if (mountCount > 0)
                        {
                            int randNum = rand() % mountCount;
                            int mountSpell = *(int*)((*(int*)0xBE8E0C) + (randNum * 4));
                            CastSpell(mountSpell, 0, 0, 0, 0);
                        }
                    }
                }
            }
        }


        if (vr::VRInput()->GetAnalogActionData(input.game.right_trigger, &analogActionData, sizeof(analogActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && analogActionData.bActive == true)
        {
            int var = 0;
            if (bumperPressedR)
            {
                if (mouseButtonDownL)
                {
                    mouseButtonDownL = true;
                    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                }

                // Right Click
                if (analogActionData.x > 0.25f && mouseButtonDownR == false)
                {
                    mouseButtonDownR = true;
                    mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
                }
                else if (analogActionData.x <= 0.25f && mouseButtonDownR == true)
                {
                    mouseButtonDownR = false;
                    mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
                }
            }
            else
            {
                if (mouseButtonDownR)
                {
                    mouseButtonDownR = false;
                    mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
                }

                // Left Click
                if (analogActionData.x > 0.25f && mouseButtonDownL == false)
                {
                    mouseButtonDownL = true;
                    if (handWatchAtUI[0]) // occlusion
                        doOcclusion = !doOcclusion;
                    else if (handWatchAtUI[1]) // keyboard
                        showOSK = !showOSK;
                    else if (handWatchAtUI[2]) // menu swap
                        isCutActionShown = !isCutActionShown;
                    else
                        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                }
                else if (analogActionData.x <= 0.25f && mouseButtonDownL == true)
                {
                    mouseButtonDownL = false;
                    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                }
            }
        }


        if (vr::VRInput()->GetDigitalActionData(input.game.right_stick_click, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bActive == true)
        {
            if (bumperPressed && gPlayerObj)
            {
                // Hotbar Button 10
                if (digitalActionData.bState == true && digitalActionData.bChanged == true)
                {
                    if (targetObj)
                    {
                        targetFacing = DiffObjFaceObj(gPlayerObj, targetObj);
                        CGInputControl__SetControlBit(inputControl, 0x100, eventTick);
                        CGInputControl__UpdatePlayer(inputControl, eventTick, 1);
                        CGInputControl__UnsetControlBit(inputControl, 0x100, eventTick, 0);
                        CGInputControl__UpdatePlayer(inputControl, eventTick, 1);
                    }

                    setKeyDown((unsigned int)'0');
                }
                else if (digitalActionData.bState == false && digitalActionData.bChanged == true)
                    setKeyUp((unsigned int)'0');
            }
            else
            {
                // Toggle First/Third
                if (digitalActionData.bState == true && digitalActionData.bChanged == true)
                {
                    readConfigFile();
                    //cfg_tID = cfg_groundMountID;
                    svr->Recenter();
                    if (camera && gPlayerObj)
                    {
                        float zoomLevel = *(float*)(camera + 0x118);
                        if (zoomLevel == 0)
                        {
                            *(float*)(camera + 0x118) = 10;
                            *(float*)(camera + 0x1E8) = 10;
                        }
                        else
                        {
                            *(float*)(camera + 0x118) = 0;
                            *(float*)(camera + 0x1E8) = 0;
                        }
                    }
                }
            }
        }

        if (vr::VRInput()->GetDigitalActionData(input.game.button_a, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bActive == true)
        {
            if (gPlayerObj)
            {
                if (bumperPressed)
                {
                    // Hotbar Button 9
                    if (digitalActionData.bState == true && digitalActionData.bChanged == true)
                    {
                        if (targetObj)
                        {
                            targetFacing = DiffObjFaceObj(gPlayerObj, targetObj);
                            CGInputControl__SetControlBit(inputControl, 0x100, eventTick);
                            CGInputControl__UpdatePlayer(inputControl, eventTick, 1);
                            CGInputControl__UnsetControlBit(inputControl, 0x100, eventTick, 0);
                            CGInputControl__UpdatePlayer(inputControl, eventTick, 1);
                        }

                        setKeyDown((unsigned int)'9');
                    }
                    else if (digitalActionData.bState == false && digitalActionData.bChanged == true)
                        setKeyUp((unsigned int)'9');
                }
                else
                {
                    // Jump
                    if (digitalActionData.bState == true && digitalActionData.bChanged == true)
                        jumpOrAscendStart();
                    else if (digitalActionData.bState == false && digitalActionData.bChanged == true)
                        jumpOrAscendStop();
                }
            }
        }

        if (vr::VRInput()->GetDigitalActionData(input.game.button_b, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bActive == true)
        {
            if (gPlayerObj)
            {
                if (bumperPressed)
                {
                    // Hotbar Button 8
                    if (digitalActionData.bState == true && digitalActionData.bChanged == true)
                    {
                        if (targetObj)
                        {
                            targetFacing = DiffObjFaceObj(gPlayerObj, targetObj);
                            CGInputControl__SetControlBit(inputControl, 0x100, eventTick);
                            CGInputControl__UpdatePlayer(inputControl, eventTick, 1);
                            CGInputControl__UnsetControlBit(inputControl, 0x100, eventTick, 0);
                            CGInputControl__UpdatePlayer(inputControl, eventTick, 1);
                        }

                        setKeyDown((unsigned int)'8');
                    }
                    else if (digitalActionData.bState == false && digitalActionData.bChanged == true)
                        setKeyUp((unsigned int)'8');
                }
                else
                {
                    int var[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
                    if (digitalActionData.bState == true && digitalActionData.bChanged == true)
                        lua_TargetNearestEnemy(var);
                }
            }
        }

        if (vr::VRInput()->GetDigitalActionData(input.game.button_x, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bActive == true)
        {
            if (gPlayerObj)
            {
                if (bumperPressed)
                {
                    // Hotbar Button 2
                    if (digitalActionData.bState == true && digitalActionData.bChanged == true)
                    {
                        if (targetObj)
                        {
                            targetFacing = DiffObjFaceObj(gPlayerObj, targetObj);
                            CGInputControl__SetControlBit(inputControl, 0x100, eventTick);
                            CGInputControl__UpdatePlayer(inputControl, eventTick, 1);
                            CGInputControl__UnsetControlBit(inputControl, 0x100, eventTick, 0);
                            CGInputControl__UpdatePlayer(inputControl, eventTick, 1);
                        }
                        setKeyDown((unsigned int)'2');
                    }
                    else if (digitalActionData.bState == false && digitalActionData.bChanged == true)
                        setKeyUp((unsigned int)'2');
                }
                else
                {
                    // Map
                    if (digitalActionData.bState == true && digitalActionData.bChanged == true)
                        setKeyDown((unsigned int)'M');
                    else if (digitalActionData.bState == false && digitalActionData.bChanged == true)
                        setKeyUp((unsigned int)'M');
                }
            }
        }

        if (vr::VRInput()->GetDigitalActionData(input.game.button_y, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bActive == true)
        {
            if (bumperPressed && gPlayerObj)
            {
                // Hotbar Button 3
                if (digitalActionData.bState == true && digitalActionData.bChanged == true)
                {
                    if (targetObj)
                    {
                        targetFacing = DiffObjFaceObj(gPlayerObj, targetObj);
                        CGInputControl__SetControlBit(inputControl, 0x100, eventTick);
                        CGInputControl__UpdatePlayer(inputControl, eventTick, 1);
                        CGInputControl__UnsetControlBit(inputControl, 0x100, eventTick, 0);
                        CGInputControl__UpdatePlayer(inputControl, eventTick, 1);
                    }

                    setKeyDown((unsigned int)'3');
                }
                else if (digitalActionData.bState == false && digitalActionData.bChanged == true)
                    setKeyUp((unsigned int)'3');
            }
            else
            {
                // Escape
                if (digitalActionData.bState == true && digitalActionData.bChanged == true)
                    setKeyDown((unsigned int)VK_ESCAPE);
                else if (digitalActionData.bState == false && digitalActionData.bChanged == true)
                    setKeyUp((unsigned int)VK_ESCAPE);
            }
        }

        if (vr::VRInput()->GetDigitalActionData(input.game.menu_select, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bActive == true)
        {
            if (gPlayerObj)
            {
                if (bumperPressed)
                {

                }
                else
                {

                }
            }
        }

        if (vr::VRInput()->GetDigitalActionData(input.game.menu_start, &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && digitalActionData.bActive == true)
        {
            if (gPlayerObj)
            {
                if (bumperPressed)
                {

                }
                else
                {

                }
            }
        }

        //----
        // Checks the mouse over target for vibration in the world
        // when an item is highlighted
        //----
        if (worldFrame)
        {
            int mouseOverTarget = *(int*)(worldFrame + 0x2C8);
            if (mouseOverTargetOld != mouseOverTarget)
            {
                mouseOverTargetOld = mouseOverTarget;
                if (mouseOverTarget != 0)
                    haptic_right = true;
            }
        }

        if (haptic_left)
            vr::VRInput()->TriggerHapticVibrationAction(input.game.haptic_left, 0, 0.1f, 1.0f, 0.25f, vr::k_ulInvalidInputValueHandle);
        if (haptic_right)
            vr::VRInput()->TriggerHapticVibrationAction(input.game.haptic_right, 0, 0.1f, 1.0f, 0.25f, vr::k_ulInvalidInputValueHandle);


        bool isFSF = false;
        int objData = 0;
        if (gPlayerObj)
        {
            objData = (int)gPlayerObj->ptrObjectData;
            //if (objData)
            //    isFSF = IsFallingSwimmingFlying(objData);
        }

        gRotation += hRotationOffset;
        gCamRotation += hRotationOffset;
        XMVECTOR angles = XMVECTOR();
        XMVECTOR anglesPalm = XMVECTOR();

        //----
        // use headbased onward rather than controller
        //----
        if ((cfg_hmdOnward & 1) == 1)
            angles = GetAngles(matHMDPos);
        else
            angles = GetAngles(matController[0]);

        if ((cfg_hmdOnward & 2) == 2)
        {
            anglesPalm = GetAngles(matHMDPos);
            setVerticalRotation(-anglesPalm.vector4_f32[0]);
        }
        else
        {
            anglesPalm = GetAngles(matControllerPalm[0]);
            setVerticalRotation(-anglesPalm.vector4_f32[0] - 0.2f);
        }
        
        static float oldOnwardYaw = 0;
        float onwardDiff = 0;
        onwardDiff = angles.vector4_f32[1] - oldOnwardYaw;
        if(leftStickActive)
        {
            gRotation = EnsureProperRadians(gRotation - onwardDiff);
            oldOnwardYaw = angles.vector4_f32[1];
        }
        

        if (std::fabs(targetFacing) > 0)
        {
            setHorizontalRotation(targetFacing, gCamRotation, mouseHold);
        }
        else if (leftStickActive || rightStickActive)
        {
            setHorizontalRotation(gRotation + hRotationStickOffset, gCamRotation, mouseHold);
            if (objData)
                CalculateForwardMovement(objData, (int)shouldRun);
        }

        curFrameDelay++;
        if (curFrameDelay > maxFrameDelay)
            curFrameDelay = 0;
    }
}
