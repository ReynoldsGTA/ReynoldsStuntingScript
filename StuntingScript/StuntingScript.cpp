#include "plugin.h"
#include "CPed.h"
#include "CBike.h"
#include "CVehicle.h"
#include "CReplay.h"
#include "CHud.h"
#include "CWorld.h"
#include "CCheat.h"
#include "CTimer.h"
#include "CFont.h"
#include "CStreaming.h"
#include "CModelInfo.h"
#include "extensions/ScriptCommands.h"
#include "CWeather.h"


using namespace plugin;

struct PositionedVehicle {
    float heading;
    CVector position;
    int ID;
    CVehicle* vehicle;
};

struct SavedPosition {
    float heading;
    CVector position;
};

class StuntingScript {
public:
    static int m_keyPressTime;

    static SavedPosition m_position1;
    static SavedPosition m_position2;
    
    static CPed* m_player;
    static CVehicle* m_vehicle;
    static CVehicle* m_lastVehicle;

    static CVector m_oldPos;

    static float m_pedDensity;

    static bool m_jetPackMode;
    static bool m_ghostTown;

    static bool m_posVehicleStored;
    static PositionedVehicle m_posVehicle;

    static unsigned m_currentFrame;

    static float m_speeds[5];


    static void savePosition(CPed* player, CVehicle* vehicle, SavedPosition& savePos) {
        if (vehicle) {
            savePos.heading = vehicle->GetHeading();
            savePos.position = vehicle->GetPosition();
        }
        else {
            savePos.heading = player->GetHeading();
            savePos.position = player->GetPosition();
        }
    }

    static void loadPosition(CPed* player, CVehicle* vehicle, CVehicle* lastVehicle, SavedPosition savePos) {
        
        if (!vehicle && lastVehicle) {
            Command<Commands::WARP_CHAR_INTO_CAR>(player, lastVehicle);
            vehicle = lastVehicle;
        }
        if (vehicle) {
            vehicle->SetPosn(savePos.position);
            Command<Commands::SET_CAR_COORDINATES>(vehicle, savePos.position.x, savePos.position.y, savePos.position.z);
            vehicle->SetHeading(savePos.heading);
        }
        Command<Commands::RESTORE_CAMERA_JUMPCUT>();
    }

    /*This does not get a proper offset*/
    static void spawnVehicle(int model, float x, float y) {
        Command<Commands::REQUEST_MODEL>(model);
        Command<Commands::LOAD_ALL_MODELS_NOW>();
        float a, b, c;
        Command<Commands::GET_OFFSET_FROM_CHAR_IN_WORLD_COORDS>(m_player, x, y, 0.0, &a, &b, &c);
        CVehicle* veh;
        Command<Commands::CREATE_CAR>(model, a, b, c, veh);
    }



    static void processJetPack() {
        if (KeyPressed('W')) {
            float a, b, c;
            Command<Commands::GET_OFFSET_FROM_CHAR_IN_WORLD_COORDS>(m_player, 0.0, 1.0, 0.0, &a, &b, &c);
            m_player->SetPosn(CVector(a, b, c));
        }

        if (KeyPressed('S')) {
            float a, b, c;
            Command<Commands::GET_OFFSET_FROM_CHAR_IN_WORLD_COORDS>(m_player, 0.0, -1.0, 0.0, &a, &b, &c);
            m_player->SetPosn(CVector(a, b, c));
        }

        if (KeyPressed('D')) {
            float h = m_player->GetHeading();
            m_player->SetHeading(h - 0.02f);
            Command<Commands::RESTORE_CAMERA_JUMPCUT>();
        }

        if (KeyPressed('A')) {
            float h = m_player->GetHeading();
            m_player->SetHeading(h + 0.02f);
            Command<Commands::RESTORE_CAMERA_JUMPCUT>();
        }

        if (KeyPressed(VK_SPACE)) {
            CVector curPos = m_player->GetPosition();
            m_player->SetPosn(curPos + CVector(0.0, 0.0, 1.0));
        }

        if (KeyPressed('C')) {
            CVector curPos = m_player->GetPosition();
            m_player->SetPosn(curPos - CVector(0.0, 0.0, 1.0));
        }
    }

    static void rememberVehicle(CVehicle* vehicle) {
        int model;
        CVector pos;
        float heading;
        if (vehicle) {
            model = vehicle->m_nModelIndex;
            pos = vehicle->GetPosition();
            heading = vehicle->GetHeading();

            Command<Commands::REQUEST_MODEL>(model);
            Command<Commands::LOAD_ALL_MODELS_NOW>();

            CVehicle* veh = nullptr;
            Command<Commands::CREATE_CAR>(model, pos.x, pos.y, pos.z, veh);

            Command<Commands::WARP_CHAR_FROM_CAR_TO_COORD>(m_player, pos.x, pos.y, pos.z);
            
            if (veh) {
                veh->SetPosn(pos);
                veh->SetHeading(heading);
                Command<Commands::WARP_CHAR_INTO_CAR>(m_player, veh);
                Command<Commands::RESTORE_CAMERA_JUMPCUT>();
            }
            
        }
    }

    static void setPositionedVehicle(CVehicle* vehicle) {
        m_posVehicle.vehicle = vehicle;
        m_posVehicle.position = vehicle->GetPosition();
        m_posVehicle.heading = vehicle->GetHeading();
        m_posVehicle.ID = vehicle->m_nModelIndex;
        m_posVehicleStored = true;
    }

    static void respawnPositionedVehicle(PositionedVehicle positionedVehicle) {
        if (m_posVehicleStored) {
            if (positionedVehicle.vehicle) {
                positionedVehicle.vehicle->SetPosn(positionedVehicle.position);
                positionedVehicle.vehicle->SetHeading(positionedVehicle.heading);
                positionedVehicle.vehicle->m_nVehicleFlags.bEngineOn = true;
            }
            else {
                CHud::SetHelpMessage("Vehicle Gone!", true, false, false);
            }
        }
    }

    static void addNitroToCar() {
        if (m_vehicle) {
            Command<Commands::SET_CAR_HYDRAULICS>(m_vehicle, 1);
            Command<Commands::GIVE_NON_PLAYER_CAR_NITRO>(m_vehicle);
            m_vehicle->m_nNitroBoosts = 10;
        }
    }

    static void toggleJetPack() {
        m_jetPackMode = !m_jetPackMode;
        //Command<Commands::FREEZE_CHAR_POSITION>(m_player, m_jetPackMode);
        Command<Commands::FREEZE_CHAR_POSITION_AND_DONT_LOAD_COLLISION>(m_player, m_jetPackMode);
        if (m_jetPackMode) {
            CHud::SetHelpMessage("Jetpack On", true, false, false);
        }
        else {
            CHud::SetHelpMessage("Jetpack Off", true, false, false);
        }
    }

    static void toggleGhostTown() {
        m_ghostTown = !m_ghostTown;
        if (m_ghostTown) {
            m_pedDensity = 0.0f;
            CHud::SetHelpMessage("Ghosttown On", true, false, false);
        }
        else {
            m_pedDensity = 1.0f;
            CHud::SetHelpMessage("Ghosttown Off", true, false, false);
        }
    }

    static void popTire(eWheels wheel) {
        Command<Commands::BURST_CAR_TYRE>(m_vehicle, wheel);
        CHud::SetHelpMessage("Popped tire", true, false, false);
    }

    static void changeWeather() {
        Command<Commands::FORCE_WEATHER>(eWeatherType::WEATHER_SUNNY_LA);
    }

    static void process() {
        //this will be processed once per frame
        m_player = FindPlayerPed();
        m_vehicle = FindPlayerVehicle(0, false);

        if (m_vehicle) {
            m_lastVehicle = m_vehicle;
            m_vehicle->m_fHealth = 1000.0; 

        } 
        Command<Commands::SET_PLAYER_INVINCIBLE>(m_player, 1);
        Command<Commands::SET_CAR_DENSITY_MULTIPLIER>(m_pedDensity);
        Command<Commands::SET_PED_DENSITY_MULTIPLIER>(m_pedDensity);
        m_player->m_fHealth = 250.0;
        Command<Commands::SET_MAX_WANTED_LEVEL>(0);
       
        

        if( (CTimer::m_snTimeInMilliseconds - m_keyPressTime) > 500) {
            bool usedCommand = true;

            if (KeyPressed('R') && KeyPressed('M')) {
                rememberVehicle(m_vehicle);
                CHud::SetHelpMessage("Car Remembered", true, false, false);
            } else if (KeyPressed(VK_CONTROL) && KeyPressed(VK_SHIFT) && KeyPressed('A')) {
                loadPosition(m_player, m_vehicle, m_lastVehicle, m_position1);
                CHud::SetHelpMessage("Load Position 1", true, false, false);
            } else if (KeyPressed(VK_CONTROL) && KeyPressed(VK_SHIFT) && KeyPressed('D')) {
                loadPosition(m_player, m_vehicle, m_lastVehicle, m_position2);
                CHud::SetHelpMessage("Load Position 2", true, false, false);
            } else if (KeyPressed(VK_SHIFT) && KeyPressed('B') && KeyPressed('A')) {
                savePosition(m_player, m_vehicle, m_position1);
                CHud::SetHelpMessage("Save Position 1", true, false, false);
            } else if (KeyPressed(VK_SHIFT) && KeyPressed('B') && KeyPressed('D')) {
                savePosition(m_player, m_vehicle, m_position2);
                CHud::SetHelpMessage("Save Position 2", true, false, false);
            } else if (KeyPressed(VK_SHIFT) && KeyPressed('C') && KeyPressed('A')) {
                CCheat::VehicleCheat(522);
                CHud::SetHelpMessage("Spawn NRG", true, false, false);
            } else if (KeyPressed(VK_SHIFT) && KeyPressed('C') && KeyPressed('D')) {
                CCheat::VehicleCheat(443);
                CHud::SetHelpMessage("Spawn Packer", true, false, false);
            } else if (KeyPressed(VK_SHIFT) && KeyPressed('C') && KeyPressed('W')) {
                CCheat::VehicleCheat(411);
                CHud::SetHelpMessage("Spawn Infernus", true, false, false);
            } else if (KeyPressed(VK_SHIFT) && KeyPressed('C') && KeyPressed('S')) {
                CCheat::VehicleCheat(432);
                CHud::SetHelpMessage("Spawn Rhino", true, false, false);
            } else if (KeyPressed(VK_SHIFT) && KeyPressed(VK_TAB)) {
                addNitroToCar();
            } else if (KeyPressed('C') && KeyPressed('V')) {
                toggleGhostTown();
            }
            else if (KeyPressed('C') && KeyPressed('W')) {
                changeWeather();
            }
            else if (KeyPressed('T') && KeyPressed('F') && KeyPressed('L')) {
                popTire(eWheels::WHEEL_FRONT_LEFT);
            }
            else if (KeyPressed('T') && KeyPressed('F') && KeyPressed('R')) {
                popTire(eWheels::WHEEL_FRONT_RIGHT);
            }
            else if (KeyPressed('T') && KeyPressed('B') && KeyPressed('L')) {
                popTire(eWheels::WHEEL_REAR_LEFT);
            }
            else if (KeyPressed('T') && KeyPressed('B') && KeyPressed('R')) {
                popTire(eWheels::WHEEL_REAR_RIGHT);
            }
            else if (KeyPressed('V') && !m_vehicle) {
                toggleJetPack();
            } else if (KeyPressed('P') && m_vehicle) {
                 setPositionedVehicle(m_vehicle); 
                 CHud::SetHelpMessage("Saved Positioned Vehicle", true, false, false);
            } else if (KeyPressed('L') && m_posVehicleStored) {
                 respawnPositionedVehicle(m_posVehicle);
                 CHud::SetHelpMessage("Repositioned Vehicle", true, false, false);
            } else {
                usedCommand = false;
            }

            if (usedCommand) {
                m_keyPressTime = CTimer::m_snTimeInMilliseconds;
            }

        }

        if (m_jetPackMode)
            processJetPack();

    }




    StuntingScript() {
        m_jetPackMode = false;
        m_ghostTown = false;
        m_keyPressTime = 0;
        m_pedDensity = 1.0f;
        m_lastVehicle = 0;



        Events::gameProcessEvent.Add(process);

    }


} stuntingScript;

int StuntingScript::m_keyPressTime;
SavedPosition StuntingScript::m_position2;
SavedPosition StuntingScript::m_position1;
CVehicle* StuntingScript::m_lastVehicle;
bool StuntingScript::m_jetPackMode;
bool StuntingScript::m_ghostTown;
CPed* StuntingScript::m_player;
CVehicle* StuntingScript::m_vehicle;
float StuntingScript::m_pedDensity;

CVector StuntingScript::m_oldPos;

unsigned StuntingScript::m_currentFrame;
float StuntingScript::m_speeds[5];

bool StuntingScript::m_posVehicleStored;
PositionedVehicle StuntingScript::m_posVehicle;

